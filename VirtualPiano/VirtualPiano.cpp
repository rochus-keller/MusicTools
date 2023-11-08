/*
* Copyright 2023 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the MusicTools application suite.
*
* The following is the license that applies to this copy of the
* file. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "VirtualPiano.h"
#include <fluidsynth.h>
#include <RtMidi.h>
#include <QtDebug>
#include <QCoreApplication>
#include <stdio.h>
#include <QSettings>

VirtualPiano::VirtualPiano(QObject *parent) : QObject(parent)
{
    d_settings = new_fluid_settings();
    fluid_settings_setstr(d_settings, "audio.driver", "pulseaudio");
    //fluid_settings_setstr(d_settings, "midi.driver", "alsa_raw");
    //fluid_settings_setstr(d_settings, "midi.alsa.device", "hw:1,0,0");


    d_synth = new_fluid_synth(d_settings);

    d_audio = new_fluid_audio_driver(d_settings, d_synth);
    //fluid_synth_sfload(d_synth, "/home/me/Downloads/FullConcertGrandV3.sf2", 1);
    //fluid_synth_sfload(d_synth, "/home/me/Downloads/Chateau Grand Lite-v1.0.sf2", 1);
    QSettings set;
    const QString path = set.value("SoundFont").toString();
    if( !path.isEmpty() )
    {
        qDebug() << "loading" << path;
        fluid_synth_sfload(d_synth, path.toUtf8().constData(), 1);
    }

    //d_midi = new_fluid_midi_driver(d_settings, handleMidiIn, this);

    fluid_synth_program_change( d_synth, 0, 0 );
    fluid_synth_cc( d_synth, 0, 7, 127 );
    fluid_synth_set_gain( d_synth, 1.0 );
    //fluid_synth_set_reverb( d_synth, 0.7, 0.0, 0.8, 0.9 );
    fluid_synth_set_reverb_on( d_synth, 0 );

    RtMidiIn in;
    const size_t nPorts = in.getPortCount();
    for ( size_t i=0; i<nPorts; i++ )
    {
        const QByteArray name = in.getPortName(i).c_str();
        qDebug() << "*** open" << name << "midi input port";
        RtMidiIn* port = new RtMidiIn();
        port->ignoreTypes(true,true,true);
        port->setCallback(midiIn,this);
        port->openPort(i, name.constData());
        ports.append(port);
    }

    RtMidiIn* port = new RtMidiIn();
    port->ignoreTypes(true,true,true);
    port->setCallback(midiIn,this);
    port->openVirtualPort("VirtualPiano");
    ports.append(port);
    qDebug() << "*** open VirtualPiano midi input port";

}

VirtualPiano::~VirtualPiano()
{
    for( int i = 0; i < ports.size(); i++ )
        delete ports[i];
    if( d_audio )
        delete_fluid_audio_driver( d_audio );
    delete_fluid_synth(d_synth);
    delete_fluid_settings(d_settings);
}

void VirtualPiano::loadSound(const QString& path)
{
    fluid_synth_sfload(d_synth, path.toUtf8().constData(), 1);
    QSettings set;
    set.setValue("SoundFont",path);
}

void VirtualPiano::midiIn(double deltatime, std::vector<unsigned char>* message, void* userData)
{
    VirtualPiano* that = (VirtualPiano*)userData;
    if( message == 0 || message->size() < 3 )
        return;
#if 0
    QByteArray msg;
    for( int i = 0; i < message->size(); i++ )
        msg += (char)message->at(i);
    qDebug() << "midi in" << msg.toHex();
#endif
    switch( message->at(0) & 0xf0 )
    {
    case 0x90: // note on
        fluid_synth_noteon(that->d_synth, message->at(0) & 0x0f, message->at(1), message->at(2));
        break;
    case 0x80: // note off
        fluid_synth_noteoff(that->d_synth, message->at(0) & 0x0f, message->at(1));
        break;
    case 0xB0: // control change
        fluid_synth_cc(that->d_synth, message->at(0) & 0x0f, message->at(1), message->at(2));
        break;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    VirtualPiano p;

    if( a.arguments().size() > 1 )
        p.loadSound(a.arguments()[1]);

    qDebug() << "listening, press enter to quit";
    getchar();
    return 0; //a.exec();
}
