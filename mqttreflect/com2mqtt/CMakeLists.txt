cmake_minimum_required(VERSION 3.7)
project(com2mqtt)

#find_package(async_comm REQUIRED)
#find_package(Threads REQUIRED)

add_executable(serial_loopback serial_loopback.cpp)
target_link_libraries(serial_loopback async_comm boost_system boost_thread pthread)

add_executable(com2mqtt com2mqtt.cpp com2mqtt.hpp ../common/config.cpp)
target_link_libraries(com2mqtt async_comm paho-mqttpp3 paho-mqtt3a boost_system boost_thread pthread)

add_executable(boost-com-test boost-com-sample.cpp SimpleSerial.h)
target_link_libraries(boost-com-test ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# target_link_libraries(boost-com-test boost_system boost_thread pthread)
