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
    \file    http_client.h
    \brief   HTTP Client class definition.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#pragma once

#include <string>
#include <map>
#include <memory>
#include <curl/curl.h>
#include <chrono>
#include <mutex>


/**
    \brief Enumeration of HTTP methods.
*/
enum class HttpMethod
{
    GET,
    POST,
    UPDATE,
    DELETE,
    PUT
};

/**
    \brief Structure representing an HTTP response.
*/
struct HttpResponse
{
    long status_code;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /**
        \brief Add a header to the HTTP request.
        \param name The name of the header.
        \param value The value of the header.
     */
    void AddHeader(const std::string& name, const std::string& value);

    /**
        \brief Remove a header from the HTTP request.
        \param name The name of the header to remove.
    */
    void RemoveHeader(const std::string& name);

    /**
        \brief Clear all headers from the HTTP request.
    */
    void ClearHeaders();

    /**
        \brief Perform an HTTP GET request.
        \param url The URL to send the request to.
        \param headers Optional headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse Get(const std::string& url,
                     const std::map<std::string, std::string>& headers = {});

    /**
        \brief Perform an HTTP POST request.
        \param url The URL to send the request to.
        \param body The body of the POST request.
        \param headers Optional headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse Post(const std::string& url,
                      const std::string& body,
                      const std::map<std::string, std::string>& headers = {});

    /**
        \brief Perform an HTTP UPDATE request.
        \param url The URL to send the request to.
        \param body The body of the UPDATE request.
        \param headers Optional headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse Update(const std::string& url,
                        const std::string& body,
                        const std::map<std::string, std::string>& headers = {});

    /**
        \brief Perform an HTTP DELETE request.
        \param url The URL to send the request to.
        \param headers Optional headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse Delete(const std::string& url,
                        const std::map<std::string, std::string>& headers = {});

    /**
        \brief Perform an HTTP PUT request without a body.
        \param url The URL to send the request to.
        \param body The body of the PUT request.
        \param headers Optional headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse Put(const std::string& url,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {});

private:
    CURL                                *m_curl;               // <! CURL handle
    long                                m_timeout_ms;          // <! Timeout in milliseconds
    int                                 m_max_redirects;       // <! Maximum number of redirects
    int                                 m_initial_backoff_ms;  // <! Initial backoff time in milliseconds
    int                                 m_max_backoff_ms;      // <! Maximum backoff time in milliseconds
    double                              m_backoff_multiplier;  // <! Backoff multiplier
    std::map<std::string, std::string>  m_headers;             // <! Headers to include in the request
    mutable std::mutex                  m_headers_mutex;       // <! Mutex to protect header access

    /**
        \brief Perform an HTTP request with retry logic.
        \param method The HTTP method to use.
        \param url The URL to send the request to.
        \param body The body of the request.
        \param headers Headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse RequestOnce(HttpMethod method,
                             const std::string& url,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers);

    /**
        \brief Perform an HTTP request with retry logic.
        \param method The HTTP method to use.
        \param url The URL to send the request to.
        \param body The body of the request.
        \param headers Headers to include in the request.
        \return The HTTP response.
    */
    HttpResponse RequestWithRetry(HttpMethod method,
                                  const std::string& url,
                                  const std::string& body,
                                  const std::map<std::string, std::string>& headers);

    /**
        \brief Determine if a request should be retried based on the status code.
        \param status_code The HTTP status code from the response.
        \return True if the request should be retried, false otherwise.
    */
    bool ShouldRetry(long status_code);

    /**
        \brief Write callback function for CURL.
        \param contents The data received from the server.
        \param size The size of each element.
        \param nmemb The number of elements.
        \param userp User-defined data pointer.
        \return The number of bytes written.
     */
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /**
        \brief Header callback function for CURL.
        \param buffer The header data received from the server.
        \param size The size of each element.
        \param nitems The number of elements.
        \param userdata User-defined data pointer.
        \return The number of bytes processed.
    */
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
};
