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
along with Rigs of Rods Server. If not, see <http://www.gnu.org/licenses/>.
*/

#include "playerfilter.h"

#include "config.h"
#include "logger.h"
#include "sequencer.h"
#include "utils.h"

#include <fstream>
#include <json/json.h>

PlayerFilter::PlayerFilter(Sequencer* sequencer)
    : m_sequencer(sequencer)
{
}

void PlayerFilter::SaveBlacklistToFile()
{
    std::ofstream f;
    f.open(Config::getBlacklistFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open the local blacklist file ('%s'). Bans were not saved.",
                    Config::getBlacklistFile().c_str());
        return;
    }

    Json::Value j_bans(Json::arrayValue);
    std::vector<ban_t> bans = m_sequencer->GetBanListCopy();

    for (ban_t& ban : bans)
    {
        Json::Value j_ban(Json::objectValue);
		j_ban["bid"] = ban.bid;
        j_ban["ip"] = ban.ip;
        j_ban["nickname"] = ban.nickname;
        j_ban["banned_by_nickname"] = ban.bannedby_nick;
        j_ban["message"] = ban.banmsg;
        j_bans.append(j_ban);
    }

    Json::Value j_doc(Json::objectValue);
    j_doc["bans"] = j_bans;

    Json::StyledStreamWriter j_writer;
    j_writer.write(f, j_doc);
}

void PlayerFilter::SavePlayerWhitelistToFile()
{
    std::ofstream f;
    f.open(Config::getPlayerWhitelistFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open the player-whitelist file ('%s'). Whitelist was not saved.",
                    Config::getPlayerWhitelistFile().c_str());
        return;
    }

    Json::Value j_entries(Json::arrayValue);
    for (const player_whitelist_t& e : m_sequencer->m_whitelist_entries)
    {
        Json::Value j_entry(Json::objectValue);
        if (e.token[0] != '\0')
            j_entry["token"] = e.token;
        if (e.username[0] != '\0')
            j_entry["username"] = e.username;
        j_entries.append(j_entry);
    }

    Json::Value j_doc(Json::objectValue);
    j_doc["whitelisted_players"] = j_entries;

    Json::StyledStreamWriter j_writer;
    j_writer.write(f, j_doc);
}

bool PlayerFilter::LoadBlacklistFromFile()
{
    std::ifstream f;
    f.open(Config::getBlacklistFile(), std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open the local blacklist file ('%s'). No bans were loaded.",
                    Config::getBlacklistFile().c_str());
        return false;
    }

    if (Utils::IsEmptyFile(f))
    {
        f.close();
        Logger::Log(LOG_WARN,
                    "Local blacklist file ('%s') is empty.",
                    Config::getBlacklistFile().c_str());
        return false;
    }

    Json::Value j_doc;
    Json::Reader j_reader;
    j_reader.parse(f, j_doc);
    if (!j_reader.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't parse blacklist file, messages:\n%s",
                    j_reader.getFormattedErrorMessages().c_str());
        return false;
    }

    for (Json::Value& j_ban: j_doc["bans"])
    {
        m_sequencer->RecordBan(
            // ban IDs are reset to start at 1 on every start/restart
            j_ban["ip"].asString(),
            j_ban["nickname"].asString(),
            j_ban["banned_by_nickname"].asString(),
            j_ban["message"].asString());
    }

    return true;
}

bool PlayerFilter::LoadPlayerWhitelist(const std::string& filepath)
{
    if (filepath.empty())
        return false;

    std::ifstream f;
    f.open(filepath, std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open the player-whitelist file ('%s'). No player whitelist loaded.",
                    filepath.c_str());
        return false;
    }

    if (Utils::IsEmptyFile(f))
    {
        f.close();
        Logger::Log(LOG_WARN, "Player-whitelist file ('%s') is empty.", filepath.c_str());
        return false;
    }

    Json::Value j_doc;
    Json::Reader j_reader;
    j_reader.parse(f, j_doc);
    if (!j_reader.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't parse player-whitelist file ('%s'), messages:\n%s",
                    filepath.c_str(), j_reader.getFormattedErrorMessages().c_str());
        return false;
    }

    int count = 0;
    for (const Json::Value& entry : j_doc["whitelisted_players"])
    {
        bool has_token    = entry.isMember("token")    && entry["token"].isString();
        bool has_username = entry.isMember("username") && entry["username"].isString();

        if (!has_token && !has_username)
        {
            Logger::Log(LOG_WARN,
                        "Skipping player-whitelist entry with neither 'token' nor 'username'.");
            continue;
        }

        std::string token    = has_token    ? entry["token"].asString()    : "";
        std::string username = has_username ? entry["username"].asString() : "";
        m_sequencer->RecordWhitelistedPlayer(token, username);
        count++;
    }

    m_sequencer->m_player_whitelist_loaded = true;
    Logger::Log(LOG_INFO, "Loaded %d player whitelist entry/entries from '%s'.", count, filepath.c_str());
    return true;
}
