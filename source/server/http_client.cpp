/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2025 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/**
    \file    http_client.cpp
    \brief   HTTP Client class implementation.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#include "http_client.h"

#include <thread>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cmath>

#include <curl/curl.h>

HttpClient::HttpClient()
    : m_curl(curl_easy_init()),
      m_timeout_ms(5000),
      m_max_redirects(5),
      m_initial_backoff_ms(200),
      m_max_backoff_ms(10000),
      m_backoff_multiplier(2.0)
{
    // curl_global_init(CURL_GLOBAL_DEFAULT); TODO: is this needed here?
    m_curl = curl_easy_init();
}

HttpClient::~HttpClient()
{
    if (m_curl)
    {
        curl_easy_cleanup(m_curl);
    }
    curl_global_cleanup();
}

void HttpClient::AddHeader(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(m_headers_mutex);
    m_headers[key] = value;
}

void HttpClient::RemoveHeader(const std::string &key)
{
    std::lock_guard<std::mutex> lock(m_headers_mutex);
    m_headers.erase(key);
}

void HttpClient::ClearHeaders()
{
    std::lock_guard<std::mutex> lock(m_headers_mutex);
    m_headers.clear();
}

HttpResponse HttpClient::Get(const std::string &url,
                             const std::map<std::string, std::string> &headers)
{
    return RequestWithRetry(HttpMethod::GET, url, "", headers);
}

HttpResponse HttpClient::Post(const std::string &url,
                              const std::string &body,
                              const std::map<std::string, std::string> &headers)
{
    return RequestWithRetry(HttpMethod::POST, url, body, headers);
}

HttpResponse HttpClient::Update(const std::string &url,
                                const std::string &body,
                                const std::map<std::string, std::string> &headers)
{
    return RequestWithRetry(HttpMethod::UPDATE, url, body, headers);
}

HttpResponse HttpClient::Delete(const std::string &url,
                                const std::map<std::string, std::string> &headers)
{
    return RequestWithRetry(HttpMethod::DELETE, url, "", headers);
}

HttpResponse HttpClient::Put(const std::string &url,
                             const std::string &body,
                             const std::map<std::string, std::string> &headers)
{
    return RequestWithRetry(HttpMethod::PUT, url, body, headers);
}

HttpResponse HttpClient::RequestOnce(HttpMethod method,
                                     const std::string &url,
                                     const std::string &body,
                                     const std::map<std::string, std::string> &headers)
{
    HttpResponse response;
    response.status_code = 0;
    std::string response_body;

    if (!m_curl)
    {
        response.status_code = -1;
        return response;
    }

    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, m_timeout_ms);

    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, this->WriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, this->HeaderCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &response.headers);

    switch (method)
    {
    case HttpMethod::GET:
        curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
        break;
    case HttpMethod::POST:
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, body.size());
        break;
    case HttpMethod::UPDATE:
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, body.size());
        break;
    case HttpMethod::DELETE:
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    }

    struct curl_slist *header_list = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_headers_mutex);
        for (const auto &[key, value] : m_headers)
        {
            std::string header = key + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }
    }
    if (header_list)
    {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode status = curl_easy_perform(m_curl);

    if (header_list)
    {
        curl_slist_free_all(header_list);
    }

    if (status != CURLE_OK)
    {
        response.status_code = -1;
        return response;
    }

    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = response_body;

    return response;
}

HttpResponse HttpClient::RequestWithRetry(HttpMethod method,
                                          const std::string &url,
                                          const std::string &body,
                                          const std::map<std::string, std::string> &headers)
{
    HttpResponse response;

    for (int attempt = 0; attempt <= m_max_redirects; ++attempt)
    {
        response = RequestOnce(method, url, body, headers);

        if (response.status_code >= 200 && response.status_code < 300)
        {
            return response;
        }

        // If the response indicates a server error, we implement
        // an exponential backoff until we hit the max number of retries.

        if (attempt < m_max_redirects && this->ShouldRetry(response.status_code))
        {
            int backoff_time = std::min(
                m_initial_backoff_ms * static_cast<int>(std::pow(m_backoff_multiplier, attempt)),
                m_max_backoff_ms);

            // Add some jitter to avoid the thundering herd problem (~+/- 20% jitter).

            int jitter = (rand() % 40 - 20) * backoff_time / 100;
            backoff_time += jitter;

            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_time));
        }
        else
        {
            break;
        }
    }
}

bool HttpClient::ShouldRetry(long status_code)
{
    return status_code >= 500 && status_code < 600;
}

size_t HttpClient::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    std::string *response_body = static_cast<std::string *>(userp);
    response_body->append(static_cast<char *>(contents), total_size);
    return total_size;
}

size_t HttpClient::HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    size_t total_size = size * nitems;
    std::string header(buffer, total_size);

    auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);

    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos)
    {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[key] = value;
    }

    return total_size;
}
