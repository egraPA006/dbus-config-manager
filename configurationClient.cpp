#include "CLI/CLI.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>

// Constants for D-Bus names and paths
namespace constants
{
const std::string SERVICE_NAME{"com.system.configurationManager"};
const std::string INTERFACE_NAME{
    "com.system.configurationManager.Application.Configuration"};
const std::string CONFIG_CHANGED_SIGNAL{"configurationChanged"};
const std::string DEFAULT_CONFIG_DIR{"~/com.system.configurationManager/"};
const std::string DEFAULT_APP_NAME{"confManagerApplication1"};
}

namespace fs = std::filesystem;
using json = nlohmann::json;

static void initialize_logging()
{
    auto logger = spdlog::stdout_color_mt("configuration_client");
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

// Helper function to expand home directory path
std::string expandHomeDirectory(const std::string& path)
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

class ClientApplication
{
  public:
    ClientApplication() : ClientApplication(1000, "Hey", true) {}

    ClientApplication(int64_t timeout, const std::string& timeoutPhrase)
        : ClientApplication(timeout, timeoutPhrase, true)
    {
    }
    
    ClientApplication(int64_t timeout, const std::string& timeoutPhrase,
                     const std::string& customConfigPath)
        : ClientApplication(timeout, timeoutPhrase, true, customConfigPath)
    {
    }

    ~ClientApplication() { stop(); }

    void run() { connection->enterEventLoop(); }

  private:
    ClientApplication(int64_t timeout, const std::string& timeoutPhrase,
                     bool forceCreate, const std::string& customConfigPath = "")
        : timeout(timeout), timeoutPhrase(timeoutPhrase),
          forceCreateConf(forceCreate)
    {
        // Set config path
        if (customConfigPath.empty())
        {
            configPath = expandHomeDirectory(constants::DEFAULT_CONFIG_DIR) +
                        constants::DEFAULT_APP_NAME + ".json";
        }
        else
        {
            configPath = customConfigPath;
        }
        
        spdlog::debug("Using configuration path: {}", configPath);
        initialize();
    }

    void initialize()
    {
        try
        {
            connection = sdbus::createSessionBusConnection();
            if (!connection)
            {
                throw std::runtime_error("Failed to create D-Bus connection");
            }
            spdlog::debug("D-Bus session connection created successfully");
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to create D-Bus connection: {}", e.what());
            throw;
        }
        loadConfiguration();
        setupDBusProxy();
        startTimeoutThread();
    }

    void stop()
    {
        running = false;
        if (timeoutThread.joinable())
        {
            timeoutThread.join();
        }
    }

    void loadConfiguration()
    {
        try
        {
            spdlog::debug(
                "Loading configuration with default timeout {} and phrase {}",
                timeout, timeoutPhrase);
            if (!fs::exists(configPath))
            {
                fs::create_directories(fs::path(configPath).parent_path());
            }

            if (!fs::exists(configPath) || forceCreateConf)
            {
                createConfig();
                spdlog::info("Created configuration: Timeout={}ms, Phrase='{}'",
                             timeout, timeoutPhrase);
                return;
            }

            std::ifstream configFile(configPath);
            json config = json::parse(configFile);

            {
                std::lock_guard<std::mutex> lock(configMutex);
                timeout = config["Timeout"].get<int64_t>();
                timeoutPhrase = config["TimeoutPhrase"].get<std::string>();
            }

            spdlog::info("Loaded configuration: Timeout={}ms, Phrase='{}'",
                         timeout, timeoutPhrase);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to load configuration: {}", e.what());
            throw;
        }
    }

    void createConfig()
    {
        std::ofstream configFile(configPath);
        json config = {{"Timeout", timeout}, {"TimeoutPhrase", timeoutPhrase}};
        configFile << config.dump(4);
    }

    void setupDBusProxy()
    {
        try
        {
            // Extract the application name from config path
            fs::path configFilePath(configPath);
            std::string appName = configFilePath.stem().string();
            
            // Build the object path
            std::string objectPath = buildObjectPath(appName);
            spdlog::debug("Using D-Bus object path: {}", objectPath);
            
            proxy = sdbus::createProxy(
                *connection,
                static_cast<sdbus::ServiceName>(constants::SERVICE_NAME),
                static_cast<sdbus::ObjectPath>(objectPath));

            proxy->uponSignal(constants::CONFIG_CHANGED_SIGNAL)
                .onInterface(constants::INTERFACE_NAME)
                .call([this](const std::map<std::string, sdbus::Variant>& newConfig)
                      { this->handleConfigurationChange(newConfig); });
                      
            // Check connection is active
            if (!proxy->isConnected())
            {
                throw std::runtime_error("Failed to connect to D-Bus service");
            }
            
            spdlog::debug("D-Bus proxy setup successfully");
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to set up D-Bus proxy: {}", e.what());
            throw;
        }
    }
    
    std::string buildObjectPath(const std::string& appName) const
    {
        std::string path = "/" + constants::SERVICE_NAME;
        std::replace(path.begin(), path.end(), '.', '/');
        return path + "/Application/" + appName;
    }

    void handleConfigurationChange(
        const std::map<std::string, sdbus::Variant>& newConfig)
    {
        try
        {
            spdlog::info("Configuration change received");
            spdlog::debug("New configuration size: {}, first key: {}",
                          newConfig.size(), newConfig.begin()->first);
            {
                std::lock_guard<std::mutex> lock(configMutex);
                if (newConfig.count("Timeout"))
                {
                    try
                    {
                        timeout = newConfig.at("Timeout").get<int64_t>();
                        spdlog::debug("Updated Timeout to {}", timeout);
                    }
                    catch (const std::exception& e)
                    {
                        spdlog::error("Failed to get Timeout: {}", e.what());
                    }
                }
                if (newConfig.count("TimeoutPhrase"))
                {
                    try
                    {
                        timeoutPhrase =
                            newConfig.at("TimeoutPhrase").get<std::string>();
                        spdlog::debug("Updated TimeoutPhrase to '{}'",
                                      timeoutPhrase);
                    }
                    catch (const std::exception& e)
                    {
                        spdlog::error("Failed to get TimeoutPhrase: {}",
                                      e.what());
                    }
                }
            }

            spdlog::info("New configuration applied: Timeout={}ms, Phrase='{}'",
                         timeout, timeoutPhrase);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to apply new configuration: {}", e.what());
        }
    }

    void startTimeoutThread()
    {
        timeoutThread = std::thread(
            [this]()
            {
                while (running)
                {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(getCurrentTimeout()));
                    if (!running)
                        break;
                    std::cout << getCurrentPhrase() << std::endl;
                }
            });
    }

    int64_t getCurrentTimeout()
    {
        std::lock_guard<std::mutex> lock(configMutex);
        return timeout;
    }

    std::string getCurrentPhrase()
    {
        std::lock_guard<std::mutex> lock(configMutex);
        return timeoutPhrase;
    }

    // Configuration
    std::string configPath;
    int64_t timeout;
    std::string timeoutPhrase;
    std::mutex configMutex;
    bool forceCreateConf;

    // D-Bus
    std::unique_ptr<sdbus::IConnection> connection;
    std::unique_ptr<sdbus::IProxy> proxy;

    // Threading
    std::atomic<bool> running{true};
    std::thread timeoutThread;
};

int main(int argc, char* argv[])
{
    initialize_logging();

    try
    {
        int64_t timeout = 1000;
        std::string phrase = "Hey";
        std::string configPath;
        bool verbose = false;
        bool createNewConfig = false;

        CLI::App app{"Configuration Client Application"};
        app.add_option("--timeout", timeout, "Timeout in milliseconds")
            ->check(CLI::PositiveNumber)
            ->default_val(1000);

        app.add_option("--phrase", phrase, "Timeout message")
            ->default_val("Hey");
            
        app.add_option("--config-path", configPath,
                      "Path to configuration file (default: ~/com.system.configurationManager/confManagerApplication1.json)");

        app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
        
        app.add_flag("--create-config", createNewConfig,
                    "Force creation of a new configuration file");

        CLI11_PARSE(app, argc, argv);

        if (verbose)
        {
            spdlog::set_level(spdlog::level::debug);
            spdlog::debug("Verbose logging enabled");
        }

        spdlog::info(
            "Starting with configuration - timeout: {}ms, phrase: '{}'",
            timeout, phrase);

        ClientApplication client_app = configPath.empty()
            ? ClientApplication(timeout, phrase, createNewConfig)
            : ClientApplication(timeout, phrase, configPath);
        client_app.run();
    }
    catch (const std::exception& e)
    {
        spdlog::critical("Application failed: {}", e.what());
        return 1;
    }
    return 0;
}