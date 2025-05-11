# D-Bus Configuration Manager

## Description
A lightweight D-Bus service that enables **runtime configuration** of applications via JSON files. The manager:
- Scans `~/com.system.configurationManager/` for JSON config files (flat key-value structure)
- Exposes each application's configuration via D-Bus at:
  - Bus: `com.system.configurationManager`
  - Path: `/com/system/configurationManager/Application/{applicationName}`
  - Interface: `com.system.configurationManager.Application.Configuration`

### Available Methods
- `ChangeConfiguration(key: string, value: variant)` - Modify a single setting
- `GetConfiguration()` → `map<string,variant>` - Retrieve all current settings

**Example**: Includes a demo client application that prints configurable messages at adjustable intervals.

## Technology Stack
- **Core**: C++17
- **Build**: CMake
- **Dependencies**:
  - [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) (D-Bus integration)
  - [spdlog](https://github.com/gabime/spdlog) (logging)
  - [nlohmann/json](https://github.com/nlohmann/json) (configuration parsing)
  - [CLI11](https://github.com/CLIUtils/CLI11) (command-line interface)
- **Formatting**: clang-format

## Installation

### Prerequisites
| Requirement              | Minimum Version | Notes                          |
|--------------------------|-----------------|--------------------------------|
| Linux (Ubuntu recommended) | -               | Tested on 24.04 LTS            |
| CMake                    | 3.14            | Build system                   |
| C++ Compiler (GCC/Clang) | C++17 support   | GCC ≥ 7, Clang ≥ 5             |
| Git                      | -               | Dependency fetching            |

**Optional**:
- `libsystemd-dev` (preferred) **OR** Meson (fallback builder)

### Building
```bash
# Standard build
./build

# Additional options:
./build --format  # Apply clang-formatting
./build --help    # Show all options
```

## Usage

### Quick Demo
```bash
./demo.sh  # Interactive guided demonstration
```

### Manual Operation
1. Start the demo client:
   ```bash
   ./bin/client --help  # See available options
   ```
2. Start the manager:
   ```bash
   ./bin/manager
   ```

### Direct D-Bus Interaction
Use `gdbus` for manual configuration:

```bash
# Change settings
gdbus call -e -d com.system.configurationManager \
  -o /com/system/configurationManager/Application/confManagerApplication1 \
  -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
  "TimeoutPhrase" "<'New message text'>"

gdbus call -e -d com.system.configurationManager \
  -o /com/system/configurationManager/Application/confManagerApplication1 \
  -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
  "Timeout" "<int64 1000>"

# Retrieve current config
gdbus call -e -d com.system.configurationManager \
  -o /com/system/configurationManager/Application/confManagerApplication1 \
  -m com.system.configurationManager.Application.Configuration.GetConfiguration
```

## Troubleshooting
- If builds fail, check missing dependencies via CMake error messages
- Ensure D-Bus session bus is running (`dbus-run-session` may help in containers)