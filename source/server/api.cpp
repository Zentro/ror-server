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
#include "api.h"
#include "logger.h"
#include "config.h"
#include "utils.h"
#include "json/json.h"

#include <rornet.h>

#include <curl/curl.h>
#include <curl/easy.h>


ApiClient::ApiClient(const std::string& baseUrl, const std::string& apiKey)
    : apiKey(apiKey)
    , baseUrl(baseUrl) {
}

bool ApiClient::getPublicIp(std::string &ip)
{
    HttpResponse response;
    ApiErrorState error_code;

    HttpRequest request(HttpMethod::GET, "/ip");

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    if (error_code == API_NO_ERROR)
    {
        ip = response.body;
    }

    return (error_code != API_NO_ERROR);
}

bool ApiClient::isCallable() const
{
    HttpResponse response;
    ApiErrorState error_code;

    HttpRequest request(HttpMethod::GET, "/");

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return (error_code != API_NO_ERROR);
}

bool ApiClient::isAuthenticated() const
{
    return true;
}

ApiErrorState ApiClient::registerServer()
{
    HttpResponse response;
    ApiErrorState error_code;

    Json::Value data(Json::objectValue);
    data["name"] = Config::getServerName();
    data["ip"] = Config::Server::ip();
    data["port"] = Config::getListenPort();
    data["version"] = RORNET_VERSION;
    data["description"] = "This is temp";
    data["max_clients"] = Config::getMaxClients();
    data["has_password"] = Config::isPublic();

    HttpRequest request(HttpMethod::POST, "/servers", data.asString());

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return error_code;
}

ApiErrorState ApiClient::updateServer()
{
    HttpResponse response;
    ApiErrorState error_code;

    char url[300] = "";
    sprintf(url, "/servers/%d", 10000);

    Json::Value data(Json::objectValue);

    HttpRequest request(HttpMethod::UPDATE, url, data.asString());

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return error_code;
}

ApiErrorState ApiClient::syncServer()
{
    HttpResponse response;
    ApiErrorState error_code;

    Json::Value data(Json::objectValue);

    HttpRequest request(HttpMethod::PATCH, "/servers", data.asString());

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return error_code;
}

ApiErrorState ApiClient::syncServerPowerState(std::string status)
{
    HttpResponse response;
    ApiErrorState error_code;

    Json::Value data(Json::objectValue);
    data["power_status"] = status;

    HttpRequest request(HttpMethod::UPDATE, "/servers", data.asString());


    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return error_code;
}

ApiErrorState ApiClient::verifyClientSession(std::string challenge)
{
    HttpResponse response;
    ApiErrorState error_code;

    // Need to maybe look into whether or not C++20 has better string formatting?
    char url[300] = "";
    sprintf(url, "/auth/sessions/%s/verify", "ee1b920c-f815-4c9e-b5a2-b60db71dba88");

    // We don't actually know what is in the claims of the challenge, so
    // we'll wait for the API to return a pass or fail on them.
    Json::Value data(Json::objectValue);
    data["challenge"] = challenge.c_str(); // TODO: I really don't appreciate how funky this is ...
    HttpRequest request(HttpMethod::GET, url, data.toStyledString());

    response = this->executeHttpQuery(request);
    error_code = this->handleHttpRequestErrors(response);

    return error_code;
}

ApiClient::HttpResponse ApiClient::executeHttpQuery(HttpRequest &request) const
{
    HttpResponse response;
    CURLcode curl_result;

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpMethodToString(request.method));
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    if (!m_api_key_key.empty())
    {
        request.headers.push_back("Authorization: Bearer " + m_api_key_key);
    }

    /**
     * Set the content type to application/json.
     */

    request.headers.push_back("Accept: application/json");

    struct curl_slist *headers = nullptr;
    for (const auto &header : request.headers)
    {
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, request.user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStringWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    curl_result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    if (curl_result != CURLE_OK)
    {
        response.status_code = 500;
    }

    return response;
}

ApiErrorState ApiClient::handleHttpRequestErrors(HttpResponse &response)
{
    ApiErrorState error_code;

    if (!this->hasError(response.status_code))
    {
        error_code = API_NO_ERROR;
    }

    if (response.status_code >= 400 && response.status_code < 500)
    {
        error_code = API_CLIENT_ERROR;
    }

    else if (response.status_code >= 500)
    {
        error_code = API_SERVER_ERROR;
    }

    return error_code;
}

bool ApiClient::hasError(int status_code)
{
    return status_code >= 300 || status_code < 200;
}

/**
 * \brief Returns a string representation of an HTTP method.
 *
 * \param HttpMethod The HTTP method.
 * \return The HTTP method as a string.
 */
const char *ApiClient::httpMethodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::GET:
        return "GET";
    case HttpMethod::DELETE:
        return "DELETE";
    case HttpMethod::POST:
        return "POST";
    case HttpMethod::PUT:
        return "PUT";
    default:
        return "UNKNOWN";
    }
}
