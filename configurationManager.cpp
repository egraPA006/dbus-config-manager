#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <nlohmann/json.hpp>

using config_dict = std::map<std::string, sdbus::Variant>;
using json = nlohmann::json;
namespace fs = std::filesystem;


// TODO: add error handling
// TODO: add logs
class ApplicationConfiguration {
public:
    ApplicationConfiguration(sdbus::IConnection& connection, const sdbus::ObjectPath& objectPath,
         const std::string& configPath, const sdbus::InterfaceName interfaceName) :
         interfaceName(interfaceName) {
        parseConfig(configPath);
        object = sdbus::createObject(connection, objectPath);
        registerMethods();
    }
    void emitConfigurationChanged() {
        object->emitSignal("configurationChanged")
            .onInterface("com.system.configurationManager.Application.Configuration")
            .withArguments(configuration); 
    }
private:
    // TODO: implement
    void parseConfig(const std::string& configPath) {
        json configJson;
    }
    void changeConfiguration(const std::string& key, sdbus::Variant val) {
        configuration[key] = val;
        // TODO: add dbus error
    }
    config_dict getConfiguration() {
        return configuration;
    }
    void registerMethods() {
        object->addVTable(
            sdbus::registerMethod("GetConfiguration").implementedAs([this]() { return this->getConfiguration(); }))
            .forInterface(interfaceName);
         object->addVTable(
            sdbus::registerMethod("ChangeConfiguration").implementedAs(
                [this](const std::string& key, sdbus::Variant val) { return this->changeConfiguration(key, val); }
            ),
            sdbus::registerSignal("configurationChanged").withParameters<config_dict>()
            )
            .forInterface(interfaceName);
    }
    std::unique_ptr<sdbus::IObject> object;
    config_dict configuration;
    sdbus::InterfaceName interfaceName;
    
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

    std::vector<std::string> getApplicationsNames() {
        std::vector<std::string> appNames;
        for (const auto& el : applicationsConfiguration) {
            appNames.push_back(el.first);
        }
        return appNames;
    }
private:
    ConfigurationManager() {
        std::vector<std::pair<std::string, std::string>> applicationsData;
        std::string applicationObjectPath;
        connection = sdbus::createSessionBusConnection(serviceName);
        applicationsObjectPath = serviceName;
        std::replace(applicationsObjectPath.begin(), applicationsObjectPath.end(), '.', '/');
        applicationsObjectPath += "/Application/";        
        try {
            applicationsData = getApplicationsConfigs();
        } catch (const std::exception& e) {
            throw std::runtime_error("Configuration init failed: " + std::string(e.what()));
        }
        for (const auto& [path, name] : applicationsData)  {
            applicationObjectPath = applicationsObjectPath + name;
            applicationsConfiguration[name] = std::make_unique<ApplicationConfiguration>(
                *connection, static_cast<sdbus::ObjectPath>(applicationObjectPath), path, interfaceName
            );
        }
    }
    /**
     * @return vector<path to app config json, name of config json>
     * @throws runtime error if could not access config directory
     * @note if more than one application has same name, peaks one random
     */
    std::vector<std::pair<std::string, std::string>> getApplicationsConfigs() {
        std::vector<std::pair<std::string, std::string>> applicationsData;
        std::unordered_set<std::string> uniqueNames;
        std::string filename;
        size_t unique_size;
        if (configDir.find("~/") == 0) {
            configDir.replace(0, 1, std::getenv("HOME"));
        }
        try {
            for (const auto& entry : fs::recursive_directory_iterator(configDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    filename = entry.path().filename().string();
                    unique_size = uniqueNames.size();
                    uniqueNames.insert(filename);
                    if (uniqueNames.size() == unique_size) {
                        continue; // yep, just skip duplicates
                    }
                    applicationsData.emplace_back(
                        entry.path().string(),
                        filename
                    );
                }
            }
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error( "Error accessing config directory: " + std::string(e.what()));
        }        
        return applicationsData;
    }
    std::string configDir{"~/com.system.configurationManager/"};
    const sdbus::ServiceName serviceName{"com.system.configurationManager"};
    const sdbus::InterfaceName interfaceName{"com.system.configurationManager.Application.Configuration"};
    std::string applicationsObjectPath;
    std::unique_ptr<sdbus::IConnection> connection;
    std::unordered_map<std::string, std::unique_ptr<ApplicationConfiguration>> applicationsConfiguration;
};

int main() {
}