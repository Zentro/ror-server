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

#pragma once

#include "api.h"
#include "UnicodeStrings.h"

#include <unordered_map>
#include <string>
#include <mutex>

/**
 * \file userauth.h
 * \brief User auth
 * \author Rafael Galvan
 * \date 2025-07-15
 */

/**
 * \class UserAuth
 * Example usage:
 * \code
 * UserAuth userAuth("zauth.yaml", apiClient);
 * userAuth.addUser("username", "token");
 * userAuth.removeUser("username");
 * \endcode
 */
class UserAuth
{
private:
    /** \brief Path to the authentication file */
    std::string authFile;

    /** \brief User authentication pair (token, authlevel) */
    using UserAuthPair = std::pair<std::string, int>;

    /** \brief User cache for storing token mappings (username, UserAuthPair) */
    std::unordered_map<std::string, UserAuthPair> userCache;

    /** \brief Mutex for thread-safe access to userCache */
    std::mutex userCacheMutex;

    /** \brief Reference to the API client for authentication operations */
    ApiClient &apiClient;

    /**
     * \brief Loads user token data from the authorizations file
     * 
     * Reads the authorizations file specified in the constructor and populates
     * the userCache with username/token pairs. If the file doesn't exist or
     * cannot be read, the cache remains empty.
     * 
     * \throws std::runtime_error if the file cannot be read
     * 
     * \note This function is called during the construction of the UserAuth
     *       object to initialize the user cache.
     */
    void loadConfig();

    /**
     * \brief Saves current user authentication data to the authorizations file
     * 
     * Writes the current userCache contents to the authorizations file specified
     * in the constructor. If the file cannot be written, an error is logged.
     * 
     * \note This function is called during the destruction of the UserAuth
     *       object to persist the user cache.
     */
    void saveConfig();

public:
    /**
     * \brief Constructs a UserAuth object with specified file and API client
     * 
     * \param authFile Path to the authorizations file
     * \param apiClient Reference to the API client
     * 
     * The constructor automatically loads the user authentication data from the
     * specified authorizations file.
     */
    explicit UserAuth(const std::string& authFile, ApiClient& apiClient);

    /**
     * \brief Destructor that saves current state to file
     * 
     * A destructor that calls saveConfig() to persist the user cache.
     * 
     * \note Destructor is marked noexcept and will not throw exceptions
     */
    ~UserAuth() noexcept;

    /**
     * \brief Adds or updates a user's authentication token.
     * 
     * \param username The username to add or update (case-sensitive)
     * \param token The authentication token for the user
     * \param authLevel The authorization level for the user (default is 0)
     * 
     * \throws std::invalid_argument if username or token is empty
     * \throws std::runtime_error if the user cannot be added
     * 
     * If the user already exists, their token will be updated with the new
     * value. The operation is thread-safe.
     * 
     * \note Changes are persisted to file only when the object is destroyed
     *       or saveConfig() is called explicitly.
     */
    void addUser(const std::string &username, const std::string &token, int authLevel);

    /**
     * \brief Removes a user from the authentication system.
     * 
     * \param username The username to remove (case-sensitive)
     * 
     * \throws std::invalid_argument if username is empty
     * \throws std::runtime_error if the user cannot be removed
     * 
     * This operation is thread-safe.
     * 
     * \note Changes are persisted to file only when the object is destroyed
     *       or saveConfig() is called explicitly.
     */
    void removeUser(const std::string &username);

    /**
     * \brief Checks if a user exists in the authentication system.
     * 
     * \param username The username to check (case-sensitive)
     * 
     * \return true if the user exists, false otherwise
     * 
     */
    bool userExists(const std::string &username);

    /**
     * \brief Resolves a user's authentication token against the API client
     * 
     * \param username The username to resolve (case-sensitive)
     * \param token The session token to resolve
     * 
     * \return The authorization level for the user, or 0 if not found
     */
    int resolveUser(const std::string &username, const std::string &token);

    /**
     * \brief Gets the number of users currently stored
     * 
     * \return The number of users in the cache
     * 
     * This is a thread-safe read-only operation that returns the current
     * size of the user cache. 
     */
    size_t getUserCount();

    // Prevent copying to avoid issues with reference member and file handling
    UserAuth(const UserAuth&) = delete;
    UserAuth& operator=(const UserAuth&) = delete;
    
    // Allow moving
    UserAuth(UserAuth&&) = default;
    UserAuth& operator=(UserAuth&&) = default;
};
