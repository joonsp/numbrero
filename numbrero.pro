QT += core gui qml quick quickcontrols2 dbus svg

CONFIG += c++17 release
TARGET = numbrero
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/engine.h \
    src/filepicker.h \
    src/highlighter.h \
    src/portalfilepicker.h \
    src/sheetstore.h \
    src/theme.h \
    src/translator.h \
    src/units.h

SOURCES += \
    src/backend.cpp \
    src/cli.cpp \
    src/engine.cpp \
    src/highlighter.cpp \
    src/main.cpp \
    src/portalfilepicker.cpp \
    src/sheetstore.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/units.cpp

RESOURCES += src/resources.qrc
