cmake_minimum_required(VERSION 3.7)
project(mqttreflect)

find_package(Boost)
IF (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})
endif ()
set(Boost_USE_MULTITHREAD ON)

add_executable(mqttreflect mqtt-reflect.cpp mqtt-reflect.hpp ${LIB_FILES} ../common/config.hpp ../common/config.cpp)
target_link_libraries(mqttreflect paho-mqtt3a paho-mqttpp3 boost_system boost_thread pthread boost_log)

