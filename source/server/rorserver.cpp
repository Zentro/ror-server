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
#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"
#include "messaging.h"
#include "listener.h"
#include "heartbeat.h"
#include "api_client.h"
#include "utils.h"

#include "sha1_util.h"
#include "sha1.h"

#include <atomic>
#include <thread>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
# include "windows.h"
# include "resource.h"
#endif // _WIN32

static Sequencer s_sequencer;
static std::atomic<bool> s_exit_requested{false};

#ifndef _WIN32
void handler(int signalnum) {
    s_exit_requested.store(true);
}
#else // _WIN32
BOOL WINAPI WindowsConsoleHandlerRoutine(DWORD ctrl_type)
{
    s_exit_requested.store(true);
    return TRUE;
}
#endif // _WIN32

#ifndef WITHOUTMAIN

int main(int argc, char *argv[]) {
    // set default verbose levels
    Logger::SetLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
    Logger::SetLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
    Logger::SetOutputFile("server.log");

    if (!Config::ProcessArgs(argc, argv)) {
        return -1;
    }
    if (Config::GetShowHelp()) {
        Config::ShowHelp();
        return 0;
    }
    if (Config::GetShowVersion()) {
        Config::ShowVersion();
        return 0;
    }

    // Check configuration
    ServerType server_mode = Config::getServerMode();
    if (server_mode != SERVER_LAN) {
        Logger::Log(LOG_INFO, "Starting server in INET mode");
    }

    if (!Config::checkConfig()) {
        return 1;
    }

    if (!sha1check()) {
        Logger::Log(LOG_ERROR, "SHA1 self-test failed. Exit.");
        return -1;
    }

    // Set up signal handling
#ifndef _WIN32
    signal(SIGHUP, handler);
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
#else // _WIN32
    SetConsoleCtrlHandler(WindowsConsoleHandlerRoutine, TRUE);
#endif // _WIN32

    Listener listener(&s_sequencer);
    if (!listener.Initialize()) {
        return -1;
    }
    s_sequencer.Initialize();

    // Validate API key for INET mode
    std::string api_key = Config::GetApiKeyKey();
    if (server_mode != SERVER_LAN && api_key.empty())
    {
        if (server_mode == SERVER_INET)
        {
            Logger::Log(LOG_ERROR, "API key was not set or is missing from the config file");
            listener.Shutdown();
            return -1;
        }
        Logger::Log(LOG_WARN, "API key was not set or is missing from the config file, continuing in LAN mode");
        server_mode = SERVER_LAN;
    }

    // Register with the API if in INET mode
    std::unique_ptr<ApiClient> api_client;
    if (server_mode != SERVER_LAN)
    {
        api_client = std::unique_ptr<ApiClient>(new ApiClient(Config::GetApiHost(), api_key));
        ApiResponse resp = api_client->CreateServer();
        if (resp.error_state != ApiErrorState::API_NO_ERROR)
        {
            if (server_mode == SERVER_INET)
            {
                Logger::Log(LOG_ERROR, "Failed to register with the API, error code: %d", static_cast<int>(resp.error_state));
                listener.Shutdown();
                return -1;
            }

            Logger::Log(LOG_WARN, "Failed to register with the API, continuing in LAN mode.");
            api_client.reset();
            server_mode = SERVER_LAN;
        }
        else
        {
            Logger::Log(LOG_INFO, "Server is registered with the API");
        }
    }

    // Main loop
    if (server_mode != SERVER_LAN) {
        Heartbeat heartbeat(*api_client, s_sequencer);
        heartbeat.Start();

        while (!s_exit_requested.load()) {
            Messaging::UpdateMinuteStats();
            s_sequencer.UpdateMinuteStats();
            Utils::SleepSeconds(60);
        }

        heartbeat.Stop();
    } else {
        while (!s_exit_requested.load()) {
            Messaging::UpdateMinuteStats();
            s_sequencer.UpdateMinuteStats();

            Messaging::broadcastLAN();

            Utils::SleepSeconds(60);
        }
    }

    Logger::Log(LOG_INFO, "Shutdown requested, exiting...");

    // Cleanup
    if (api_client)
    {
        Logger::Log(LOG_INFO, "Unregistering from the API...");
        auto response = api_client->DeleteServer();
        if (response.error_state != ApiErrorState::API_NO_ERROR)
        {
            Logger::Log(LOG_ERROR, "Failed to unregister from the API, error code: %d", static_cast<int>(response.error_state));
        }
    }

    s_sequencer.Close();
    listener.Shutdown();

    Logger::Log(LOG_INFO, "Server shutdown complete");
    return 0;
}

#endif //WITHOUTMAIN
