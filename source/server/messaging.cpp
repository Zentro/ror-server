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
along with "Rigs of Rods Server". 
If not, see <http://www.gnu.org/licenses/>.
*/

#include "messaging.h"

#include "sequencer.h"
#include "rornet.h"
#include "logger.h"
#include "SocketW.h"
#include "config.h"
#include "http.h"
#include "UnicodeStrings.h"

#include <cstring>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <mutex>

static stream_traffic_t s_traffic = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static std::mutex s_traffic_mutex;

namespace Messaging {

    void UpdateMinuteStats() {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);

        // normal bandwidth
        s_traffic.bandwidthIncomingRate = (s_traffic.bandwidthIncoming - s_traffic.bandwidthIncomingLastMinute) / 60;
        s_traffic.bandwidthIncomingLastMinute = s_traffic.bandwidthIncoming;
        s_traffic.bandwidthOutgoingRate = (s_traffic.bandwidthOutgoing - s_traffic.bandwidthOutgoingLastMinute) / 60;
        s_traffic.bandwidthOutgoingLastMinute = s_traffic.bandwidthOutgoing;

        // dropped bandwidth
        s_traffic.bandwidthDropIncomingRate =
                (s_traffic.bandwidthDropIncoming - s_traffic.bandwidthDropIncomingLastMinute) / 60;
        s_traffic.bandwidthDropIncomingLastMinute = s_traffic.bandwidthDropIncoming;
        s_traffic.bandwidthDropOutgoingRate =
                (s_traffic.bandwidthDropOutgoing - s_traffic.bandwidthDropOutgoingLastMinute) / 60;
        s_traffic.bandwidthDropOutgoingLastMinute = s_traffic.bandwidthDropOutgoing;
    }

    void StatsAddIncoming(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthIncoming += static_cast<double>(bytes);
    }

    void StatsAddOutgoing(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthOutgoing += static_cast<double>(bytes);
    }

    void StatsAddIncomingDrop(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthDropIncoming += static_cast<double>(bytes);
    }

    void StatsAddOutgoingDrop(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthDropOutgoing += static_cast<double>(bytes);
    }

    stream_traffic_t GetTrafficStats() {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        return s_traffic;
    }

/**
 * @param socket  Socket to communicate over
 * @param type    Command ID
 * @param source  Source ID
 * @param len     Data length
 * @param content Payload
 * @return 0 on success
 */
    int SWSendMessage(SWInetSocket *socket, int type, int source, unsigned int streamid, unsigned int len,
                    const char *content) {
        assert(socket != nullptr);

        SWBaseSocket::SWBaseError error;
        RoRnet::Header head;

        const int msgsize = sizeof(RoRnet::Header) + len;

        if (msgsize >= RORNET_MAX_MESSAGE_LENGTH) {
            ROR_SVR_ERROR("UID: {} - attempt to send too long message", source);
            return -4;
        }

        char buffer[RORNET_MAX_MESSAGE_LENGTH];

        memset(&head, 0, sizeof(RoRnet::Header));
        head.command = type;
        head.source = source;
        head.size = len;
        head.streamid = streamid;

        // construct buffer
        memset(buffer, 0, RORNET_MAX_MESSAGE_LENGTH);
        memcpy(buffer, (char *) &head, sizeof(RoRnet::Header));
        memcpy(buffer + sizeof(RoRnet::Header), content, len);

        if (socket->fsend(buffer, msgsize, &error) < msgsize)
        {
            ROR_SVR_ERROR("send error -1: {}", error.get_error());
            return -1;
        }
        StatsAddOutgoing(msgsize);
        return 0;
    }

/**
 * @param out_type        Message type, see RoRnet::RoRnet::MSG2_* macros in rornet.h
 * @param out_source      Magic. Value 5000 used by serverlist to check this server.
 * @return                0 on success, negative number on error.
 */
    int SWReceiveMessage(
            SWInetSocket *socket,
            int *out_type,
            int *out_source,
            unsigned int *out_stream_id,
            unsigned int *out_payload_len,
            char *out_payload,
            unsigned int payload_buf_len) {
        assert(socket != nullptr);
        assert(out_type != nullptr);
        assert(out_source != nullptr);
        assert(out_stream_id != nullptr);
        assert(out_payload != nullptr);

        SWBaseSocket::SWBaseError error;

        RoRnet::Header head;
        if (socket->frecv((char*)&head, sizeof(RoRnet::Header), &error) < sizeof(RoRnet::Header))
        {
            // this also happens when the connection is canceled
            return -2;
        }

        *out_type = head.command;
        *out_source = head.source;
        *out_payload_len = head.size;
        *out_stream_id = head.streamid;

        if ( head.size > payload_buf_len) {
            ROR_SVR_ERROR("SWReceiveMessage(): payload too long: {} b (max. is {} b)", head.size,
                          payload_buf_len);
            return -3;
        }

        if (head.size > 0) {
            //read the rest
            std::memset(out_payload, 0, payload_buf_len);
            if (socket->frecv(out_payload, head.size, &error) < head.size) {
                return -1;
            }
        }

        StatsAddIncoming(sizeof(RoRnet::Header) + head.size);
        return 0;
    }

    int getTime() { return (int) time(NULL); }

} // namespace Messaging
