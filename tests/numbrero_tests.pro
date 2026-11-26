QT += core gui qml quick quickcontrols2 testlib dbus svg
CONFIG += c++17 testcase
TARGET = numbrero_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/cli.h \
    ../src/engine.h \
    ../src/filepicker.h \
    ../src/highlighter.h \
    ../src/sheetstore.h \
    ../src/theme.h \
    ../src/translator.h \
    ../src/units.h

SOURCES += \
    numbrero_tests.cpp \
    ../src/backend.cpp \
    ../src/cli.cpp \
    ../src/engine.cpp \
    ../src/highlighter.cpp \
    ../src/sheetstore.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp \
    ../src/units.cpp

RESOURCES += ../src/resources.qrc
