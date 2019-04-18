cmake_minimum_required(VERSION 3.13)
project(mqttreflect)

include_directories($ENV{HOME}/local/include ../common)
link_directories($ENV{HOME}/local/lib ../common)
set(CMAKE_CXX_STANDARD 17)

find_package(Boost)
IF (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})
endif ()
set(Boost_USE_MULTITHREAD ON)

# for perf
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

#add_dependencies(mqttreflect ../common)
#set(SOURCES mqtt-reflect.cpp mqtt-reflect.hpp)
#set(LIB_FILES ../common/config.hpp ../common/config.cpp)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lGL -lSDL2 -lOpenGL")
add_executable(mqttreflect mqtt-reflect.cpp mqtt-reflect.hpp ${LIB_FILES} ../common/config.hpp ../common/config.cpp)
target_link_libraries(mqttreflect paho-mqttpp3 paho-mqtt3a boost_system boost_thread pthread)
