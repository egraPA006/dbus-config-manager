#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <nlohmann/json.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <systemd/sd-bus.h>
#include <unordered_set>
#include <vector>
#include "CLI/CLI.hpp"

using config_dict = std::map<std::string, sdbus::Variant>;
namespace nlohmann
{
template <> struct adl_serializer<sdbus::Variant>
{
    static void to_json(json& j, const sdbus::Variant& v)
    {
        // Not needed for parsing
        throw std::runtime_error("Variant to JSON not implemented");
    }

    static void from_json(const json& j, sdbus::Variant& v)
    {
        if (j.is_string())
            v = sdbus::Variant(j.get<std::string>());
        else if (j.is_number_integer())
            v = sdbus::Variant(j.get<int64_t>());
        else if (j.is_number_float())
            v = sdbus::Variant(j.get<double>());
        else if (j.is_boolean())
            v = sdbus::Variant(j.get<bool>());
        else
            throw json::type_error::create(
                302, "Unsupported type for variant conversion", &j);
    }
};
} // namespace nlohmann
using json = nlohmann::json;
namespace fs = std::filesystem;

// Constants for D-Bus names and paths
namespace constants
{
const std::string CONFIG_DIR_PATH{"~/com.system.configurationManager/"};
const std::string SERVICE_NAME{"com.system.configurationManager"};
const std::string INTERFACE_NAME{
    "com.system.configurationManager.Application.Configuration"};
const std::string CONFIG_CHANGED_SIGNAL{"configurationChanged"};
}

static void initialize_logging()
{
    auto logger = spdlog::stdout_color_mt("config_manager");
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

class ApplicationConfiguration
{
  public:
    ApplicationConfiguration(sdbus::IConnection& connection,
                             const sdbus::ObjectPath& objectPath,
                             const std::string& configPath,
                             const sdbus::InterfaceName& interfaceName)
        : interfaceName(interfaceName), configPath(configPath)
    {
        try
        {
            spdlog::debug("Creating ApplicationConfiguration for {}",
                          configPath);
            parseConfig();
            object = sdbus::createObject(connection, objectPath);
            registerMethods();
            spdlog::info("Successfully created ApplicationConfiguration for {}",
                         configPath);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to create ApplicationConfiguration for {}",
                          configPath);
            throw std::runtime_error(
                "Failed to create ApplicationConfiguration: " +
                std::string(e.what()));
        }
    }

    void emitConfigurationChanged()
    {
        if (!object)
        {
            throw std::runtime_error("D-Bus object not initialized");
        }
        try
        {
            object->emitSignal("configurationChanged")
                .onInterface(interfaceName)
                .withArguments(configuration);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(
                "Failed to emit configurationChanged signal: " +
                std::string(e.what()));
        }
    }

    ~ApplicationConfiguration()
    {
        if (object)
        {
            object->unregister();
        }
    }

  private:
    void parseConfig()
    {
        try
        {
            spdlog::debug("Parsing config file: {}", configPath);
            std::ifstream configFile(configPath);
            if (!configFile)
            {
                spdlog::error("Could not open config file: {}", configPath);
                throw std::runtime_error("Could not open config file");
            }
            configuration = json::parse(configFile).get<config_dict>();
            spdlog::info("Successfully parsed config file: {}", configPath);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to parse config file: {}", configPath);
            throw std::runtime_error("Config parsing failed: " +
                                     std::string(e.what()));
        }
    }

    void changeConfiguration(const std::string& key, const sdbus::Variant& val)
    {
        spdlog::debug("Changing configuration key: {}", key);
        if (key.empty())
        {
            throw std::invalid_argument("Key cannot be empty");
        }
        if (val.isEmpty())
        {
            throw std::invalid_argument("Value cannot be empty");
        }

        {
            std::lock_guard<std::mutex> lock(configMutex);
            configuration[key] = val;
        }
        emitConfigurationChanged();
        // Save changes back to JSON
        saveConfigToFile();

        spdlog::info("Configuration changed for key: {}", key);
    }

    void saveConfigToFile()
    {
        try
        {
            json config;
            {
                std::lock_guard<std::mutex> lock(configMutex);
                for (const auto& [key, value] : configuration)
                {
                    if (value.containsValueOfType<std::string>())
                        config[key] = value.get<std::string>();
                    else if (value.containsValueOfType<int64_t>())
                        config[key] = value.get<int64_t>();
                    else if (value.containsValueOfType<double>())
                        config[key] = value.get<double>();
                    else if (value.containsValueOfType<bool>())
                        config[key] = value.get<bool>();
                }
            }
            
            std::ofstream file(configPath);
            if (!file)
            {
                spdlog::error("Failed to open config file for writing: {}",
                            configPath);
                return;
            }
            file << config.dump(4);
            spdlog::debug("Configuration saved to file: {}", configPath);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to save configuration: {}", e.what());
        }
    }

    config_dict getConfiguration() const
    {
        std::lock_guard<std::mutex> lock(configMutex);
        return configuration;
    }

    void registerMethods()
    {
        if (!object)
        {
            throw std::runtime_error("D-Bus object not initialized");
        }

        object
            ->addVTable(sdbus::registerMethod("GetConfiguration")
                            .implementedAs(
                                [this]() { return this->getConfiguration(); }))
            .forInterface(interfaceName);

        object
            ->addVTable(
                sdbus::registerMethod("ChangeConfiguration")
                    .implementedAs([this](const std::string& key,
                                          const sdbus::Variant& val)
                                   { this->changeConfiguration(key, val); }),
                sdbus::registerSignal("configurationChanged")
                    .withParameters<config_dict>())
            .forInterface(interfaceName);
    }

    std::unique_ptr<sdbus::IObject> object;
    config_dict configuration;
    sdbus::InterfaceName interfaceName;
    std::string configPath;
    mutable std::mutex configMutex;
};

class ConfigurationManager
{
  public:
    static ConfigurationManager& getInstance(const std::string& customConfigDir = "")
    {
        static std::once_flag initInstanceFlag;
        static ConfigurationManager* instance = nullptr;
        
        std::call_once(initInstanceFlag, [&customConfigDir]() {
            instance = new ConfigurationManager(customConfigDir);
        });
        
        return *instance;
    }

    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    std::vector<std::string> getApplicationNames() const
    {
        std::vector<std::string> names;
        names.reserve(applicationsConfiguration.size());

        for (const auto& [name, _] : applicationsConfiguration)
        {
            names.push_back(name);
        }

        return names;
    }
    
    void setConfigDir(const std::string& dir)
    {
        if (!dir.empty())
        {
            configDir = dir;
        }
    }

    void run()
    {
        if (!connection)
        {
            throw std::runtime_error("D-Bus connection not initialized");
        }
        connection->enterEventLoopAsync();
    }

    void stop()
    {
        if (connection)
        {
            connection->leaveEventLoop();
        }
    }

  private:
    ConfigurationManager(const std::string& customConfigDir = "")
    {
        try
        {
            spdlog::debug("Initializing ConfigurationManager");
            if (!customConfigDir.empty())
            {
                configDir = customConfigDir;
            }
            initialize();
            spdlog::info("ConfigurationManager initialized successfully");
        }
        catch (const std::exception& e)
        {
            spdlog::critical("ConfigurationManager initialization failed");
            throw std::runtime_error(
                "ConfigurationManager initialization failed: " +
                std::string(e.what()));
        }
    }

    ~ConfigurationManager()
    {
        if (connection)
        {
            connection->releaseName(serviceName);
        }
    }

    void initialize()
    {
        spdlog::debug("Creating D-Bus connection");
        connection = sdbus::createSessionBusConnection(
            static_cast<sdbus::ServiceName>(constants::SERVICE_NAME));

        auto applicationsData = getApplicationsConfigs();
        spdlog::info("Found {} application configs", applicationsData.size());
        
        for (const auto& [path, name] : applicationsData)
        {
            std::string applicationObjectPath = buildApplicationsObjectPath(name);
            applicationsConfiguration[name] =
                std::make_unique<ApplicationConfiguration>(
                    *connection,
                    static_cast<sdbus::ObjectPath>(applicationObjectPath),
                    path,
                    static_cast<sdbus::InterfaceName>(constants::INTERFACE_NAME));
        }
    }

    // Expands home directory in path (~/path to /home/user/path)
    std::string expandHomeDirectory(const std::string& path) const
    {
        if (path.empty() || path.find("~/") != 0)
        {
            return path;
        }
        
        const char* home = std::getenv("HOME");
        if (!home)
        {
            throw std::runtime_error("HOME environment variable not set");
        }
        
        return std::string(home) + path.substr(1);
    }

    std::vector<std::pair<std::string, std::string>>
    getApplicationsConfigs() const
    {
        spdlog::debug("Scanning config directory: {}", configDir);
        std::vector<std::pair<std::string, std::string>> applicationsData;
        std::string actualConfigDir = expandHomeDirectory(configDir);
        try
        {
            for (const auto& entry : fs::directory_iterator(actualConfigDir))
            {
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".json")
                {
                    applicationsData.emplace_back(entry.path().string(),
                                                  entry.path().stem().string());
                }
            }
        }
        catch (const fs::filesystem_error& e)
        {
            throw std::runtime_error("Error accessing config directory: " +
                                     std::string(e.what()));
        }

        if (applicationsData.empty())
        {
            throw std::runtime_error("No valid configuration files found in " +
                                     actualConfigDir);
        }
        spdlog::debug("Found {} valid config files", applicationsData.size());
        return applicationsData;
    }

    std::string buildApplicationsObjectPath(const std::string& appName) const
    {
        std::string path = "/" + constants::SERVICE_NAME;
        std::replace(path.begin(), path.end(), '.', '/');
        return path + "/Application/" + appName;
    }

    std::string configDir{constants::CONFIG_DIR_PATH};
    const sdbus::InterfaceName interfaceName{constants::INTERFACE_NAME};
    std::unique_ptr<sdbus::IConnection> connection;
    std::unordered_map<std::string, std::unique_ptr<ApplicationConfiguration>>
        applicationsConfiguration;
    static inline std::condition_variable shutdownCV;
    static inline std::mutex shutdownMutex;
    static inline bool shouldShutdown = false;
};

// Signal handler
void signalHandler(int signal)
{
    spdlog::info("Received signal: {}", signal);
    
    // Notify the waiting condition variable
    {
        std::lock_guard<std::mutex> lock(ConfigurationManager::shutdownMutex);
        ConfigurationManager::shouldShutdown = true;
    }
    ConfigurationManager::shutdownCV.notify_all();
    
    // Also stop the manager
    try {
        ConfigurationManager::getInstance().stop();
    } catch (const std::exception& e) {
        spdlog::error("Error stopping manager: {}", e.what());
    }
}

int main(int argc, char* argv[])
{
    initialize_logging();
    
    // Parse command line arguments
    std::string configDir;
    bool verbose = false;
    
    CLI::App app{"D-Bus Configuration Manager Service"};
    app.add_option("--config-dir", configDir, "Configuration directory")
        ->default_val(constants::CONFIG_DIR_PATH);
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }
    
    if (verbose)
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Verbose logging enabled");
    }
    
    try
    {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        spdlog::info("Starting ConfigurationManager with config dir: {}", configDir);
        auto& manager = ConfigurationManager::getInstance(configDir);
        manager.run();
        spdlog::info("ConfigurationManager running");
        
        // Wait for shutdown signal
        std::unique_lock<std::mutex> lock(ConfigurationManager::shutdownMutex);
        ConfigurationManager::shutdownCV.wait(lock, []{
            return ConfigurationManager::shouldShutdown;
        });
        
        spdlog::info("Shutting down ConfigurationManager");
    }
    catch (const std::exception& e)
    {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}