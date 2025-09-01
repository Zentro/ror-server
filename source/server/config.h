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
 * \file config.h
 * \brief YAML configuration handler with console and field validation
 * \author Rafael Galvan
 * \date 2025-07-15
 */

#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <regex>
#include <memory>
#include <mutex>
#include <vector>

// Forward declarations
class ConfigValidator;
class ConfigField;

/**
 * \brief Exception thrown for validation errors.
 */
class ValidationError : public std::runtime_error
{
public:
    /**
     * \brief Constructs a ValidationError with the given message.
     * \param message The error message.
     */
    ValidationError(const std::string &message);
};

/**
 * \brief Abstract base class for all configuration field validators
 */
class Validator
{
public:
    virtual ~Validator() = default;

    /**
     * \brief Validate a configuration value
     * \param value The YAML node containing the value to validate
     * \param error Reference to string that will contain error message if validation fails
     * \return true if validation passes, false otherwise
     */
    virtual bool validate(const YAML::Node &value, std::string &error) const = 0;

    /**
     * \brief Get a human-readable description of what this validator checks
     * \return String description of the validation criteria
     */
    virtual std::string description() const = 0;
};

class StringLengthValidator : public Validator
{
private:
    size_t min_len; ///< Minimum required string length
    size_t max_len; ///< Maximum allowed string length
public:
    /**
     * \brief Construct a new StringLengthValidator
     * \param min Minimum string length (default: 0)
     * \param max Maximum string length (default: SIZE_MAX)
     */
    StringLengthValidator(size_t min = 0, size_t max = SIZE_MAX);

    /**
     * \brief Validate string length
     * \param value The YAML node containing the string to validate
     * \param error Reference to string that will contain error message if validation fails
     * \return true if validation passes, false otherwise
     */
    bool validate(const YAML::Node &value, std::string &error) const override;

    /**
     * \brief Get description of string length requirements
     * \return Description string for help text
     */
    std::string description() const override;
};

/**
 * \brief Validates port numbers within a specified range
 *
 * Ensures port numbers are integers within the valid range (default 1-65535).
 */
class PortValidator : public Validator
{
private:
    int min_port; ///< Minimum allowed port number
    int max_port; ///< Maximum allowed port number

public:
    /**
     * \brief Construct a new PortValidator
     * \param min Minimum allowed port (default: 1)
     * \param max Maximum allowed port (default: 65535)
     */
    PortValidator(int min = 1, int max = 65535);

    /**
     * \brief Validate a port number
     * \param value YAML node containing the port number
     * \param error Error message output if validation fails
     * \return true if port is within valid range, false otherwise
     */
    bool validate(const YAML::Node &value, std::string &error) const override;

    /**
     * \brief Get description of port validation requirements
     * \return Description string for help text
     */
    std::string description() const override;
};

class ConfigField
{
private:
    std::string key;                                    ///< The configuration key (e.g., "port")
    YAML::Node default_value;                           ///< Default value if none specified
    std::vector<std::unique_ptr<Validator>> validators; ///< Chain of validators
    std::string description;                            ///< Human-readable description
    bool required;                                      ///< Whether this field is mandatory
    std::string cli_arg;                                ///< Long CLI argument (e.g., "--port")
    std::string cli_short;                              ///< Short CLI argument (e.g., "-p")
public:
    /**
     * \brief Construct a new ConfigField
     * \param k The configuration key (e.g., "port")
     * \param default_val Default value if none specified
     * \param req Whether this field is mandatory (the default is false)
     */
    ConfigField(const std::string &k, const YAML::Node &default_val = YAML::Node(), bool req = false);

    /**
     * \brief Set the human-readable description
     * \param description Description string
     * \return Reference to this ConfigField for method chaining
     */
    ConfigField &setDescription(const std::string &description);

    /**
     * \brief Add a validator to the validation chain
     * \param validator Unique pointer to validator instance
     * \return Reference to this ConfigField for method chaining
     */
    ConfigField &addValidator(std::unique_ptr<Validator> validator);

    /**
     * \brief Set CLI argument names
     * \param long_arg Long argument format (e.g., "--port")
     * \param short_arg Short argument format (e.g., "-p", optional)
     * \return Reference to this ConfigField for method chaining
     */
    ConfigField &setCLIArg(const std::string &long_arg, const std::string &short_arg = "");

    /**
     * \brief Validate a value against all configured validators
     * \param value YAML node containing value to validate
     * \param error Error message output if validation fails
     * \return true if all validations pass, false otherwise
     */
    bool validate(const YAML::Node &value, std::string &error) const;

    /**
     * \brief Parse CLI string value to appropriate YAML type
     * \param value String value from command line
     * \return YAML node with parsed and typed value
     */
    YAML::Node parseCliValue(const std::string &value) const;

    const std::string &getKey() const;         ///< Get configuration key
    const YAML::Node &getDefault() const;      ///< Get default value
    bool isRequired() const;                   ///< Check if field is required
    const std::string &getDescription() const; ///< Get description
    const std::string &getCLIArg() const;      ///< Get long CLI argument
    const std::string &getCLIShort() const;    ///< Get short CLI argument

    /**
     * \brief Get combined description of all validators
     * \return Comma-separated string of validator descriptions
     */
    std::string getValidatorDescriptions() const;
};

/**
 * \brief Singleton manager for global configuration system
 *
 * Manages all configuration fields, handles file I/O, CLI parsing,
 * and provides thread-safe access to configuration values.
 */
class GlobalConfigManager
{
private:
    std::unordered_map<std::string, std::unique_ptr<ConfigField>> fields; ///< Registered fields
    YAML::Node config_data;                                               ///< Configuration from file
    YAML::Node cli_overrides;                                             ///< CLI argument overrides
    std::string config_file;                                              ///< Configuration file path
    mutable std::mutex config_mutex;                                      ///< Thread safety mutex
    bool initialized = false;                                             ///< Initialization state

    /**
     * \brief Private constructor for singleton pattern
     */
    GlobalConfigManager() = default;

    /**
     * \brief Internal validation without mutex (assumes lock held)
     * \return true if validation passes, false otherwise
     */
    bool validateAllUnsafe();

    /**
     * \brief Print CLI help to console
     */
    void printCLIHelp() const;

public:
    /**
     * \brief Get singleton instance
     * \return Reference to the global configuration manager
     */
    static GlobalConfigManager &getInstance();

    // Delete copy constructor and assignment operator
    GlobalConfigManager(const GlobalConfigManager &) = delete;
    GlobalConfigManager &operator=(const GlobalConfigManager &) = delete;

    /**
     * \brief Initialize the configuration system
     * \param filename YAML configuration file path
     */
    void initialize(const std::string &filename = "config.yaml");

    /**
     * \brief Register a new configuration field
     * \param key Configuration key (dot notation supported)
     * \param default_value Default value if not specified
     * \param required Whether field is mandatory
     * \return Reference to created ConfigField for method chaining
     */
    ConfigField &registerField(const std::string &key, const YAML::Node &default_value = YAML::Node(), bool required = false);

    /**
     * \brief Parse command-line arguments
     * \param argc Argument count from main()
     * \param argv Argument values from main()
     * \return true if parsing succeeded, false if help shown or error occurred
     */
    bool parseArguments(int argc, char *argv[]);

    /**
     * \brief Load configuration from YAML file
     * \return true if loaded and validated successfully, false otherwise
     */
    bool load();

    /**
     * \brief Save current configuration to YAML file
     * \return true if saved successfully, false otherwise
     */
    bool save() const;

    /**
     * \brief Validate all configuration fields
     * \return true if all validations pass, false otherwise
     */
    bool validateAll();

    /**
     * \brief Get configuration value with type safety
     * \tparam T Expected value type
     * \param key Configuration key
     * \return Typed configuration value
     * \throws std::runtime_error if key not found or conversion fails
     *
     * Priority order: CLI overrides > config file > default value
     */
    template <typename T>
    T get(const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        auto field_it = fields.find(key);
        if (field_it == fields.end())
        {
            throw std::runtime_error("Unknown config field: " + key);
        }

        // Priority: CLI override > config file > default
        if (cli_overrides[key])
        {
            return cli_overrides[key].as<T>();
        }
        else if (config_data[key])
        {
            return config_data[key].as<T>();
        }
        else if (field_it->second->getDefault())
        {
            return field_it->second->getDefault().as<T>();
        }
        else
        {
            throw std::runtime_error("No value or default for field: " + key);
        }
    }

    /**
     * \brief Set configuration value with validation
     * \tparam T Value type
     * \param key Configuration key
     * \param value New value to set
     * \return true if value was valid and set, false otherwise
     */
    template <typename T>
    bool set(const std::string &key, const T &value)
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        auto field_it = fields.find(key);
        if (field_it == fields.end())
        {
            std::cerr << "Warning: Setting unknown config field '" << key << "'" << std::endl;
        }

        YAML::Node node = YAML::Node(value);

        if (field_it != fields.end())
        {
            std::string error;
            if (!field_it->second->validate(node, error))
            {
                std::cerr << "Validation error for '" << key << "': " << error << std::endl;
                return false;
            }
        }

        config_data[key] = node;
        return true;
    }

    /**
     * \brief Print configuration help to console
     */
    void printHelp() const;

    /**
     * \brief Create default configuration file with all default values
     */
    void createDefaultConfig();
};

/**
 * \brief Global configuration namespace providing convenient access
 *
 * This namespace provides a clean, global interface to the configuration
 * system that can be accessed from anywhere in the application.
 */
namespace Config
{
    /**
     * \brief Initialize the configuration system
     * \param filename YAML configuration file path
     */
    void initialize(const std::string &filename = "config.yaml");

    /**
     * \brief Parse command-line arguments
     * \param argc Argument count from main()
     * \param argv Argument values from main()
     * \return true if parsing succeeded, false if help shown or error
     */
    bool parseArguments(int argc, char *argv[]);

    /**
     * \brief Load configuration from file
     * \return true if loaded successfully, false otherwise
     */
    bool load();

    /**
     * \brief Save configuration to file
     * \return true if saved successfully, false otherwise
     */
    bool save();

    /**
     * \brief Create default configuration file
     */
    void createDefault();

    /**
     * \brief Print configuration help
     */
    void printHelp();

    /**
     * \brief Register a new configuration field
     * \param key Configuration key
     * \param default_value Default value
     * \param required Whether field is mandatory
     * \return Reference to ConfigField for method chaining
     */
    ConfigField &registerField(const std::string &key, const YAML::Node &default_value = YAML::Node(), bool required = false);

    /**
     * \brief Get typed configuration value
     * \tparam T Expected value type
     * \param key Configuration key
     * \return Typed configuration value
     */
    template <typename T>
    T get(const std::string &key)
    {
        return GlobalConfigManager::getInstance().get<T>(key);
    }

    /**
     * \brief Set configuration value
     * \tparam T Value type
     * \param key Configuration key
     * \param value New value
     * \return true if set successfully, false otherwise
     */
    template <typename T>
    bool set(const std::string &key, const T &value)
    {
        return GlobalConfigManager::getInstance().set(key, value);
    }

    // Getters
    std::string ip();       ///< Get server IP address
    int port();             ///< Get server port
    std::string name();     ///< Get server name
    std::string owner();    ///< Get server owner
    bool debug();           ///< Get debug mode status
    std::string authFile(); ///< Get server auth file
    std::string banFile();  ///< Get server ban file
    std::string motdFile(); ///< Get server MOTD file
    std::string password(); ///< Get server password

    // Setters
    bool setIP(const std::string &ip);             ///< Set server IP
    bool setPort(int port);                        ///< Set server port
    bool setName(const std::string &name);         ///< Set server name
    bool setOwner(const std::string &owner);       ///< Set server owner
    bool setDebug(bool debug);                     ///< Set debug mode
    bool setAuthFile(const std::string &file);     ///< Set server auth file
    bool setBanFile(const std::string &file);      ///< Set server ban file
    bool setMotdFile(const std::string &file);     ///< Set server MOTD file
    bool setPassword(const std::string &password); ///< Set server password

    // Getters
    std::string apiEndpoint(); ///< Get API endpoint
    std::string apiKey();      ///< Get API key

    // Setters
    bool setAPIEndpoint(const std::string &endpoint); ///< Set API endpoint
    bool setAPIKey(const std::string &key);           ///< Set API key

    // Getters
    int maxPlayers();      ///< Get maximum players
    std::string terrain(); ///< Get terrain type

    // Setters
    bool setMaxPlayers(int max);                 ///< Set maximum players
    bool setTerrain(const std::string &terrain); ///< Set terrain type
}

/**
 * \brief Setup function to register all configuration fields
 *
 * Call this function once at application startup to register all
 * configuration fields with their validators and CLI arguments.
 *
 * \note This should be called before any other configuration operations.
 */
void setupConfiguration();
