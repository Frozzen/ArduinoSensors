cmake_minimum_required(VERSION 3.7)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(SOURCE_FILES config.cpp config.hpp)

add_library(mqttapp STATIC ${SOURCE_FILES})

install(TARGETS mqttapp DESTINATION $ENV{HOME}/local/lib)

add_executable(cfg-test cfg-test.cpp ${LIB_FILES} config.hpp config.cpp)

