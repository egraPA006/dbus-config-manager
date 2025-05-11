cmake_minimum_required(VERSION 3.14)
project(Dbus-config-manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable more warnings and treat them as errors
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Collect all source files for formatting
set(ALL_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/configurationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configurationClient.cpp
)

find_package(sdbus-c++ 2.0.0 QUIET)
find_package(spdlog QUIET)
find_package(nlohmann_json 3.12.0 QUIET)
find_package(CLI11 2.5.0 QUIET)
find_library(SYSTEMD_LIB 
    NAMES systemd
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
    QUIET
    NO_DEFAULT_PATH
)

set(SYSTEMD_INSTALL_REQUIRED OFF)
if(NOT SYSTEMD_LIB AND sdbus-c++_FOUND)
    message(FATAL_ERROR "libsystemd not found - install systemd development package")
elseif(NOT SYSTEMD_LIB)
    set(SYSTEMD_INSTALL_REQUIRED ON)
endif()

if(NOT CLI11_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.5.0
    )
    FetchContent_MakeAvailable(CLI11)
endif()

if(NOT sdbus-c++_FOUND)
    # Did not work automatically :(
    include(FetchContent)
    FetchContent_Declare(
        sdbus-cpp
        GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
        GIT_TAG v2.1.0
    )
    FetchContent_GetProperties(sdbus-cpp)
    if(NOT sdbus-cpp_POPULATED)
        FetchContent_Populate(sdbus-cpp)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
                    -DCMAKE_BUILD_TYPE=Release
                    -DBUILD_SHARED_LIBS=OFF
                    -DSDBUSCPP_BUILD_DOCS=OFF
                    -DSDBUSCPP_BUILD_LIBSYSTEMD=${SYSTEMD_INSTALL_REQUIRED}
                    ${sdbus-cpp_SOURCE_DIR}
            WORKING_DIRECTORY ${sdbus-cpp_BINARY_DIR}
            RESULT_VARIABLE configure_result
        )
        if(NOT configure_result EQUAL 0)
            message(FATAL_ERROR "Configuration failed!")
        endif()
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build .
            WORKING_DIRECTORY ${sdbus-cpp_BINARY_DIR}
            RESULT_VARIABLE build_result
        )
        if(NOT build_result EQUAL 0)
            message(FATAL_ERROR "Build failed!")
        endif()
    endif()

    if(NOT TARGET sdbus-cpp AND EXISTS "${CMAKE_BINARY_DIR}/_deps/sdbus-cpp-build/libsdbus-c++.a")
        add_library(sdbus-cpp STATIC IMPORTED)
        set_target_properties(sdbus-cpp PROPERTIES
            IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/_deps/sdbus-cpp-build/libsdbus-c++.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/_deps/sdbus-cpp-src/include"
        )
    endif()
    add_library(sdbus-c++::sdbus-c++ ALIAS sdbus-cpp)
endif()

if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_GetProperties(nlohmann_json)
    if(NOT nlohmann_json_POPULATED)
        FetchContent_Populate(nlohmann_json)
        
        # nlohmann_json is a header-only library, so we just need to make the includes available
        add_library(nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${nlohmann_json_SOURCE_DIR}/include"
        )
    endif()
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# Define executables
add_executable(manager configurationManager.cpp)
add_executable(client configurationClient.cpp)

# Set output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_link_libraries(manager PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIB}
    CLI11::CLI11
)
target_link_libraries(client PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIB}
    CLI11::CLI11
)

# Find clang-format for code formatting
find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
        COMMENT "Formatting all source files..."
    )
else()
    message(STATUS "clang-format not found - formatting target unavailable")
endif()

# Add testing target (for future implementation)
enable_testing()
add_custom_target(tests
    COMMAND echo "No tests implemented yet"
    COMMENT "Running all tests..."
)

# Installation rules
install(TARGETS client manager
    RUNTIME DESTINATION bin
)

# Install demo script
install(FILES demo.sh
    DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Print configuration summary
message(STATUS "========== Configuration Summary ==========")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Using sdbus-c++: ${sdbus-c++_FOUND}")
message(STATUS "Using spdlog: ${spdlog_FOUND}")
message(STATUS "Using nlohmann_json: ${nlohmann_json_FOUND}")
message(STATUS "Using CLI11: ${CLI11_FOUND}")
message(STATUS "Formatting available: ${CLANG_FORMAT}")
message(STATUS "==========================================")