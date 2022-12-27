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

#pragma once

#ifdef WITH_CURL

#include "prerequisites.h"

#include <string>

namespace Api {
enum ApiErrorStates_t {
	API_NO_ERROR = 1,
	API_CLIENT_ERROR,
	API_SERVER_ERROR,
	API_ERROR_UNKNOWN = -1,
};
class Client {
	struct HttpResponse {
		int status_code;
		std::string body;
		std::string headers;
	};
	struct HttpRequest {
		std::string body;
		std::string url;
		std::string headers;
		std::string method = "get";
		std::string content_type = "Content-Type: application/json";
	};
public:
	Client();
	int getIpv4();
	int getIpv6();
	ApiErrorStates_t removeFromServerList();
	bool canDoHttpRequests() { return m_http_do_requests; };
	ApiErrorStates_t addToServerList();
	ApiErrorStates_t updateToServerList();
	ApiErrorStates_t getVersionCheck();
private:
	ApiErrorStates_t handleHttpRequestErrors(HttpResponse &response);
	bool hasErrors(int status_code);
	HttpResponse apiHttpQuery(HttpRequest &request);
	std::string m_http_user_agent;
	bool m_http_do_requests = true;
	bool m_added_to_serverlist = false;
};
} // namespace Api

#endif // WITH_CURL