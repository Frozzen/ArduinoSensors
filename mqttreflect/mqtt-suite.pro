TEMPLATE = subdirs
SUBDIRS = common serial-test mqttreflect
serial-test.depends = common
mqttreflect.depends = common

CONFIG += ordered
