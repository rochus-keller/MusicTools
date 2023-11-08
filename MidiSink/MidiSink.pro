QT       += core gui widgets

TARGET = MidiSink
TEMPLATE = app

HEADERS += \
    MidiEngine.h \
    ../rtmidi/RtMidi.h

SOURCES += \
    MidiEngine.cpp \
    ../rtmidi/RtMidi.cpp

LIBS += -lasound

CONFIG += c++11

INCLUDEPATH += ../rtmidi

DEFINES += __LINUX_ALSA__
