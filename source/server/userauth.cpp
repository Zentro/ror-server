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

#include "userauth.h"

#include "config.h"
#include "rornet.h"
#include "logger.h"
#include "api.h"

#include <stdexcept>
#include <cstdio>
#include <fstream>

#ifdef __GNUC__

#include <unistd.h>
#include <stdlib.h>

#endif

void UserAuth::loadConfig() {
    std::ifstream file(authFile);

    /**
     * File doesn't yet exist, cache remains empty
     */
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') {
            /**
             * Skip empty lines and comments, denoted as ';'
             */
            continue;
        }

        /**
         * Read the file with the format of:
         * <authLevel> <token> <username>
         */
        std::istringstream iss(line);
        std::string authLevelStr, token, username;
        if (iss >> authLevelStr >> token >> username) {
            int authLevel = std::stoi(authLevelStr);

            this->addUser(username, token, authLevel);
        }
    }
}

void UserAuth::saveConfig() {
    std::lock_guard<std::mutex> lock(userCacheMutex);
    std::ofstream file(authFile);

    /**
     * File doesn't yet exist, cache is likely already empty
     */
    if (!file.is_open()) {
        return;
    }

    /**
     * Save into the file with the same format
     */
    for (const auto& [username, userAuthPair] : userCache) {
        const std::string& token = userAuthPair.first;
        int authLevel = userAuthPair.second;
        file << authLevel << " " << token << " " << username << "\n";
    }

    /**
     * Flush changes to the file
     */
    file.flush();
    file.close();
}

void UserAuth::addUser(const std::string &username, const std::string &token, int authLevel) {
    std::lock_guard<std::mutex> lock(userCacheMutex);

    if (username.empty() || token.empty()) {
        throw std::invalid_argument("Username and token must not be empty");
    }

    /**
     * Not every auth level is allowed to be set
     */
    if (authLevel & RoRnet::AUTH_RANKED) authLevel &= ~RoRnet::AUTH_RANKED;
    if (authLevel & RoRnet::AUTH_BANNED) authLevel &= ~RoRnet::AUTH_BANNED;

    UserAuthPair userAuthPair = std::make_pair(token, authLevel);
    userCache[username] = userAuthPair;
}

void UserAuth::removeUser(const std::string &username) {
    std::lock_guard<std::mutex> lock(userCacheMutex);

    /**
     * Look up user in cache, remove it
     * This is NOT persisted until saveConfig() is called
     */
    auto it = userCache.find(username);
    if (it != userCache.end()) {
        userCache.erase(it);
    }
}

bool UserAuth::userExists(const std::string &username) {
    std::lock_guard<std::mutex> lock(userCacheMutex);

    return userCache.find(username) != userCache.end();
}

size_t UserAuth::getUserCount() {
    std::lock_guard<std::mutex> lock(userCacheMutex);
    return userCache.size();
}

int UserAuth::resolveUser(const std::string &username, const std::string &token) {
    std::lock_guard<std::mutex> lock(userCacheMutex);

    /**
     * Look up the user in the cache
     */
    auto it = userCache.find(username);
    if (it != userCache.end()) {
        const UserAuthPair& userAuthPair = it->second;
        const std::string& storedToken = userAuthPair.first;
        int authLevel = userAuthPair.second;

        if (storedToken == token) {
            /**
             * Token matches, return the stored auth level
             */
            return authLevel;
        } else {
            /**
             * Token does not match, return no auth
             */
            return RoRnet::AUTH_NONE;
        }
    }

    /**
     * User not found, return no auth
     */
    return RoRnet::AUTH_NONE;
}