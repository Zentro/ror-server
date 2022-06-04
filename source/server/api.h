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

#pragma once

#include "prerequisites.h"

#include <string>

class Api {
public:
	Api();

	bool				GetIpv4();
	bool				GetIpv6();
	bool				PostCreateServer(std::string payload);
	bool				PutUpdateServer(std::string payload);
	void				DeleteServer();
	void				PutHeartbeat();
private:
	std::string			m_base_url;
	std::string			m_api_key;
	std::string			m_user_agent;
};
