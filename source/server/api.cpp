/*
	This file is part of Rigs of Rods Server
	Copyright 2007		Pierre-Michel Ricordel
	Copyright 2013-2022 Petr Ohlidal & contributors

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/// @author Rafael Galvan
/// A lean and fullproof abstraction layer with cURL
#ifdef WITH_CURL

#include "api.h"
#include "logger.h"
#include "rornet.h"

//#include <curl/curl.h>
//#include <curl/easy.h>

namespace Api {

	Client::Client() {}

	int Client::getIpv4()
	{
		return 1;
	}

	int Client::getIpv6()
	{
		return 1;
	}

	ApiErrorStates_t Client::getVersionCheck()
	{
		HttpRequest request;
		HttpResponse response;

		request.url = "/version";
		request.method = "get";
		request.body = "";

		response = this->apiHttpQuery(request);

		return this->handleHttpRequestErrors(response);
	}

	ApiErrorStates_t Client::removeFromServerList()
	{
		HttpRequest request;
		HttpResponse response;

		response = this->apiHttpQuery(request);

		return this->handleHttpRequestErrors(response);
	}

	ApiErrorStates_t Client::updateToServerList()
	{
		HttpRequest request;
		HttpResponse response;

		response = this->apiHttpQuery(request);

		return this->handleHttpRequestErrors(response);
	}

	ApiErrorStates_t Client::addToServerList() {
		HttpRequest request;
		HttpResponse response;

		response = this->apiHttpQuery(request);

		return this->handleHttpRequestErrors(response);
	}

	Client::HttpResponse Client::apiHttpQuery(HttpRequest &request) {
		HttpResponse response;
		//CURL* curl = curl_easy_init();

		//curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
		//curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#ifdef _WIN32
		//curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
		//curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
		//write_callback
		//curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

		//curl_easy_perform(curl);

		//curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

		//curl_easy_cleanup(curl);
		//curl = nullptr;

		return response;
	}

	ApiErrorStates_t Client::handleHttpRequestErrors(HttpResponse &response) {
		if (!this->hasErrors(response.status_code)) {
			return API_NO_ERROR;
		}

		if (response.status_code >= 400 && response.status_code < 500) {
			return API_CLIENT_ERROR;
		}

		else if (response.status_code >= 500) {
			return API_SERVER_ERROR;
		}

		return API_ERROR_UNKNOWN;
	}

	bool Client::hasErrors(int status_code) {
		return status_code >= 300 || status_code < 200;
	}
} // namespace Api

#endif // WITH_CURL