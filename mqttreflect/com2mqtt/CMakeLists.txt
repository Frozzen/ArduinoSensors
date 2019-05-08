cmake_minimum_required(VERSION 3.10)
project(com2mqtt)

set(CMAKE_CXX_FLAGS "-std=c++11")
include_directories($ENV{HOME}/local/include ../common)
link_directories($ENV{HOME}/local/lib $ENV{HOME}/local/lib/async_comm ../common)

add_executable(serial_loopback serial_loopback.cpp)
target_link_libraries(serial_loopback async_comm boost_system boost_thread pthread)

add_executable(com2mqtt com2mqtt.cpp)
target_link_libraries(com2mqtt async_comm boost_system boost_thread pthread)