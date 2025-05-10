cmake_minimum_required(VERSION 3.10)
project(Dbus-config-manager)

# First, find sdbus-c++
find_package(sdbus-c++ REQUIRED)

# Use the sdbus-c++ target in SDBusCpp namespace
add_executable(manager configurationManager.cpp)
add_executable(client configurationClient.cpp)
target_link_libraries(manager PRIVATE SDBusCpp::sdbus-c++)

install(TARGETS client manager
  RUNTIME DESTINATION .  # Copies to ${CMAKE_INSTALL_PREFIX}/bin
)