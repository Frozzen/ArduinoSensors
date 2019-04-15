TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt
LIBS += -L/home/vovva/local/lib
INCLUDEPATH += /home/vovva/local/include ../common

LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a -lboost_system -lboost_thread -lpthread
LIBS += -L../common -lcommon

SOURCES +=  mqtt-reflect.cpp

HEADERS += config.hpp \
    mqtt-reflect.hpp
