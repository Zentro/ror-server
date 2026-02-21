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
    \file    ApiClient.cpp
    \brief   API Client class implementation.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#include "api_client.h"
#include "config.h"
#include "json/json.h"

ApiClient::ApiClient(const std::string &base_url,
                     const std::string &api_key)
    : m_base_url(base_url),
      m_api_key(api_key),
      m_http_client(new HttpClient())
{
    m_http_client->AddHeader("Authorization", "Bearer " + m_api_key);
    m_http_client->AddHeader("Content-Type", "application/json");
    m_http_client->AddHeader("Accept", "application/json");
}

ApiClient::~ApiClient()
{
}

ApiErrorState ApiClient::GetErrorStateFromHttpCode(long http_code)
{
    if (http_code >= 200 && http_code < 300) {
        return ApiErrorState::API_NO_ERROR;
    } else if (http_code >= 400 && http_code < 500) {
        return ApiErrorState::API_CLIENT_ERROR;
    } else if (http_code >= 500) {
        return ApiErrorState::API_SERVER_ERROR;
    } else {
        return ApiErrorState::API_UNKNOWN_ERROR;
    }
}

ApiResponse ApiClient::CreateServer()
{
    std::string url = m_base_url + "/servers";

    Json::Value payload;
    payload["name"] = Config::getServerName();
    payload["terrain"] = Config::getTerrainName();
    payload["port"] = Config::getListenPort();
    payload["max_clients"] = Config::getMaxClients();
    payload["ip"] = Config::getIPAddr();
    payload["password"] = !Config::getPublicPassword().empty();

    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, payload);

    auto response = m_http_client->Post(url, body);

    ApiResponse api_response;
    api_response.error_state = this->GetErrorStateFromHttpCode(response.status_code);
    api_response.message = response.body;
    return api_response;
}

ApiResponse ApiClient::UpdateServerStatus(const std::string& status)
{
    std::string url = m_base_url + "/servers/status";

    Json::Value payload;
    payload["status"] = status;

    Json::StreamWriterBuilder builder;
    std::string body = Json::writeString(builder, payload);

    auto response = m_http_client->Put(url, body);

    ApiResponse api_response;
    api_response.error_state = this->GetErrorStateFromHttpCode(response.status_code);
    api_response.message = response.body;
    return api_response;
}

ApiResponse ApiClient::DeleteServer()
{
    std::string url = m_base_url + "/servers";

    auto response = m_http_client->Delete(url);

    ApiResponse api_response;
    api_response.error_state = this->GetErrorStateFromHttpCode(response.status_code);
    api_response.message = response.body;
    return api_response;
}

ApiResponse ApiClient::PutHeartbeat(const std::string& data)
{
    std::string url = m_base_url + "/heartbeat";
    Json::StreamWriterBuilder builder;
    std::string payload = Json::writeString(builder, data);

    auto response = m_http_client->Put(url, payload);

    ApiResponse api_response;
    api_response.error_state = this->GetErrorStateFromHttpCode(response.status_code);
    api_response.message = response.body;

    return api_response;
}
