/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
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

// RoRserver.cpp : Defines the entry point for the console application.

#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"
#include "messaging.h"
#include "listener.h"
#include "master-server.h"
#include "utils.h"

#include "sha1_util.h"
#include "sha1.h"

#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
# include "windows.h"
# include "resource.h"
#else // _WIN32

# include <fcntl.h>
# include <signal.h>
# include <unistd.h>
# include <sys/types.h>
# include <pwd.h>
# include <sys/types.h>
# include <sys/stat.h>

#endif // _WIN32


static Sequencer s_sequencer;
static MasterServer::Client s_master_server;
static bool s_exit_requested = false;
#ifndef _WIN32

void handler(int signalnum) {
    if (s_exit_requested) {
        return;
    }
    s_exit_requested = true;
    // reject handler
    signal(signalnum, handler);

    bool terminate = false;

    if (signalnum == SIGINT) {
        ROR_SVR_DEBUG("got interrupt signal, terminating ...");
        terminate = true;
    } else if (signalnum == SIGTERM) {
        ROR_SVR_DEBUG("got terminate signal, terminating ...");
        terminate = true;
    } else if (signalnum == SIGHUP) {
        ROR_SVR_DEBUG("got HUP signal, terminating ...");
        terminate = true;
    } else {
        ROR_SVR_ERROR("got unkown signal: {}", signalnum);
    }

    if (terminate) {
        ROR_SVR_INFO("Stopping the server");
        if (Config::getServerMode() == SERVER_LAN) {
            s_sequencer.Close();
        } else {
            if (s_master_server.IsRegistered()) {
                s_master_server.UnRegister();
            }
            s_sequencer.Close();
        }
        exit(0);
    }
}

#endif // ! _WIN32

#ifdef _WIN32
// Reference: https://msdn.microsoft.com/en-us/library/ms686016.aspx
BOOL WINAPI WindowsConsoleHandlerRoutine(DWORD ctrl_type)
{
    switch (ctrl_type)
    {
    case CTRL_C_EVENT:
        ROR_SVR_INFO("Received `Ctrl+C` event.");
        break;
    case CTRL_BREAK_EVENT:
        ROR_SVR_INFO("Received `Ctrl+Break` event.");
        break;
    case CTRL_CLOSE_EVENT:
        ROR_SVR_INFO("Received `Close` event.");
        break;
    case CTRL_SHUTDOWN_EVENT:
        ROR_SVR_INFO("Received `System shutdown` event.");
        break;
    default:
        ROR_SVR_WARN("Received unknown console event: {}.", static_cast<unsigned long>(ctrl_type));
        return TRUE; // Means 'event handled'
    }

    if (s_master_server.IsRegistered())
    {
        ROR_SVR_INFO("Unregistering...");
        s_master_server.UnRegister();
    }
    s_sequencer.Close(); // TODO: This somehow closes (crashes?) the process on Windows, debugger doesn't intercept anything...
    ROR_SVR_INFO("Clean exit (Windows)");
    ExitProcess(0); // Recommended by MSDN, see above link.
}
#endif

#ifndef WITHOUTMAIN


int main(int argc, char *argv[]) {
    // set default verbose levels
    Logger::SetLogLevel(LOGTYPE_CONSOLE, LOG_INFO);
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

    ROR_SVR_INFO("Starting Rigs of Rods Server version {} ({})", RORNET_VERSION, __DATE__);

    // Check configuration
    ServerType server_mode = Config::getServerMode();
    if (server_mode != SERVER_LAN) {
        ROR_SVR_INFO("Starting server in INET mode");
        std::string ip_addr = Config::getIPAddr();
        if (ip_addr.empty() || (ip_addr == "0.0.0.0")) {
            ROR_SVR_WARN("No IP given, detecting...");
            if (!MasterServer::RetrievePublicIp()) {
                ROR_SVR_ERROR("Failed to auto-detect public IP, exit.");
                return -1;
            }
        }
        ROR_SVR_INFO("IP address: {}", Config::getIPAddr());

        if (Config::getServerName().empty()) {
            ROR_SVR_ERROR("Server name not specified, exit.");
            return -1;
        }
        ROR_SVR_INFO("Server name: {}", Config::getServerName());
    }

    if (!Config::checkConfig()) {
        return 1;
    }

    if (!sha1check()) {
        ROR_SVR_ERROR("sha1 malfunction!");
        return -1;
    }

    // so ready to run, then set up signal handling
#ifndef _WIN32
    signal(SIGHUP, handler);
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
#else // _WIN32
    SetConsoleCtrlHandler(WindowsConsoleHandlerRoutine, TRUE);
#endif // ! _WIN32


    Listener listener(&s_sequencer);
    if (!listener.Initialize()) {
        return -1;
    }
    s_sequencer.Initialize();

    // Listener is ready, let's register ourselves on serverlist (which will contact us back to check).
    if (server_mode != SERVER_LAN) {
        bool registered = s_master_server.Register();
        if (!registered && (server_mode == SERVER_INET)) {
            ROR_SVR_ERROR("Failed to register on serverlist. Exit");
            listener.Shutdown();
            return -1;
        } else if (!registered) // server_mode == SERVER_AUTO
        {
            ROR_SVR_WARN("Failed to register on serverlist, continuing in LAN mode");
            server_mode = SERVER_LAN;
        } else {
            ROR_SVR_INFO("Registration successful");
        }
    }

    // start the main program loop
    // if we need to communiate to the master user the notifier routine
    if (server_mode != SERVER_LAN) {
        //heartbeat
        while (!s_exit_requested) {
            Messaging::UpdateMinuteStats();
            s_sequencer.UpdateMinuteStats();

            //every minute
            Utils::SleepSeconds(Config::GetHeartbeatIntervalSec());

            ROR_SVR_DEBUG("Sending heartbeat...");
            Json::Value user_list(Json::arrayValue);
            s_sequencer.GetHeartbeatUserList(user_list);
            if (!s_master_server.SendHeatbeat(user_list)) {
                unsigned int timeout = Config::GetHeartbeatRetrySeconds();
                unsigned int max_retries = Config::GetHeartbeatRetryCount();
                ROR_SVR_WARN("A heartbeat failed! Retry in {} seconds.", timeout);
                bool success = false;
                for (unsigned int i = 0; i < max_retries; ++i) {
                    Utils::SleepSeconds(timeout);
                    success = s_master_server.SendHeatbeat(user_list);

                    const char *log_result = (success ? "successful." : "failed.");
                    if (success) {
                        ROR_SVR_INFO("Heartbeat retry {}/{} {}", i + 1, max_retries, log_result);
                        break;
                    } else {
                        ROR_SVR_ERROR("Heartbeat retry {}/{} {}", i + 1, max_retries, log_result);
                    }
                }
                if (!success) {
                    ROR_SVR_ERROR("Unable to send heartbeats, exit");
                    s_exit_requested = true;
                }
            } else {
                ROR_SVR_DEBUG("Heartbeat sent OK");
            }
        }

        if (s_master_server.IsRegistered()) {
            s_master_server.UnRegister();
        }
    } else {
        while (!s_exit_requested) {
            Messaging::UpdateMinuteStats();
            s_sequencer.UpdateMinuteStats();

            // broadcast our "i'm here" signal
            Messaging::broadcastLAN();

            // sleep a minute
            Utils::SleepSeconds(60);
        }
    }

    s_sequencer.Close();
    return 0;
}

#endif //WITHOUTMAIN

