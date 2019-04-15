TEMPLATE = app
CONFIG += console c++17
#CONFIG -= app_bundle
CONFIG -= qt
LIBS +=  -L../common -lcommon
INCLUDEPATH += ../common/
#DEPENDPATH
serial-test.depends = ../common
SOURCES += main.cpp ../common/serial.cpp

