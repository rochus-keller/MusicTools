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

#include "MidiEngine.h"
#include <RtMidi.h>
#include <QtDebug>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QApplication>

class MidiEngine::Imp
{
public:

    int bytes;
    QElapsedTimer timer;

    Imp():bytes(0)
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if( path.isEmpty() )
            path = QDir::homePath();
        if( path.isEmpty() )
            throw QString("cannot find document nor home directory");
        QDir dir(path);
        const QByteArray tag("MidiSink");
        if( !dir.mkpath(tag) )
            throw QString("cannot create directory: %1").arg(path);
        dir.cd(tag);
        const QByteArray name = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss").toUtf8();
        out.setFileName( dir.absoluteFilePath(name + ".midisink" ) );
        if( !out.open(QIODevice::WriteOnly) )
            throw QString("cannot open file for writing: %1").arg(out.fileName());
        bytes += out.write(tag.constData(),tag.size()+1);
        bytes += out.write(name.constData(), name.size()+1);
        qDebug() << "Streaming to" << out.fileName();
        timer.start();
    }

    ~Imp()
    {
        for( int i = 0; i < ports.size(); i++ )
            delete ports[i];
        out.flush();
        if( out.size() <= 25 )
            out.remove();
    }

    void fetchPorts()
    {
        RtMidiIn in;
        // works as well: in.openVirtualPort("MidiSink");
        const size_t nPorts = in.getPortCount();
        qDebug() << "*** found" << nPorts << "ports";
        for ( size_t i=0; i<nPorts; i++ )
        {
            const QByteArray name = in.getPortName(i).c_str();
            Port* p = new Port(name, i, ports.size(), this );
            ports.append( p );
            if( ports.size() > 255 )
                throw QString("too many MIDI in ports, only 255 supported");
            //qDebug() << "    " << name;
        }
    }

    struct Port
    {
        QByteArray name;
        quint16 index;
        quint8 track;
        bool hasData;
        RtMidiIn in;
        Imp* that;
        quint32 lastTime;
        Port(const QByteArray& n, int i, int t, Imp* imp):name(n),index(i),track(t),that(imp),lastTime(0),hasData(false)
        {
            in.ignoreTypes(true,true,true);
            in.setCallback(callback,this);
            in.openPort(i);
        }
    };

    QList<Port*> ports;
    QFile out;

    static void callback( double deltatime, std::vector< unsigned char > *message, void *userData )
    {
        // tested on Mac.
        // deltatime is a strange beast; seems to not differ channels; the first event on a channel comes
        // with delta 0, but port 2 (and only that) strangely starts with a very large number; both
        // observations make the concept unuseful for me; I therefore need to create the timestamp myself.
        Port* port = (Port*) userData;
        if( !port->hasData )
        {
            QByteArray msg = toVarLen(0);
            msg += char(port->track);
            msg += char(0xff);
            msg += char(0x03); // Sequence/Track Name
            msg += toVarLen(port->name.size());
            msg += port->name;
            port->that->bytes += port->that->out.write(msg);
            qDebug() << "    " << port->name;
            port->hasData = true;
        }
        // const quint32 tick = deltatime * 1000.0 + 0.5;
        const quint32 tick = port->that->timer.elapsed();
        const quint32 diff = tick - port->lastTime;
        port->lastTime = tick;
        QByteArray msg = toVarLen(diff); // milliseconds
        msg += char(port->track);
        const QByteArray data = QByteArray::fromRawData((const char*)message->data(), message->size());
        msg += data;
        port->that->bytes += port->that->out.write(msg);
        // qDebug() << port->track << diff << data.toHex().constData();
    }

    static QByteArray toVarLen(quint32 value)
    {
        quint32 buffer = value & 0x7f;
        while ((value >>= 7) > 0)
        {
            buffer <<= 8;
            buffer |= 0x80;
            buffer += (value & 0x7f);
        }
        QByteArray res;
        while (true)
        {
            res += char(buffer & 0xff);
            if (buffer & 0x80)
                buffer >>= 8;
            else
                break;
        }
        return res;
    }

    static quint32 fromVarLen(QIODevice* in)
    {
        quint32 value;
        char ch;
        if( !in->getChar(&ch) )
            ch = 0;
        value = (quint8) ch;
        if ( value & 0x80 )
        {
            value &= 0x7f;
            quint8 c = 0;
            do
            {
                if( !in->getChar(&ch) )
                    ch = 0;
                c = ch;
                value = (value << 7) + ( c & 0x7f);
            } while (c & 0x80);
        }
        return value;
    }

    struct Track
    {
        QByteArray name;
        QByteArray data;
    };
    typedef QVector<Track> Tracks;

    static QByteArray readString( QIODevice* in)
    {
        QByteArray str;
        while( !in->atEnd() )
        {
            char ch = 0;
            in->getChar(&ch);
            if( ch == 0 )
                break;
            else
                str += ch;
        }
        return str;
    }

    struct Cell
    {
        quint32 time;
        quint8 track;
        bool meta;
        QByteArray data;
        Cell():time(0),track(0),meta(false){}
    };

    static bool readCell( QIODevice* in, Cell& cell)
    {
        cell.time = fromVarLen(in);
        char ch;
        if( !in->getChar(&ch) )
            ch = 0;
        cell.track = (quint8) ch;
        if( !in->getChar(&ch) )
            ch = 0;
        const quint8 type = (quint8) ch;
        if( type == 0xff )
        {
            cell.meta = true;
            if( !in->getChar(&ch) )
                ch = 0;
            if( ch != 0x03 )
                return false;
            const quint32 len = fromVarLen(in);
            cell.data = in->read(len);
            return true;
        }else if( type >= 0xf0 )
            return false;
        else
        {
            cell.meta = false;
            if( !(type & 0x80) )
                return false; // don't support running status
            cell.data.resize(1);
            cell.data[0] = type;
            const quint8 status = type >> 4;
            if( status == 0xc || status == 0xd )
                cell.data += in->read(1);
            else
                cell.data += in->read(2);
            return true;
        }
    }

    static bool checkHeader( QFile& in )
    {
        if( !in.open(QIODevice::ReadOnly) )
            return false;

        const QByteArray tag = readString(&in);
        if( tag != "MidiSink" )
            return false;
        readString(&in); // ignore timestamp
        return true;
    }

    static bool readStream( const QString& path, Tracks& tracks )
    {
        QFile in(path);
        if( !checkHeader(in) )
            return false;

        Cell cell;
        quint32 lastTime = 0;
        while( !in.atEnd() )
        {
            if( !readCell(&in,cell) )
                return false;
            if( cell.meta )
            {
                if( tracks.size() <= cell.track)
                    tracks.resize(cell.track + 1);
                tracks[cell.track].name = cell.data;
            }else
            {
                if( tracks.size() <= cell.track)
                    return false;
                lastTime = cell.time;
                tracks[cell.track].data += toVarLen(cell.time);
                tracks[cell.track].data += cell.data;
                // qDebug() << cell.track << cell.time << cell.data.toHex().constData();
            }
        }

        for( int i = 0; i < tracks.size(); i++ )
        {
            if( tracks[i].data.isEmpty() || tracks[i].name.isEmpty() )
                continue;

            QByteArray start;
            start += toVarLen(0);
            start += char(0xff);
            start += char(0x03);
            start += toVarLen(tracks[i].name.size());
            start += tracks[i].name;

            QByteArray end;
            end += toVarLen(lastTime);
            end += char(0xff);
            end += char(0x2f);
            end += char(0x00);

            tracks[i].data = start + tracks[i].data + end;
        }

        return true;
    }

    static bool writeStream( const QString& path, const Tracks& tracks )
    {
        QFile out(path);
        if( !out.open(QIODevice::WriteOnly) )
            return false;

        int numTracks = 0;
        for( int i = 0; i < tracks.size(); i++ )
        {
            if( tracks[i].data.isEmpty() || tracks[i].name.isEmpty() )
                continue;
            numTracks++;
        }

        out.write("MThd");
        QByteArray len(4,char(0));
        len[3] = 6;
        out.write(len);
        QByteArray word(2,char(0));
        word[1] = 1; // Format 1, one or more simultaneous tracks
        out.write(word);
        word[1] = numTracks;
        out.write(word);
#if 0
        // millisecond-based tracks by specifying 25 frames/sec and a resolution of 40 units per frame
        word[0] = char(0xe7); // twos complement of 25
        word[1] = 0x28; // 40 units
#else
        // 120 pbm = 120 quarter notes per minute = 2 quarter notes per second
        // so 1 quarter note is 500 ms
        const short ticks = 500;
        word[0] = char((ticks >> 8)) & 0xff;
        word[1] = char(ticks & 0xff);
        // tempo is assumed to be 120 bpm
        // optionally add FF 58 and FF 51 to each track
#endif
        out.write(word);

        for( int i = 0; i < tracks.size(); i++ )
        {
            if( tracks[i].data.isEmpty() || tracks[i].name.isEmpty() )
                continue;
            out.write("MTrk"); // 4D 54 72 6B
            const quint32 len = tracks[i].data.size();
            QByteArray bytes(4,char(0));
            bytes[0] = char((len >> 24) & 0xff);
            bytes[1] = char((len >> 16) & 0xff);
            bytes[2] = char((len >> 8)) & 0xff;
            bytes[3] = char(len & 0xff);
            out.write(bytes);
            out.write(tracks[i].data);
        }

        return true;
    }

    static void gmPrefix(QFile& out, quint32 time, quint8 chan)
    {
        QByteArray buf(3,0);
        buf[0] = char( 0xb0 | chan );

        out.write(toVarLen(time));
        out.write(buf); // b0 0 0

        buf[1] = 0x20;
        buf[2] = 0;
        out.write(toVarLen(0));
        out.write(buf); // b0 20 0

        buf[1] = 0x7;
        buf[2] = 0x6e;
        out.write(toVarLen(0));
        out.write(buf); // b0 7 6e

        buf[1] = 0xa;
        buf[2] = 0x39;
        out.write(toVarLen(0));
        out.write(buf); // b0 a 39

        buf[1] = 0xb;
        buf[2] = 0x40;
        out.write(toVarLen(0));
        out.write(buf); // b0 b 40

        buf[1] = 0x5b;
        buf[2] = 0x69;
        out.write(toVarLen(0));
        out.write(buf); // b0 5b 69

        buf[1] = 0x5d;
        buf[2] = 0x1e;
        out.write(toVarLen(0));
        out.write(buf); // b0 5d 1e
    }

};

MidiEngine::MidiEngine(QObject *parent):QObject(parent),d_imp(0)
{
    try
    {
        d_imp = new Imp();
        d_imp->fetchPorts();
        startTimer(1000);
    }catch(  const RtMidiError &error )
    {
        throw QString::fromUtf8(error.what());
    }
}

MidiEngine::~MidiEngine()
{
    if( d_imp )
        delete d_imp;
}

QString MidiEngine::getSinkPath() const
{
    return d_imp->out.fileName();
}

void MidiEngine::timerEvent(QTimerEvent *event)
{
    if( d_imp->bytes )
    {
        d_imp->out.flush();
        // qDebug() << "written" << d_imp->bytes << "bytes";
        emit onWritten(d_imp->bytes);
        d_imp->bytes = 0;
    }
}


MidiMonitor::MidiMonitor():d_eng(0),d_written(0)
{
    QVBoxLayout* vbox = new QVBoxLayout(this);
    d_file = new QLabel(this);
    vbox->addWidget(d_file);
    d_time = new QLabel(this);
    vbox->addWidget(d_time);
    d_bytes = new QLabel(this);
    vbox->addWidget(d_bytes);
    QPushButton* pb = new QPushButton("Convert to MIDI file", this);
    vbox->addWidget(pb);
    connect(pb,SIGNAL(clicked(bool)),this,SLOT(onConvert()));
    pb = new QPushButton("Convert to GM file", this);
    vbox->addWidget(pb);
    connect(pb,SIGNAL(clicked(bool)),this,SLOT(onConvert2()));
    try
    {
        d_eng = new MidiEngine(this);
        d_file->setText(d_eng->getSinkPath());
        connect(d_eng,SIGNAL(onWritten(int)),this,SLOT(onWritten(int)));
    }catch( const QString& err )
    {
        QMessageBox::critical(this,"Error initializing MidiSink", err );
    }

    // TEST
    // const QString path = "/home/me/MidiSink/referenz.midisink";
    // convert(path, path + ".mid");
}

void MidiMonitor::onWritten(int bytes)
{
    d_time->setText( QTime::currentTime().toString(Qt::ISODate) );
    QLocale loc;
    d_written += bytes;
    d_bytes->setText(tr("%1 KB").arg(loc.toString(d_written/1024.0,'f',1)));
}

void MidiMonitor::onConvert()
{
    const QString path = QFileDialog::getOpenFileName(this,tr("Open MidiSink Stream"),
                                                      QFileInfo(d_eng->getSinkPath()).absolutePath(),
                                                      "*.midisink");
    if( path.isEmpty() )
        return;

    convert(path, path.left(path.size()-8) + "mid");
}

void MidiMonitor::onConvert2()
{
    QString path = QFileDialog::getOpenFileName(this,tr("Open MidiSink Stream"),
                                                      QFileInfo(d_eng->getSinkPath()).absolutePath(),
                                                      "*.midisink");
    if( path.isEmpty() )
        return;

    QFile in(path);
    if( !MidiEngine::Imp::checkHeader(in) )
    {
        QMessageBox::critical(this,tr("Convert to GM file"), tr("Cannot read stream, invalid file format") );
        return;
    }

    path = path.left(path.size()-8) + "mid";

    QFile out(path);

    if( !out.open(QIODevice::WriteOnly) )
    {
        QMessageBox::critical(this,tr("Convert to GM file"), tr("Cannot open output file for writing") );
        return;
    }

    out.write("MThd");
    QByteArray len(4,char(0));
    len[3] = 6;
    out.write(len);
    QByteArray word(2,char(0));
    word[1] = 0; // type 0
    out.write(word);
    word[1] = 1; // one track
    out.write(word);
    const short ticks = 500;
    // dummy 120 pbm to fake one tick per ms
    word[0] = char((ticks >> 8)) & 0xff;
    word[1] = char(ticks & 0xff);
    out.write(word);

    out.write("MTrk");
    const int lenpos = out.pos();
    out.write( QByteArray(4,char(0)) );  // dummy, fix later

    //out.write(MidiEngine::Imp::toVarLen(0));
    // out.write(QByteArray::fromHex("F0057E7F0901F7")); // turn on GM
    //out.write(QByteArray::fromHex("f0 0a 41 10 42 12 40 00 7f 00 41 f7"));

    /*
    out.write(MidiEngine::Imp::toVarLen(0));
    out.write(QByteArray::fromHex("FF 58 04 04 02 24 08"));
    out.write(MidiEngine::Imp::toVarLen(0));
    out.write(QByteArray::fromHex("FF 51 03 50 00 00"));
    */

    enum Kind { Unknown, Drums, BassPiano, Pedal };
    struct Track
    {
        Kind kind;
        quint32 time;
        Track():kind(Unknown),time(0){}
    };

    QHash<quint8,Track> map;

    MidiEngine::Imp::Cell cell;
    const char splitpoint = 60;
    quint32 gmtime = 0;
    quint32 unused = 0;
    bool first = true;
    while( !in.atEnd() )
    {
        if( !MidiEngine::Imp::readCell(&in,cell) )
        {
            QMessageBox::critical(this,tr("Convert to GM file"), tr("Error reading file") );
            return;
        }

        Track& t = map[cell.track];
        t.time +=  cell.time;
        qint32 diff = t.time - gmtime;
        if( diff < 0 )
            diff = 0;
        gmtime += diff;

        if( cell.meta )
        {
            if( cell.data == "YAMAHA MOTIF XF7 Port3" )
            {
                t.kind = Drums;
                //MidiEngine::Imp::gmPrefix(out,diff+unused,10);
                //unused = 0;
                // out.write(MidiEngine::Imp::toVarLen(0));
                // out.write(QByteArray::fromHex("CA5F"));
            }else if( cell.data == "YAMAHA MOTIF XF7 Port1" )
            {
                t.kind = BassPiano;

                //MidiEngine::Imp::gmPrefix(out,diff+unused,0);
                //unused = 0;
                //out.write(MidiEngine::Imp::toVarLen(0));
                //out.write(QByteArray::fromHex("C005")); // Electric Piano 2 (06) on channel 0

                //MidiEngine::Imp::gmPrefix(out,0,1);
                out.write(MidiEngine::Imp::toVarLen(0));
                out.write(QByteArray::fromHex("C121")); // Electric Bass (finger, 34) on channel 1

            }else if( cell.data.startsWith("Pico CircuitPython usb_midi") )
            {
                t.kind = Pedal;
            }
        }else if( cell.data.size() > 1 && (quint8(cell.data[0]) &  0x80) )
        {
            quint8 status = (quint8)cell.data[0];
            switch( map.value(cell.track).kind )
            {
            case Drums:
                out.write(MidiEngine::Imp::toVarLen(diff+unused));
                unused = 0;
                if( (status & 0x80) || (status & 0x90) )
                {
                    // only interested in NoteOn/Off
                    status = ( status & 0xf0 ) | 0x9; // redirect to channel 10
                    cell.data[0] = (char)status;
                    switch( cell.data[1] )
                    {
                    case 39:
                        cell.data[1] = 38;
                        break;
                    case 43:
                        cell.data[1] = 45;
                        break;
                    case 45:
                        cell.data[1] = 47;
                        break;
                    case 47:
                        cell.data[1] = 50;
                        break;
                    case 48:
                        cell.data[1] = 49;
                        break;
                    case 49:
                        cell.data[1] = 53;
                        break;
                    case 50:
                        cell.data[1] = 57;
                        break;
                    case 52:
                        cell.data[1] = 59;
                        break;
                    default:
                        break;
                    }
                    out.write(cell.data);
                }
                break;
            case BassPiano:
                if( (status & 0x80) || (status & 0x90) ) // noteon/off
                {
                    out.write(MidiEngine::Imp::toVarLen(diff+unused));
                    unused = 0;
                    if( cell.data[1] >= splitpoint )
                    {
                        // Piano
                        status = ( status & 0xf0 );
                        cell.data[0] = (char)status;
                        cell.data[1] = cell.data[1] - char(12);
                    }else
                    {
                        // bass
                        status = ( status & 0xf0 ) | 0x1;
                        cell.data[0] = (char)status;
                        // cell.data[1] = cell.data[1] + char(12);
                    }
                    out.write(cell.data);
                }else if( status & 0xd0 ) // channel pressure
                {
                    out.write(MidiEngine::Imp::toVarLen(diff+unused));
                    unused = 0;
                    status = ( status & 0xf0 ) | 0x1; // redirect to channel 1
                    cell.data[0] = (char)status;
                    out.write(cell.data);
                }else
                    unused += diff;
                break;
            case Pedal:
                status = ( status & 0xf0 );
                cell.data[0] = (char)status;
                out.write(MidiEngine::Imp::toVarLen(diff+unused));
                unused = 0;
                out.write(cell.data);
                break;
            default:
                // don't send it, but increase timestamp for next message
                unused += diff;
                break;
            }
        }else
            qCritical() << "running status not supported" << gmtime << cell.data.toHex().constData();
        first = false;
    }

    QByteArray end;
    end += MidiEngine::Imp::toVarLen(0);
    end += char(0xff);
    end += char(0x2f);
    end += char(0x00);
    out.write(end);

    const quint32 l = out.pos() - lenpos - 4;
    QByteArray bytes(4,char(0));
    bytes[0] = char((l >> 24) & 0xff);
    bytes[1] = char((l >> 16) & 0xff);
    bytes[2] = char((l >> 8)) & 0xff;
    bytes[3] = char(l & 0xff);
    out.seek(lenpos);
    out.write(bytes);
}

void MidiMonitor::convert(const QString &inpath, const QString &outpath)
{
    MidiEngine::Imp::Tracks tracks;
    if( !MidiEngine::Imp::readStream( inpath, tracks ) )
    {
        QMessageBox::critical(this,tr("Open MidiSink Stream"), tr("Cannot read stream, invalid file format") );
        return;
    }

    MidiEngine::Imp::writeStream(outpath, tracks );
}

int main(int argc, char ** argv)
{
    QApplication a(argc,argv);


    MidiMonitor w;
    w.show();


    return a.exec();
}
