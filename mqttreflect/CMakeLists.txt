cmake_minimum_required(VERSION 3.7)
project(cpp-projects)
# cmake  -D CMAKE_C_COMPILER=gcc-9 -D CMAKE_CXX_COMPILER=g++-9 ../mqttreflect/
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++17")
add_definitions(-Wno-unused-result)
add_definitions(-std=c++17)
include_directories($ENV{HOME}/local/include common)
link_directories($ENV{HOME}/local/lib $ENV{HOME}/local/lib/async_comm common)

SET(CMAKE_MODULE_PATH lib/rapidjson/CMakeModules)
SET(CMAKE_MODULE_PATH lib/mqtt.c/cmake)
SET(CMAKE_MODULE_PATH lib/mqtt.cpp/cmake)

add_subdirectory(common)
add_subdirectory(mqttreflect)
add_subdirectory(com2mqtt)

