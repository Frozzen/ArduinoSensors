cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_MODULE_PATH lib/rapidjson/CMakeModules)
SET(CMAKE_MODULE_PATH lib/mqtt.c/cmake)
SET(CMAKE_MODULE_PATH lib/mqtt.cpp/cmake)

add_subdirectory(common)
add_subdirectory(mqttreflect)
add_subdirectory(com2mqtt)

