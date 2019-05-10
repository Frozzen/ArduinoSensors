cmake_minimum_required(VERSION 3.10)
project(mqttreflect)

find_package(Boost)
IF (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})
endif ()
set(Boost_USE_MULTITHREAD ON)

# for perf
add_executable(mqttreflect mqtt-reflect.cpp mqtt-reflect.hpp ${LIB_FILES} ../common/config.hpp ../common/config.cpp)
target_link_libraries(mqttreflect paho-mqttpp3 paho-mqtt3a boost_system boost_thread pthread)

