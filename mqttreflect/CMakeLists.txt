cmake_minimum_required(VERSION 3.10)
project(cpp-projects)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++17")
include_directories($ENV{HOME}/local/include common)
link_directories($ENV{HOME}/local/lib $ENV{HOME}/local/lib/async_comm common)

SET(CMAKE_MODULE_PATH lib/rapidjson/CMakeModules)
SET(CMAKE_MODULE_PATH lib/mqtt.c/cmake)
SET(CMAKE_MODULE_PATH lib/mqtt.cpp/cmake)

add_subdirectory(common)
add_subdirectory(mqttreflect)
add_subdirectory(com2mqtt)

