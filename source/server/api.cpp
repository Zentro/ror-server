/*
	This file is part of Rigs of Rods Server
	Copyright 2007		Pierre-Michel Ricordel
	Copyright 2013-2022 Petr Ohlidal

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

/// @author Rafael Galvan 5/2022

#include "api.h"
#include "logger.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <json/json.h>

Api::Api()
{
	
}

static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, std::string* data)
{
	data->append((char*)ptr, size * nmemb);
	return size * nmemb;
}

void Api::GetIpv4()
{
	std::string response;
	long code = 0;

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;
}

void Api::GetIpv6()
{
	std::string response;
	long code = 0;

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, "https://api64.ipify.org");
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;
}

bool Api::PostCreateServer()
{
	Json::Value payload(Json::objectValue);

	std::string payload_str = payload.toStyledString();
	std::string response;
	std::string url = m_base_url + "/servers";
	long code = 0;

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_str.length());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, m_user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	Logger::Log(LOG_INFO, "");
	Logger::Log(LOG_DEBUG, "");

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;

	if (code != 200)
	{
		Logger::Log(LOG_ERROR, "");
		Logger::Log(LOG_DEBUG, "");
		return false;
	}

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(response.c_str(), root)) {
		Logger::Log(LOG_ERROR, "");
		Logger::Log(LOG_DEBUG, "");
		return false;
	}

	return true;
}

void Api::PutUpdateServer()
{
	std::string response;
	std::string url = m_base_url + "/servers";
	long code = 0;

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_PUT, 1);
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;
}

void Api::DeleteServer()
{
	std::string response;
	std::string url = m_base_url + "/servers";
	long code = 0;

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;
}

void Api::PutHeartbeat()
{
	std::string response;
	std::string url = m_base_url + "/servers";
	long code = 0;

	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_PUT, 1);
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);
	curl = nullptr;
}
