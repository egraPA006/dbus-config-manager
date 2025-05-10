cmake_minimum_required(VERSION 3.14)
project(Dbus-config-manager)

set(SDBUSCPP_MIN_VERSION "2.0.0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(sdbus-c++ ${SDBUSCPP_MIN_VERSION} QUIET)
find_package(spdlog QUIET)

if(NOT sdbus-c++_FOUND)
    include(FetchContent)
    set(SDBUSCPP_BUILD_DOCS OFF CACHE BOOL "Disable docs")
    set(SDBUSCPP_BUILD_TESTS OFF CACHE BOOL "Disable tests")
    set(SDBUSCPP_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples")
    set(SDBUSCPP_BUILD_CODEGEN OFF CACHE BOOL "Disable codegen")
    set(SDBUSCPP_INSTALL ON CACHE BOOL "Enable install")
    FetchContent_Declare(
        sdbus-cpp
        GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
        GIT_TAG v2.1.0
    )
    FetchContent_MakeAvailable(sdbus-cpp)

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

add_executable(manager configurationManager.cpp)
add_executable(client configurationClient.cpp)

target_link_libraries(manager PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
)

install(TARGETS client manager
  RUNTIME DESTINATION .
)