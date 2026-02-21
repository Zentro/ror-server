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
    \file    ApiClient.h
    \brief   API Client class definition.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#pragma once

#include "http_client.h"

#include <string>

/**
    \brief Enum representing different API error states
*/
enum class ApiErrorState
{
    API_NO_ERROR        = 0,    // <! No error (yay!)
    API_CLIENT_ERROR    = 1,    // <! Client side error (e.g., bad request)
    API_SERVER_ERROR    = 2,    // <! Server side error (e.g., internal server error)
    API_UNKNOWN_ERROR   = 999,  // <! Unknown error (cURL?)
};

/**
    \brief Structure representing an API response
*/
struct ApiResponse
{
    ApiErrorState  error_state;  // <! Error state of the API response
    std::string    message;      // <! Message from the API response
};

class ApiClient
{
public:
    /**
        \brief Constructor for ApiClient
        \param base_url The base URL for the API
        \param api_key The API key for authentication
    */
    ApiClient(const std::string& base_url,
              const std::string& api_key);
    ~ApiClient();

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    /**
        \brief Create a server entry in the API
        \return The API response
     */
    ApiResponse CreateServer();

    /** 
        \brief Update the server power status
        \param status The new status for the server
        \return The API response
     */
    ApiResponse UpdateServerStatus(const std::string& status);

    /**
        \brief Delete the server entry in the API
        \return The API response
     */
    ApiResponse DeleteServer();

    /**
        \brief Send a heartbeat request to the API
        \return The API response
     */
    ApiResponse PutHeartbeat(const std::string& data);
private:
    std::string                             m_base_url;             // <! Base URL for the API
    std::string                             m_api_key;              // <! API key for authentication
    std::unique_ptr<HttpClient>             m_http_client;          // <! HTTP client for making requests

    /**
        \brief Helper function to convert HTTP status codes to ApiErrorState
        \param http_code The HTTP status code to convert
        \return The corresponding ApiErrorState
     */
    ApiErrorState GetErrorStateFromHttpCode(long http_code);
};