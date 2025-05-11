cmake_minimum_required(VERSION 3.14)
project(Dbus-config-manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(sdbus-c++ 2.0.0 QUIET)
find_package(spdlog QUIET)
find_package(nlohmann_json 3.12.0 QUIET)
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

add_executable(manager configurationManager.cpp)
add_executable(client configurationClient.cpp)

target_link_libraries(manager PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIB}
)
target_link_libraries(client PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIB}
)

install(TARGETS client manager
  RUNTIME DESTINATION .
)