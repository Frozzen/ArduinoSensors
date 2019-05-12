cmake_minimum_required(VERSION 3.7)
project(mqttreflect)

find_package(Boost)
IF (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})
endif ()
set(Boost_USE_MULTITHREAD ON)

add_definitions(-DBOOST_LOG_DYN_LINK)
add_executable(mqttreflect mqtt-reflect.cpp mqtt-reflect.hpp ${LIB_FILES} ../common/config.hpp ../common/config.cpp)
target_link_libraries(mqttreflect paho-mqttpp3 paho-mqtt3a boost_system boost_thread pthread boost_log)

