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

#include "logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr const char* kLoggerName = "rorserver";
constexpr const char* kLogPattern = "[%Y-%m-%d %H:%M:%S] [%l] [tid %t] [%s:%# %!] %v";
constexpr std::size_t kRotatingMaxFileSize = 5 * 1024 * 1024; // 5 MiB
constexpr std::size_t kRotatingMaxFiles = 5;

std::mutex g_init_mutex;

spdlog::level::level_enum ToSpdLevel(LogLevel level) {
    switch (level) {
        case LOG_STACK:   return spdlog::level::trace;
        case LOG_DEBUG:   return spdlog::level::debug;
        case LOG_VERBOSE: return spdlog::level::debug;
        case LOG_INFO:    return spdlog::level::info;
        case LOG_WARN:    return spdlog::level::warn;
        case LOG_ERROR:   return spdlog::level::err;
        case LOG_NONE:    return spdlog::level::off;
    }
    return spdlog::level::info;
}

std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> g_console_sink;
std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> g_file_sink;
std::shared_ptr<spdlog::logger> g_logger;

std::shared_ptr<spdlog::logger> BuildLogger() {
    std::vector<spdlog::sink_ptr> sinks;
    if (!g_console_sink) {
        g_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        g_console_sink->set_level(spdlog::level::info);
    }
    sinks.push_back(g_console_sink);
    if (g_file_sink) {
        sinks.push_back(g_file_sink);
    }

    auto logger = std::make_shared<spdlog::logger>(kLoggerName, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);
    logger->set_pattern(kLogPattern);
    return logger;
}

void EnsureInitialized() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (!g_logger) {
        g_logger = BuildLogger();
        spdlog::register_logger(g_logger);
    }
}

} // namespace

namespace Logger {

    spdlog::logger* Get() {
        if (!g_logger) {
            EnsureInitialized();
        }
        return g_logger.get();
    }

    void SetOutputFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(g_init_mutex);
        g_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filename, kRotatingMaxFileSize, kRotatingMaxFiles);
        g_file_sink->set_level(spdlog::level::debug);

        if (g_logger) {
            spdlog::drop(kLoggerName);
        }
        g_logger = BuildLogger();
        spdlog::register_logger(g_logger);
    }

    void SetLogLevel(LogType type, LogLevel level) {
        EnsureInitialized();
        const auto spd_level = ToSpdLevel(level);
        if (type == LOGTYPE_CONSOLE && g_console_sink) {
            g_console_sink->set_level(spd_level);
        } else if (type == LOGTYPE_FILE && g_file_sink) {
            g_file_sink->set_level(spd_level);
        }
    }

} // namespace Logger
