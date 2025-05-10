cmake_minimum_required(VERSION 3.14)
project(Dbus-config-manager)

set(SDBUSCPP_MIN_VERSION "2.0.0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(sdbus-c++ ${SDBUSCPP_MIN_VERSION} QUIET)
find_package(spdlog QUIET)

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

add_executable(manager configurationManager.cpp)
add_executable(client configurationClient.cpp)

target_link_libraries(manager PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
)
target_link_libraries(client PRIVATE
    sdbus-c++::sdbus-c++
    spdlog::spdlog
)

install(TARGETS client manager
  RUNTIME DESTINATION .
)