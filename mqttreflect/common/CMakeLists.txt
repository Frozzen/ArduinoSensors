cmake_minimum_required(VERSION 3.13)


set(CMAKE_CXX_STANDARD 17)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lGL -lSDL2 -lOpenGL")

set(SOURCE_FILES config.cpp config.hpp serial.cpp serial.hpp)

add_library(common STATIC ${SOURCE_FILES})

install(TARGETS common DESTINATION $ENV{HOME}/local/lib)
