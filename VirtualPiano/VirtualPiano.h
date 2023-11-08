#ifndef VIRTUALPIANO_H
#define VIRTUALPIANO_H

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

#include <QObject>
#include <vector>

typedef struct _fluid_audio_driver_t fluid_audio_driver_t;      /**< Audio driver instance */
typedef struct _fluid_synth_t fluid_synth_t;                    /**< Synthesizer instance */
typedef struct _fluid_hashtable_t fluid_settings_t;             /**< Configuration settings instance */
class RtMidiIn;

class VirtualPiano : public QObject
{
public:
    explicit VirtualPiano(QObject *parent = 0);
    ~VirtualPiano();

    void loadSound(const QString& path);

private:
    static void midiIn( double deltatime, std::vector< unsigned char > *message, void *userData );

    fluid_settings_t* d_settings;
    fluid_synth_t* d_synth;
    fluid_audio_driver_t* d_audio;

    QList<RtMidiIn*> ports;
};

#endif // VIRTUALPIANO_H
