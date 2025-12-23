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
    \file    Heartbeat.cpp
    \brief   Heartbeat class implementation.
    \author  Rafael Galvan
    \date    2025-12-18
*/

#include "Heartbeat.h"
#include "ApiClient.h"

Heartbeat::Heartbeat(ApiClient& api_client, Sequencer& sequencer)
    : m_api_client(&api_client),
      m_sequencer(&sequencer),
      m_running(false)
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
    m_heartbeat_thread = std::thread(&Heartbeat::WorkerThread, this);
}

void Heartbeat::Stop()
{
    if (!m_running.load())
    {
        return;
    }

    m_running.store(false);
    m_cv.notify_all();
;
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

void Heartbeat::WorkerThread()
{
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
    auto response = m_api_client->PutHeartbeat();

    if (response.error_state != API_NO_ERROR)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_time_mutex);
        m_last_heartbeat_time = std::chrono::system_clock::now();
    }
}