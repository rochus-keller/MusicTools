QT       += core
QT       -= gui

TARGET = VirtualPiano
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    VirtualPiano.cpp \
    ../rtmidi/RtMidi.cpp

HEADERS += \
    VirtualPiano.h \
    ../rtmidi/RtMidi.h

LIBS += -lfluidsynth -lasound

CONFIG += c++11

INCLUDEPATH += ../rtmidi

DEFINES += __LINUX_ALSA__
