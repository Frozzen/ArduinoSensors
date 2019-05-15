cmake_minimum_required(VERSION 3.7)
project(com2mqtt)

add_executable(com2mqtt com2mqtt.cpp com2mqtt.hpp ../common/config.cpp ../common/config.hpp SimpleSerial.h)
target_link_libraries(com2mqtt paho-mqttpp3 paho-mqtt3a ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS com2mqtt DESTINATION bin COMPONENT binaries)
install(FILES com2mqtt.json DESTINATION bin COMPONENT config)
