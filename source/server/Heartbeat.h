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
    \file    Heartbeat.h
    \brief   Heartbeat class implementation.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#pragma once

#include "ApiClient.h"
#include "sequencer.h"

class Heartbeat
{
public:
    /**
        \brief Constructor.
        \param api_client Reference to the API client.
        \param sequencer Reference to the sequencer.
    */
    explicit Heartbeat(
        ApiClient &api_client,
        Sequencer &sequencer,
    );
    ~Heartbeat();

    Heartbeat(const Heartbeat&) = delete;
    Heartbeat& operator=(const Heartbeat&) = delete;
    Heartbeat(Heartbeat&&) = delete;
    Heartbeat& operator=(Heartbeat&&) = delete;

    /**
        \brief Start the heartbeat thread.
    */
    void Start();

    /**
        \brief Stop the heartbeat thread.
    */
    void Stop();

    /**
        \brief Check if the heartbeat thread is running.
        \return True if running, false otherwise.
    */
    bool IsRunning();

    /**
        \brief Get the time of the last heartbeat.
        \return Time of the last heartbeat.
    */
    std::chrono::system_clock::time_point GetLastHeartbeatTime() const;

private:
    ApiClient                               &m_api_client;          // <! API client
    Sequencer                               &m_sequencer;           // <! Sequencer
    std::thread                             m_heartbeat_thread;     // <! Thread for sending heartbeats
    std::atomic<bool>                       m_running{false};       // <! Flag to control the heartbeat thread
    mutable std::mutex                      m_time_mutex;           // <! Timer mutex
    std::chrono::system_clock::time_point   m_last_heartbeat_time;  // <! Time of the last heartbeat
    int                                     m_interval_seconds;     // <! Interval of heartbeat
    std::condition_variable                 m_cv;                   // <! Conditional variable
    std::mutex                              m_cv_mutex;             // <! CV mutex

    /**
        \brief Worker thread function.
    */
    void WorkerThread();

    /**
        \brief Send a heartbeat to the API.
    */
    void SendHeartbeat();
};