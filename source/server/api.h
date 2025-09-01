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
 * \file api.h
 * \brief API client
 * \author Rafael Galvan
 * \date 2025-07-15
 */

#include <string>
#include <vector>

#include <rornet.h>

/**
 * \brief Enum representing different API error states
 */
enum ApiErrorState
{
    API_NO_ERROR = 0,
    API_CLIENT_ERROR = 1,
    API_SERVER_ERROR = 2,
    API_UNKNOWN_ERROR = 999,
};

/**
 * \brief Enum representing different API states
 */
enum ApiState
{
    API_STATE_NOOP = 0,
    API_STATE_OK = 1,
    API_STATE_ERROR = 2,
};

/**
 * \class ApiClient
 * \brief Provides an interface to interact with the API.
 * 
 * Example usage:
 * \code
 * ApiClient apiClient("http://127.0.0.1/api", "your_api_key");
 * apiClient.RegisterServer();
 * apiClient.UpdateServer();
 * apiClient.SyncServer();
 * \endcode
 */
class ApiClient {
public:
    /**
     * \brief Enum representing different HTTP methods
     */
    enum HttpMethod
    {
        GET,
        POST,
        PUT,
        DELETE,
        PATCH,
        UPDATE
    };

    /**
     * \brief Sructure representing an HTTP response
     */
    struct HttpResponse
    {
        int status_code;
        std::string body;
        std::string headers;
    };

    /**
     * \brief Structure representing an HTTP request
     */
    struct HttpRequest
    {
        HttpMethod method = HttpMethod::GET;
        std::string url;
        std::string body;
        std::vector<std::string> headers;
        std::string content_type;
        std::string user_agent;

        HttpRequest(HttpMethod method,
                    const std::string &uri,
                    const std::string &body = "",
                    const std::vector<std::string> &headers = {},
                    const std::string &content_type = "Content-Type: application/json",
                    const std::string &user_agent = std::string("Rigs of Rods Server/") + RORNET_VERSION)
            : method(method),
                url("http://127.0.0.1:8080" + uri),
                body(body),
                headers(headers),
                content_type(content_type),
                user_agent(user_agent)
        {
        }
    };

public:
    /**
     * \brief Default constructor
     */
    ApiClient() = default;

    /**
     * \brief Constructor with API key and base URL
     * \param baseUrl Base URL for the API
     * \param apiKey API key for authentication
     */
    explicit ApiClient(const std::string& baseUrl, const std::string& apiKey = "");

    /**
     * \brief Destructor
     */
    ~ApiClient() = default;

    // Don't copy the client while we hold onto API credentials.
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    // Allow move semantics.
    ApiClient(ApiClient&&) = default;
    ApiClient& operator=(ApiClient&&) = default;

    /**
     * \brief Get the current API state
     * \return Current API state
     * \see ApiState
     */
    ApiState getState() const noexcept { return apiState; }

    /**
     * \brief Get public IP address
     * \param ip Reference to store the IP address
     * \return True if successful, false otherwise
     */
    bool getPublicIp(std::string& ip);

    /**
     * \brief Check if API is callable
     * \return True if API is callable, false otherwise
     */
    bool isCallable() const;

    /**
     * \brief Check if client is authenticated
     * \return True if authenticated, false otherwise
     */
    bool isAuthenticated() const;

    /**
     * @brief Register server with the API
     * @return ErrorState indicating success or failure
     */
    ApiErrorState registerServer();

    /**
     * @brief Update server information
     * @return ErrorState indicating success or failure
     */
    ApiErrorState updateServer();

    /**
     * @brief Synchronize server data
     * @return ErrorState indicating success or failure
     */
    ApiErrorState syncServer();

    /**
     * @brief Synchronize server power state
     * @param status Power status string
     * @return ErrorState indicating success or failure
     */
    ApiErrorState syncServerPowerState(const std::string& status);

    /**
     * @brief Create a new client session
     * @return ErrorState indicating success or failure
     */
    ApiErrorState createClient();

    /**
     * @brief Verify client session with challenge
     * @param challenge Challenge string for verification
     * @return ErrorState indicating success or failure
     */
    ApiErrorState verifyClientSession(const std::string& challenge);

    /**
     * @brief Set API key for authentication
     * @param key The API key to use
     */
    void setApiKey(const std::string& key) { apiKey = key; }

    /**
     * @brief Set base URL for API requests
     * @param url The base URL to use
     */
    void setBaseUrl(const std::string& url) { baseUrl = url; }

private:
    /**
     * @brief Handle HTTP request errors
     * @param response HTTP response to analyze
     * @return ErrorState based on response analysis
     */
    ApiErrorState handleHttpRequestErrors(const HttpResponse& response);

    /**
     * @brief Check if status code indicates an error
     * @param code HTTP status code to check
     * @return True if error, false otherwise
     */
    static bool hasError(int code) noexcept;

    /**
     * @brief Execute HTTP query
     * @param request HTTP request to execute
     * @return HTTP response
     */
    HttpResponse executeHttpQuery(const HttpRequest& request);

    /**
     * @brief Convert HTTP method enum to string
     * @param method HTTP method enum
     * @return String representation of HTTP method
     */
    static const char* httpMethodToString(HttpMethod method) noexcept;

    /**
     * @brief Update internal state based on operation result
     * @param state Updated state
     */
    void updateState(ApiState state) noexcept;

private:
    std::string apiKey;
    std::string baseUrl;
    ApiState apiState;
};