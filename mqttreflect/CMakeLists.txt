cmake_minimum_required(VERSION 3.7)
project(shome-projects)
# cmake  -D CMAKE_C_COMPILER=gcc-9 -D CMAKE_CXX_COMPILER=g++-9 ../mqttreflect/
add_definitions(-Wno-unused-result)
add_definitions(-std=c++17)
include_directories($ENV{HOME}/local/include common)

add_definitions(-DBOOST_LOG_DYN_LINK)
find_package(Threads REQUIRED)
if(true)
    set(CMAKE_CXX_STANDARD 17)
    set(BOOST_LIBS date_time system)
    find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
else()
    set(BOOST_LIBS boost_system boost_tread)
endif()
link_directories($ENV{HOME}/local/lib $ENV{HOME}/local/lib/async_comm common)

add_subdirectory(common)
add_subdirectory(mqttreflect)
add_subdirectory(com2mqtt)

install(FILES mqttfrwd.json DESTINATION bin COMPONENT config)
