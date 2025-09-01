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

#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>

// ValidationError implementation
ValidationError::ValidationError(const std::string &msg) : std::runtime_error(msg) {}

// PortValidator implementation
PortValidator::PortValidator(int min, int max) : min_port(min), max_port(max) {}

bool PortValidator::validate(const YAML::Node &value, std::string &error) const
{
    if (!value.IsScalar())
    {
        error = "Port must be a number";
        return false;
    }

    try
    {
        int port = value.as<int>();
        if (port < min_port || port > max_port)
        {
            error = "Port must be between " + std::to_string(min_port) + " and " + std::to_string(max_port);
            return false;
        }
    }
    catch (const YAML::BadConversion &)
    {
        error = "Port must be a valid integer";
        return false;
    }
    return true;
}

std::string PortValidator::description() const
{
    return "Port number (" + std::to_string(min_port) + "-" + std::to_string(max_port) + ")";
}

// StringLengthValidator implementation
StringLengthValidator::StringLengthValidator(size_t min, size_t max) : min_len(min), max_len(max) {}

bool StringLengthValidator::validate(const YAML::Node &value, std::string &error) const
{
    if (!value.IsScalar())
    {
        error = "Value must be a string";
        return false;
    }

    std::string str = value.as<std::string>();
    if (str.length() < min_len)
    {
        error = "String too short (minimum " + std::to_string(min_len) + " characters)";
        return false;
    }
    if (str.length() > max_len)
    {
        error = "String too long (maximum " + std::to_string(max_len) + " characters)";
        return false;
    }
    return true;
}

std::string StringLengthValidator::description() const
{
    if (max_len == SIZE_MAX)
    {
        return "String (min " + std::to_string(min_len) + " chars)";
    }
    return "String (" + std::to_string(min_len) + "-" + std::to_string(max_len) + " chars)";
}

ConfigField::ConfigField(const std::string &k, const YAML::Node &default_val, bool req)
    : key(k), default_value(default_val), required(req) {}

ConfigField &ConfigField::setDescription(const std::string &desc)
{
    description = desc;
    return *this;
}

ConfigField &ConfigField::addValidator(std::unique_ptr<Validator> validator)
{
    validators.push_back(std::move(validator));
    return *this;
}

ConfigField &ConfigField::setCLIArg(const std::string &long_arg, const std::string &short_arg)
{
    cli_arg = long_arg;
    cli_short = short_arg;
    return *this;
}

bool ConfigField::validate(const YAML::Node &value, std::string &error) const
{
    for (const auto &validator : validators)
    {
        if (!validator->validate(value, error))
        {
            return false;
        }
    }
    return true;
}

YAML::Node ConfigField::parseCliValue(const std::string &value) const
{
    // Try to parse as different types

    // Boolean values
    if (value == "true" || value == "1" || value == "yes" || value == "on")
    {
        return YAML::Node(true);
    }
    if (value == "false" || value == "0" || value == "no" || value == "off")
    {
        return YAML::Node(false);
    }

    // Try integer
    try
    {
        size_t pos;
        long long_val = std::stol(value, &pos);
        if (pos == value.length())
        {
            return YAML::Node(static_cast<int>(long_val));
        }
    }
    catch (...)
    {
    }

    // Try float
    try
    {
        size_t pos;
        float float_val = std::stof(value, &pos);
        if (pos == value.length())
        {
            return YAML::Node(float_val);
        }
    }
    catch (...)
    {
    }

    // Default to string
    return YAML::Node(value);
}

const std::string &ConfigField::getKey() const { return key; }
const YAML::Node &ConfigField::getDefault() const { return default_value; }
bool ConfigField::isRequired() const { return required; }
const std::string &ConfigField::getDescription() const { return description; }
const std::string &ConfigField::getCLIArg() const { return cli_arg; }
const std::string &ConfigField::getCLIShort() const { return cli_short; }

std::string ConfigField::getValidatorDescriptions() const
{
    std::string desc;
    for (size_t i = 0; i < validators.size(); ++i)
    {
        if (i > 0)
            desc += ", ";
        desc += validators[i]->description();
    }
    return desc;
}

GlobalConfigManager &GlobalConfigManager::getInstance()
{
    static GlobalConfigManager instance;
    return instance;
}

void GlobalConfigManager::initialize(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(config_mutex);
    config_file = filename;
    initialized = true;
}

ConfigField &GlobalConfigManager::registerField(const std::string &key, const YAML::Node &default_value, bool required)
{
    std::lock_guard<std::mutex> lock(config_mutex);
    auto field = std::make_unique<ConfigField>(key, default_value, required);
    ConfigField *field_ptr = field.get();
    fields[key] = std::move(field);
    return *field_ptr;
}

bool GlobalConfigManager::parseArguments(int argc, char *argv[])
{
    std::lock_guard<std::mutex> lock(config_mutex);

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        // Handle help
        if (arg == "--help" || arg == "-h")
        {
            printCLIHelp();
            return false;
        }

        // Find matching field
        ConfigField *field = nullptr;
        std::string value;

        // Check for --key=value format
        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos)
        {
            std::string arg_name = arg.substr(0, eq_pos);
            value = arg.substr(eq_pos + 1);

            for (const auto &[key, field_ptr] : fields)
            {
                if (("--" + key) == arg_name || field_ptr->getCLIArg() == arg_name ||
                    (!field_ptr->getCLIShort().empty() && field_ptr->getCLIShort() == arg_name))
                {
                    field = field_ptr.get();
                    break;
                }
            }
        }
        // Check for --key value format
        else
        {
            for (const auto &[key, field_ptr] : fields)
            {
                if (("--" + key) == arg || field_ptr->getCLIArg() == arg ||
                    (!field_ptr->getCLIShort().empty() && field_ptr->getCLIShort() == arg))
                {
                    field = field_ptr.get();
                    break;
                }
            }

            if (field && i + 1 < argc)
            {
                value = argv[++i];
            }
            else if (field)
            {
                // Boolean flag without value
                value = "true";
            }
        }

        if (field)
        {
            YAML::Node parsed_value = field->parseCliValue(value);

            // Validate the CLI value
            std::string error;
            if (!field->validate(parsed_value, error))
            {
                std::cerr << "CLI argument validation error for '" << field->getKey() << "': " << error << std::endl;
                return false;
            }

            cli_overrides[field->getKey()] = parsed_value;
            std::cout << "CLI override: " << field->getKey() << " = " << value << std::endl;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Unknown CLI argument: " << arg << std::endl;
            std::cerr << "Use --help to see available options." << std::endl;
            return false;
        }
    }

    return true;
}

void GlobalConfigManager::printCLIHelp() const
{
    std::cout << "Command Line Arguments:" << std::endl;
    std::cout << "  --help, -h                     Show this help message" << std::endl;
    std::cout << std::endl;

    for (const auto &[key, field] : fields)
    {
        std::cout << "  --" << key;
        if (!field->getCLIArg().empty() && field->getCLIArg() != ("--" + key))
        {
            std::cout << ", " << field->getCLIArg();
        }
        if (!field->getCLIShort().empty())
        {
            std::cout << ", " << field->getCLIShort();
        }

        std::cout << std::endl;
        std::cout << "      " << field->getDescription() << std::endl;

        std::string validators = field->getValidatorDescriptions();
        if (!validators.empty())
        {
            std::cout << "      Validation: " << validators << std::endl;
        }

        if (field->getDefault())
        {
            std::cout << "      Default: " << field->getDefault() << std::endl;
        }
        std::cout << std::endl;
    }
}

bool GlobalConfigManager::load()
{
    std::lock_guard<std::mutex> lock(config_mutex);
    if (!initialized)
    {
        std::cerr << "Config manager not initialized. Call initialize() first." << std::endl;
        return false;
    }

    try
    {
        config_data = YAML::LoadFile(config_file);
        return validateAllUnsafe();
    }
    catch (const YAML::Exception &e)
    {
        std::cerr << "Failed to load config file: " << e.what() << std::endl;
        return false;
    }
}

bool GlobalConfigManager::save() const
{
    std::lock_guard<std::mutex> lock(config_mutex);
    try
    {
        std::ofstream file(config_file);
        file << config_data;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to save config file: " << e.what() << std::endl;
        return false;
    }
}

bool GlobalConfigManager::validateAll()
{
    std::lock_guard<std::mutex> lock(config_mutex);
    return validateAllUnsafe();
}

bool GlobalConfigManager::validateAllUnsafe()
{
    std::vector<std::string> errors;

    // Check required fields
    for (const auto &[key, field] : fields)
    {
        if (field->isRequired() && !config_data[key])
        {
            errors.push_back("Required field '" + key + "' is missing");
        }
    }

    // Validate existing fields
    for (YAML::const_iterator it = config_data.begin(); it != config_data.end(); ++it)
    {
        std::string key = it->first.as<std::string>();
        auto field_it = fields.find(key);

        if (field_it == fields.end())
        {
            std::cerr << "Unknown config field '" << key << "'" << std::endl;
            continue;
        }

        std::string error;
        if (!field_it->second->validate(it->second, error))
        {
            errors.push_back("Field '" + key + "': " + error);
        }
    }

    if (!errors.empty())
    {
        std::cerr << "Configuration validation errors:" << std::endl;
        for (const auto &error : errors)
        {
            std::cerr << "  - " << error << std::endl;
        }
        return false;
    }

    return true;
}

void GlobalConfigManager::printHelp() const
{
    std::lock_guard<std::mutex> lock(config_mutex);
    std::cout << "Configuration Fields:" << std::endl;
    for (const auto &[key, field] : fields)
    {
        std::cout << "  " << key;
        if (field->isRequired())
            std::cout << " (required)";
        std::cout << ": " << field->getDescription() << std::endl;

        std::string validators = field->getValidatorDescriptions();
        if (!validators.empty())
        {
            std::cout << "    Validation: " << validators << std::endl;
        }

        if (field->getDefault())
        {
            std::cout << "    Default: " << field->getDefault() << std::endl;
        }
        std::cout << std::endl;
    }
}

void GlobalConfigManager::createDefaultConfig()
{
    std::lock_guard<std::mutex> lock(config_mutex);
    for (const auto &[key, field] : fields)
    {
        if (field->getDefault())
        {
            config_data[key] = field->getDefault();
        }
    }
}

namespace Config
{
    void initialize(const std::string &filename)
    {
        GlobalConfigManager::getInstance().initialize(filename);
    }

    bool parseArguments(int argc, char *argv[])
    {
        return GlobalConfigManager::getInstance().parseArguments(argc, argv);
    }

    bool load()
    {
        return GlobalConfigManager::getInstance().load();
    }

    bool save()
    {
        return GlobalConfigManager::getInstance().save();
    }

    void createDefault()
    {
        GlobalConfigManager::getInstance().createDefaultConfig();
        save();
    }

    void printHelp()
    {
        GlobalConfigManager::getInstance().printHelp();
    }

    ConfigField &registerField(const std::string &key, const YAML::Node &default_value, bool required)
    {
        return GlobalConfigManager::getInstance().registerField(key, default_value, required);
    }

    /**
     * Setters and getters for server configuration.
     */

    std::string ip() { return get<std::string>("server.ip"); }
    int port() { return get<int>("server.port"); }
    std::string name() { return get<std::string>("server.name"); }
    std::string owner() { return get<std::string>("server.owner"); }
    bool debug() { return get<bool>("server.debug"); } // TODO: verbosity levels? yeah
    std::string authFile() { return get<std::string>("server.auth_file"); }
    std::string banFile() { return get<std::string>("server.ban_file"); }
    std::string motdFile() { return get<std::string>("server.motd_file"); }
    std::string password() { return get<std::string>("server.password"); }
    bool setIP(const std::string &ip) { return set("server.ip", ip); }
    bool setPort(int port) { return set("server.port", port); }
    bool setName(const std::string &name) { return set("server.name", name); }
    bool setOwner(const std::string &owner) { return set("server.owner", owner); }
    bool setDebug(bool debug) { return set("server.debug", debug); }
    bool setAuthFile(const std::string &file) { return set("server.auth_file", file); }
    bool setBanFile(const std::string &file) { return set("server.ban_file", file); }
    bool setMotdFile(const std::string &file) { return set("server.motd_file", file); }
    bool setPassword(const std::string &password) { return set("server.password", password); }

    /**
     * Getters and setters for API configuration.
     */

    std::string apiEndpoint() { return get<std::string>("api.endpoint"); }
    std::string apiKey() { return get<std::string>("api.key"); }
    bool setApiEndpoint(const std::string &endpoint) { return set("api.endpoint", endpoint); }
    bool setApiKey(const std::string &key) { return set("api.key", key); }

    /**
     * Getters and setters for game configuration.
     */

    int maxPlayers() { return get<int>("game.max_players"); }
    std::string terrain() { return get<std::string>("game.terrain"); }
    bool setMaxPlayers(int max) { return set("game.max_players", max); }
    bool setTerrain(const std::string &mode) { return set("game.terrain", mode); }
    // TODO: vehicle whitelist?

    /**
     * If needed, you could also add fields to the configuration
     * in specific namespaces.
     *
     * namespace SomeNamespace
     * {
     *     int someValue() { return get<int>("some_namespace.some_value"); }
     *     bool setSomeValue(int value) { return set("some_namespace.some_value", value); }
     * }
     */
}

// Configuration setup function
void setupConfiguration()
{
    Config::registerField("server.port", YAML::Node(8080))
        .setDescription("Server port number")
        .setCLIArg("--port", "-p")
        .addValidator(std::make_unique<PortValidator>(1024, 65535));

    // Config::registerField("server.name", YAML::Node("MyServer"), true)
    //     .setDescription("Server display name")
    //     .setCLIArg("--name", "-n")
    //     .addValidator(std::make_unique<StringLengthValidator>(3, 50))
    //     .addValidator(std::make_unique<RegexValidator>(R"([a-zA-Z0-9_\-\s]+)", "alphanumeric, spaces, hyphens, underscores only"));

    Config::registerField("server.owner", YAML::Node(), true)
        .setDescription("Server owner name")
        .setCLIArg("--owner", "-o")
        .addValidator(std::make_unique<StringLengthValidator>(2, 100));
}