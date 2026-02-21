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
    \file    heartbeat.cpp
    \brief   Heartbeat class implementation.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#include "heartbeat.h"
#include "api_client.h"
#include "config.h"
#include "logger.h"
#include "json/json.h"

Heartbeat::Heartbeat(ApiClient& api_client, Sequencer& sequencer)
    : m_api_client(&api_client),
      m_sequencer(&sequencer),
      m_running(false),
      m_interval_seconds(Config::GetHeartbeatIntervalSec())
{
}

Heartbeat::~Heartbeat()
{
    this->Stop();
}

void Heartbeat::Start()
{
    if (m_running.load())
    {
        return;
    }

    m_running.store(true);
    m_heartbeat_thread = std::thread(&Heartbeat::ThreadMain, this);
}

void Heartbeat::Stop()
{
    Logger::Log(LOG_DEBUG, "Stopping heartbeat thread");
    if (!m_running.load())
    {
        return;
    }

    m_running.store(false);
    m_cv.notify_all();
    if (m_heartbeat_thread.joinable())
    {
        m_heartbeat_thread.join();
    }
}

bool Heartbeat::IsRunning()
{
    return m_running.load(std::memory_order_acquire);
}

std::chrono::system_clock::time_point Heartbeat::GetLastHeartbeatTime() const
{
    std::lock_guard<std::mutex> lock(m_time_mutex);
    return m_last_heartbeat_time;
}

void Heartbeat::ThreadMain()
{
    Logger::Log(LOG_DEBUG, "Heartbeat thread started");
    std::unique_lock<std::mutex> lock(m_cv_mutex);

    while (m_running.load(std::memory_order_acquire))
    {
        m_cv.wait_for(
            lock,
            std::chrono::seconds(m_interval_seconds),
            [this] { return !m_running.load(std::memory_order_acquire); }
        );

        if (!m_running.load(std::memory_order_acquire))
        {
            break;
        }

        lock.unlock();
        this->SendHeartbeat();
        lock.lock();
    }
}

void Heartbeat::SendHeartbeat()
{
    Logger::Log(LOG_DEBUG, "Sending heartbeat to API");

    Json::Value user_list(Json::arrayValue);
    m_sequencer->GetHeartbeatUserList(user_list);

    auto response = m_api_client->PutHeartbeat(user_list.asString());

    if (response.error_state != ApiErrorState::API_NO_ERROR)
    {
        Logger::Log(LOG_ERROR, "Failed to send heartbeat, error code: %d", static_cast<int>(response.error_state));
        return;
    }

    {
        Logger::Log(LOG_DEBUG, "Heartbeat sent, updating last heartbeat time");
        std::lock_guard<std::mutex> lock(m_time_mutex);
        m_last_heartbeat_time = std::chrono::system_clock::now();
    }
}