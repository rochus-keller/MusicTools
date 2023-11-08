#ifndef _MIDIENGINE_H
#define _MIDIENGINE_H

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

#include <QWidget>

class MidiEngine : public QObject
{
    Q_OBJECT
public:
    MidiEngine(QObject* parent);
    ~MidiEngine();

    QString getSinkPath() const;
signals:
    void onWritten(int);
protected:
    void timerEvent(QTimerEvent *event);
private:
    class Imp;
    Imp* d_imp;
    friend class MidiMonitor;
};

class QLabel;

class MidiMonitor : public QWidget
{
    Q_OBJECT
public:
    MidiMonitor();

protected slots:
    void onWritten(int);
    void onConvert();
    void onConvert2();

protected:
    void convert( const QString& inpath, const QString& outpath );

private:
    QLabel* d_file;
    QLabel* d_time;
    QLabel* d_bytes;
    quint32 d_written;
    MidiEngine* d_eng;
};

#endif // _MIDIENGINE_H
