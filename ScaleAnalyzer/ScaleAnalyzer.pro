QT       += core gui widgets

TARGET = ScaleAnalyzer

TEMPLATE = app

HEADERS += \
    ScaleAnalyzer.h \
    ScaleViewer.h \
    FlowLayout.h

SOURCES += \
    ScaleAnalyzer.cpp \
    ScaleViewer.cpp \
    FlowLayout.cpp

CONFIG += FluidSynth


FluidSynth {
    LIBS += -lfluidsynth
    DEFINES += USING_FLUID_SYNTH
    message(using fluid synth)
} else {
    LIBS += -lasound
    CONFIG += c++11
    INCLUDEPATH += ../rtmidi
    DEFINES += __LINUX_ALSA__
    message(using rtmidi)
    HEADERS += ../rtmidi/RtMidi.h
    SOURCES += ../rtmidi/RtMidi.cpp
}
