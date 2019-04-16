TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt
LIBS += -L/home/devel/local/lib
INCLUDEPATH += /home/devel/local/include ../common

LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a -lboost_system -lboost_thread -lpthread
LIBS += -L../common -L. -lcommon

SOURCES +=  mqtt-reflect.cpp

HEADERS += config.hpp \
    mqtt-reflect.hpp
