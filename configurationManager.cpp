#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <systemd/sd-bus.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using config_dict = std::map<std::string, sdbus::Variant>;
namespace nlohmann {
    template <>
    struct adl_serializer<sdbus::Variant> {
        static void to_json(json& j, const sdbus::Variant& v) {
            // Not needed for parsing
            throw std::runtime_error("Variant to JSON not implemented");
        }

        static void from_json(const json& j, sdbus::Variant& v) {
            if (j.is_string()) v = sdbus::Variant(j.get<std::string>());
            else if (j.is_number_integer()) v = sdbus::Variant(j.get<int64_t>());
            else if (j.is_number_float()) v = sdbus::Variant(j.get<double>());
            else if (j.is_boolean()) v = sdbus::Variant(j.get<bool>());
            else throw json::type_error::create(302, "Unsupported type for variant conversion", &j);
        }
    };
}
using json = nlohmann::json;
namespace fs = std::filesystem;

static void initialize_logging() {
    auto logger = spdlog::stdout_color_mt("config_manager");
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

class ApplicationConfiguration {
public:
    ApplicationConfiguration(sdbus::IConnection& connection, 
                           const sdbus::ObjectPath& objectPath,
                           const std::string& configPath, 
                           const sdbus::InterfaceName& interfaceName) 
        : interfaceName(interfaceName), 
          configPath(configPath) 
    {
        try {
            spdlog::debug("Creating ApplicationConfiguration for {}", configPath);
            parseConfig();
            object = sdbus::createObject(connection, objectPath);
            registerMethods();
            spdlog::info("Successfully created ApplicationConfiguration for {}", configPath);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create ApplicationConfiguration for {}", configPath);
            throw std::runtime_error("Failed to create ApplicationConfiguration: " + std::string(e.what()));
        }
    }

    void emitConfigurationChanged() {
        if (!object) {
            throw std::runtime_error("D-Bus object not initialized");
        }
        try {
            object->emitSignal("configurationChanged")
                .onInterface(interfaceName)
                .withArguments(configuration);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to emit configurationChanged signal: " + std::string(e.what()));
        }
    }

    ~ApplicationConfiguration() {
        if (object) {
            object->unregister();
        }
    }
private:
    void parseConfig() {
        try {
            spdlog::debug("Parsing config file: {}", configPath);
            std::ifstream configFile(configPath);
            if (!configFile) {
                spdlog::error("Could not open config file: {}", configPath);
                throw std::runtime_error("Could not open config file");
            }
            configuration = json::parse(configFile).get<config_dict>();
            spdlog::info("Successfully parsed config file: {}", configPath);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse config file: {}", configPath);
            throw std::runtime_error("Config parsing failed: " + std::string(e.what()));
        }
    }

    void changeConfiguration(const std::string& key, const sdbus::Variant& val) {
        spdlog::debug("Changing configuration key: {}", key);
        if (key.empty()) {
            throw std::invalid_argument("Key cannot be empty");
        }
        if (val.isEmpty()) {
            throw std::invalid_argument("Value cannot be empty");
        }

        configuration[key] = val;
        emitConfigurationChanged();
        // NOTE: Maybe we should save changes back to json?

        spdlog::info("Configuration changed for key: {}", key);
    }

    config_dict getConfiguration() const {
        return configuration;
    }

    void registerMethods() {
        if (!object) {
            throw std::runtime_error("D-Bus object not initialized");
        }

        object->addVTable(
            sdbus::registerMethod("GetConfiguration")
                .implementedAs([this]() { return this->getConfiguration(); })
        ).forInterface(interfaceName);

        object->addVTable(
            sdbus::registerMethod("ChangeConfiguration")
                .implementedAs([this](const std::string& key, const sdbus::Variant& val) { 
                    this->changeConfiguration(key, val); 
                }),
            sdbus::registerSignal("configurationChanged")
                .withParameters<config_dict>()
        ).forInterface(interfaceName);
    }

    std::unique_ptr<sdbus::IObject> object;
    config_dict configuration;
    sdbus::InterfaceName interfaceName;
    std::string configPath;
};

class ConfigurationManager {
public:
    static ConfigurationManager& getInstance() {
        static ConfigurationManager instance;
        return instance;
    }

    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    std::vector<std::string> getApplicationNames() const {
        std::vector<std::string> names;
        names.reserve(applicationsConfiguration.size());
        
        for (const auto& [name, _] : applicationsConfiguration) {
            names.push_back(name);
        }
        
        return names;
    }

    void run() {
        if (!connection) {
            throw std::runtime_error("D-Bus connection not initialized");
        }
        connection->enterEventLoopAsync();
    }

    void stop() {
        if (connection) {
            connection->leaveEventLoop();
        }
    }

private:
    ConfigurationManager() {
        try {
            spdlog::debug("Initializing ConfigurationManager");
            initialize();
            spdlog::info("ConfigurationManager initialized successfully");
        } catch (const std::exception& e) {
            spdlog::critical("ConfigurationManager initialization failed");
            throw std::runtime_error("ConfigurationManager initialization failed: " + std::string(e.what()));
        }
    }

    ~ConfigurationManager() {
        if (connection) {
            connection->releaseName(serviceName);
        }
    }

    void initialize() {
        spdlog::debug("Creating D-Bus connection");
        connection = sdbus::createSessionBusConnection(serviceName);
        
        auto applicationsData = getApplicationsConfigs();
        spdlog::info("Found {} application configs", applicationsData.size());
        std::string applicationObjectPath = buildApplicationsObjectPath();
        for (const auto& [path, name] : applicationsData) {
            applicationObjectPath += name;
            applicationsConfiguration[name] = std::make_unique<ApplicationConfiguration>(
                *connection, 
                static_cast<sdbus::ObjectPath>(applicationObjectPath), 
                path, 
                interfaceName
            );
        }
    }

    std::vector<std::pair<std::string, std::string>> getApplicationsConfigs() const {
        spdlog::debug("Scanning config directory: {}", configDir);
        std::vector<std::pair<std::string, std::string>> applicationsData;
        std::string actualConfigDir = configDir;
        if (actualConfigDir.find("~/") == 0) { // Maybe too much but why not ^_^
            const char* home = std::getenv("HOME");
            if (!home) {
                throw std::runtime_error("HOME environment variable not set");
            }
            actualConfigDir.replace(0, 1, home);
        }
        try {
            for (const auto& entry : fs::directory_iterator(actualConfigDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    applicationsData.emplace_back(
                        entry.path().string(),
                        entry.path().stem().string()
                    );
                }
            }
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Error accessing config directory: " + std::string(e.what()));
        }
        
        if (applicationsData.empty()) {
            throw std::runtime_error("No valid configuration files found in " + actualConfigDir);
        }
        spdlog::debug("Found {} valid config files", applicationsData.size());
        return applicationsData;
    }

    std::string buildApplicationsObjectPath() const {
        std::string path = "/" + serviceName;
        std::replace(path.begin(), path.end(), '.', '/');
        return path + "/Application/";
    }

    const std::string configDir{"~/com.system.configurationManager/"};
    const sdbus::ServiceName serviceName{"com.system.configurationManager"};
    const sdbus::InterfaceName interfaceName{"com.system.configurationManager.Application.Configuration"};
    std::unique_ptr<sdbus::IConnection> connection;
    std::unordered_map<std::string, std::unique_ptr<ApplicationConfiguration>> applicationsConfiguration;
};

int main() {
    initialize_logging();
    try {
        spdlog::info("Starting ConfigurationManager");
        auto& manager = ConfigurationManager::getInstance();
        manager.run();
        spdlog::info("ConfigurationManager running");
        while(1) {}
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}