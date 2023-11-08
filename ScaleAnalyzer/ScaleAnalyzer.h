#ifndef SCALEANALYZER_H
#define SCALEANALYZER_H

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

#include <QtDebug>
#include <QVector>

class ScaleAnalyzer
{
public:
    typedef quint16 Scale; // LSB is the first note, usually C6
    enum { NullScale = 0, ScaleWidth = 12, MaxScale = 0xfff }; // 12 bits

    ScaleAnalyzer();

    void analyze(bool removeRotationSymmetricals = false);

    const QList<Scale>& getScales(quint8 numOfNotes) const { return scales[numOfNotes-1]; }
    QList<Scale> allScales() const;

    static int OneCount(unsigned int u);

    static Scale Rotated( Scale s, int n );
    static Scale Inverted( Scale s, int n = 0 ); // n..pos of the axes; n even: on note n/2; n odd: between note n/2 and following

    static int Rotation(Scale ref, Scale other); // >= 0 if rot equal, -1 if not rot equal

    static int HalfStep(Scale s); // number of half steps per scale

    static QByteArray toBinString(Scale s );
    static QByteArray toPcSet(Scale s);
    static Scale fromSteps(QByteArray str);
    static QByteArray toSteps(Scale s);

    static bool isOn(Scale s, quint8 semitone);
    static bool isWhiteOn(Scale s, quint8 whiteNr); // 0..6
    static bool isBlackOn(Scale s, quint8 blackNr); // 0..4
    static quint8 whiteToChromatic(Scale s, quint8 whiteNr); // 0..6
    static quint8 blackToChromatic(Scale s, quint8 blackNr); // 0..4
private:
    QVector< QList<Scale> > scales;
};

#endif // SCALEANALYZER_H
