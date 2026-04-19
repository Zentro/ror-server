/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2008   Christopher Ritchey (aka Aperion)
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "UnicodeStrings.h"

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

enum LogLevel {
    LOG_STACK = 0,
    LOG_DEBUG,
    LOG_VERBOSE,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
};

enum LogType {
    LOGTYPE_FILE = 0,
    LOGTYPE_CONSOLE
};

namespace Logger {

    spdlog::logger* Get();

    void SetOutputFile(const std::string& filename);

    void SetLogLevel(LogType type, LogLevel level);

} // namespace Logger

#define ROR_SVR_TRACE(...)    SPDLOG_LOGGER_TRACE(Logger::Get(), __VA_ARGS__)
#define ROR_SVR_DEBUG(...)    SPDLOG_LOGGER_DEBUG(Logger::Get(), __VA_ARGS__)
#define ROR_SVR_INFO(...)     SPDLOG_LOGGER_INFO(Logger::Get(), __VA_ARGS__)
#define ROR_SVR_WARN(...)     SPDLOG_LOGGER_WARN(Logger::Get(), __VA_ARGS__)
#define ROR_SVR_ERROR(...)    SPDLOG_LOGGER_ERROR(Logger::Get(), __VA_ARGS__)
#define ROR_SVR_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(Logger::Get(), __VA_ARGS__)
