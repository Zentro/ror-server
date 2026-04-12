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

#include "actorfilter.h"

#include "config.h"
#include "logger.h"
#include "sequencer.h"
#include "utils.h"

#include <fstream>
#include <json/json.h>

ActorFilter::ActorFilter(Sequencer* sequencer)
    : m_sequencer(sequencer)
{
}

void ActorFilter::SaveBannedActorsToFile()
{
    std::ofstream f;
    f.open(Config::getBannedActorsFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open banned-actors file ('%s'). Bans were not saved.",
                    Config::getBannedActorsFile().c_str());
        return;
    }

    Json::Value j_entries(Json::arrayValue);
    for (const actor_ban_t& e : m_sequencer->m_actor_bans)
    {
        Json::Value j_entry(Json::objectValue);
        j_entry["filename"] = e.filename;
        if (e.hash[0] != '\0')
            j_entry["hash"] = e.hash;
        j_entries.append(j_entry);
    }

    Json::Value j_doc(Json::objectValue);
    j_doc["banned_actors"] = j_entries;

    Json::StyledStreamWriter j_writer;
    j_writer.write(f, j_doc);
}

void ActorFilter::SaveActorWhitelistToFile()
{
    std::ofstream f;
    f.open(Config::getActorWhitelistFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open actor-whitelist file ('%s'). Whitelist was not saved.",
                    Config::getActorWhitelistFile().c_str());
        return;
    }

    Json::Value j_entries(Json::arrayValue);
    for (const actor_whitelist_t& e : m_sequencer->m_actor_whitelist)
    {
        Json::Value j_entry(Json::objectValue);
        j_entry["filename"] = e.filename;
        if (e.hash[0] != '\0')
            j_entry["hash"] = e.hash;
        j_entries.append(j_entry);
    }

    Json::Value j_doc(Json::objectValue);
    j_doc["allowed_actors"] = j_entries;

    Json::StyledStreamWriter j_writer;
    j_writer.write(f, j_doc);
}

bool ActorFilter::LoadBannedActors(const std::string& filepath)
{
    if (filepath.empty())
        return false;

    std::ifstream f;
    f.open(filepath, std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open banned-actors file ('%s'). No actor bans loaded.",
                    filepath.c_str());
        return false;
    }

    if (Utils::IsEmptyFile(f))
    {
        f.close();
        Logger::Log(LOG_WARN, "Banned-actors file ('%s') is empty.", filepath.c_str());
        return false;
    }

    Json::Value j_doc;
    Json::Reader j_reader;
    j_reader.parse(f, j_doc);
    if (!j_reader.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't parse banned-actors file ('%s'), messages:\n%s",
                    filepath.c_str(), j_reader.getFormattedErrorMessages().c_str());
        return false;
    }

    bool hash_rules_present = false;
    int count = 0;
    for (const Json::Value& entry : j_doc["banned_actors"])
    {
        if (!entry.isMember("filename") || !entry["filename"].isString())
        {
            Logger::Log(LOG_WARN, "Skipping banned-actors entry without 'filename' field.");
            continue;
        }

        std::string hash = (entry.isMember("hash") && entry["hash"].isString())
            ? entry["hash"].asString() : "";
        if (!hash.empty())
            hash_rules_present = true;

        m_sequencer->RecordBannedActor(entry["filename"].asString(), hash);
        count++;
    }

    if (hash_rules_present)
    {
        Logger::Log(LOG_WARN,
                    "Banned-actors file contains hash-based rules, but the current RoRnet protocol"
                    " does not transmit file hashes in MSG2_STREAM_REGISTER."
                    " Those rules will match on filename alone.");
    }

    Logger::Log(LOG_INFO, "Loaded %d banned-actor rule(s) from '%s'.", count, filepath.c_str());
    return true;
}

bool ActorFilter::LoadActorWhitelist(const std::string& filepath)
{
    if (filepath.empty())
        return false;

    std::ifstream f;
    f.open(filepath, std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't open actor-whitelist file ('%s'). No actor whitelist loaded.",
                    filepath.c_str());
        return false;
    }

    if (Utils::IsEmptyFile(f))
    {
        f.close();
        Logger::Log(LOG_WARN, "Actor-whitelist file ('%s') is empty.", filepath.c_str());
        return false;
    }

    Json::Value j_doc;
    Json::Reader j_reader;
    j_reader.parse(f, j_doc);
    if (!j_reader.good())
    {
        Logger::Log(LOG_WARN,
                    "Couldn't parse actor-whitelist file ('%s'), messages:\n%s",
                    filepath.c_str(), j_reader.getFormattedErrorMessages().c_str());
        return false;
    }

    bool hash_rules_present = false;
    int count = 0;
    for (const Json::Value& entry : j_doc["allowed_actors"])
    {
        if (!entry.isMember("filename") || !entry["filename"].isString())
        {
            Logger::Log(LOG_WARN, "Skipping actor-whitelist entry without 'filename' field.");
            continue;
        }

        std::string hash = (entry.isMember("hash") && entry["hash"].isString())
            ? entry["hash"].asString() : "";
        if (!hash.empty())
            hash_rules_present = true;

        m_sequencer->RecordWhitelistedActor(entry["filename"].asString(), hash);
        count++;
    }

    if (hash_rules_present)
    {
        Logger::Log(LOG_WARN,
                    "Actor-whitelist file contains hash-based rules, but the current RoRnet protocol"
                    " does not transmit file hashes in MSG2_STREAM_REGISTER."
                    " Those rules will match on filename alone.");
    }

    m_sequencer->m_actor_whitelist_loaded = true;
    Logger::Log(LOG_INFO, "Loaded %d actor whitelist rule(s) from '%s'.", count, filepath.c_str());
    return true;
}
