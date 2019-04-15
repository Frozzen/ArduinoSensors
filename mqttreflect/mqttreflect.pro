TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt
LIBS += -L/home/devel/local/lib
INCLUDEPATH += /home/devel/local/include

LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a -lboost_system -lboost_thread -lpthread

SOURCES +=  config.cpp \
    mqtt-reflect.cpp \
    rs232.c \
    serial.cpp

HEADERS += \
    rs232.h \
    config.hpp \
    mqtt-reflect.hpp
