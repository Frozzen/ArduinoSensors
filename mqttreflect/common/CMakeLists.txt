cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
include_directories($ENV{HOME}/local/include .)

include_directories($ENV{HOME}/local/include ../common)
link_directories($ENV{HOME}/local/lib ../common)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(SOURCE_FILES config.cpp config.hpp)

add_library(common STATIC ${SOURCE_FILES})

install(TARGETS common DESTINATION $ENV{HOME}/local/lib)

add_executable(cfg-test cfg-test.cpp ${LIB_FILES} config.hpp config.cpp)

