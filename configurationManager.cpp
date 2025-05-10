#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <algorithm>
#include <unordered_set>

class ApplicationConfiguration {
public:
    ApplicationConfiguration(sdbus::IConnection& connection, const std::string& objectPath,
         const std::string& configPath) {
        
    }
private:
    void parseConfig(const std::string& configPath) {

    }
    void changeConfiguration(const std::string& key, sdbus::Variant) {

    }
};

class ConfigurationManager {
public:
    static ConfigurationManager& get_instance() {
        // Having a singleton here makes sence - one manager per session == nice
        static ConfigurationManager instance;
        return instance;
    }
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;
private:
    ConfigurationManager() {
        connection = sdbus::createSessionBusConnection(serviceName);

        applicationObjectPath = serviceName;
        std::replace(applicationObjectPath.begin(), applicationObjectPath.end(), '.', '/');
        applicationObjectPath += "/Application/";
        // auto root_obj = sdbus::createObject(*connection, objectPath); // Not needed yet
        try {
            std::vector<std::pair<std::string, std::string>> applicationData = getApplicationConfigs();
        } catch (const std::exception& e) {
            throw std::runtime_error("Configuration init failed: " + std::string(e.what()));
        }
        

    }
    /**
     * @return vector<path to app config json, name of config json>
     * @throws runtime error if could not access config directory
     * @note if more than one application has same name, peaks one random
     */
    std::vector<std::pair<std::string, std::string>> getApplicationConfigs() {
        std::vector<std::pair<std::string, std::string>> applicationData;
        std::unordered_set<std::string> uniqueNames;      
        if (configDir.find("~/") == 0) {
            configDir.replace(0, 1, std::getenv("HOME"));
        }
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(configDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    
                    applicationData.emplace_back(
                        entry.path().string(),
                        entry.path().filename().string()
                    );
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error( "Error accessing config directory: " + std::string(e.what()));
        }        
        return applicationData;
    }
    std::string configDir = "~/com.system.configurationManager/";
    const std::string serviceName = "com.system.configurationManager";
    std::string applicationObjectPath;
    std::unique_ptr<sdbus::IConnection> connection;
};

int main() {
}