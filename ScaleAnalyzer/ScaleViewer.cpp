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

#include "ScaleViewer.h"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <bitset>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QToolBar>
#include <QInputDialog>
#include "FlowLayout.h"
#include <math.h>

#define MIN_EVENT_DURATION 60

#ifdef USING_FLUID_SYNTH
#include <fluidsynth.h>
class ScaleViewer::Imp
{
public:
    Imp()
    {
        d_settings = new_fluid_settings();
        fluid_settings_setstr(d_settings, "audio.driver", "pulseaudio");

        d_synth = new_fluid_synth(d_settings);

        d_audio = new_fluid_audio_driver(d_settings, d_synth);
        //fluid_synth_sfload(d_synth, "/home/me/Downloads/FullConcertGrandV3.sf2", 1);
        //fluid_synth_sfload(d_synth, "/home/me/Downloads/Chateau Grand Lite-v1.0.sf2", 1);

        fluid_synth_cc( d_synth, 0, 7, 100 );
        fluid_synth_set_gain( d_synth, 0.8 );
        fluid_synth_set_reverb_on( d_synth, 0 );

        d_seq = new_fluid_sequencer ();
        d_dest = fluid_sequencer_register_fluidsynth( d_seq, d_synth );    }
    ~Imp()
    {
        delete_fluid_sequencer( d_seq );
        if( d_audio )
            delete_fluid_audio_driver( d_audio );
        delete_fluid_synth(d_synth);
        delete_fluid_settings(d_settings);
    }
    void loadSound(const QString& path)
    {
        fluid_synth_sfload(d_synth, path.toUtf8().constData(), 1);
    }
    void playNote(quint8 chan, quint8 key, quint8 velo, quint32 start, quint16 duration)
    {
        fluid_event_t *ev = new_fluid_event();
        fluid_event_set_source( ev, -1 );
        fluid_event_set_dest( ev, d_dest );
        if( velo )
            fluid_event_noteon( ev, chan, key, velo );
        else
            fluid_event_noteoff( ev, chan, key );
        fluid_sequencer_send_at( d_seq, ev, start, 0 );
        if( duration )
        {
            fluid_event_noteoff( ev, chan, key );
            fluid_sequencer_send_at( d_seq, ev, start + duration, 0 );
        }
        delete_fluid_event (ev);
    }
    QStringList getPorts() { return QStringList(); }
private:
    fluid_settings_t* d_settings;
    fluid_synth_t* d_synth;
    fluid_audio_driver_t* d_audio;
    fluid_sequencer_t* d_seq;
    short d_dest;
};
#else
#include <QLinkedList>
#include <QElapsedTimer>
#include <RtMidi.h>
class ScaleViewer::Imp : public QObject
{
    struct Event
    {
        quint8 chan, key, velo;
        quint32 time;
        Event(qint32 t = 0, quint8 c = 0, quint8 k = 0, quint8 v = 0):time(t),chan(c),key(k),velo(v){}
    };

    QLinkedList<Event> queue;
    enum { delta = MIN_EVENT_DURATION/10 };
    RtMidiOut out;
    QElapsedTimer timer;
public:
    Imp()
    {
        timer.start();
        startTimer(delta);
    }

    void playNote(quint8 chan, quint8 key, quint8 velo, quint32 start, quint16 duration)
    {
        start += timer.elapsed();
        QLinkedList<Event>::iterator i = queue.begin();
        while( i != queue.end() && (*i).time < start )
            ++i;
        queue.insert(i,Event(start,chan,key,velo));
        //qDebug() << "added on" << start << queue.size();
        start += duration;
        while( i != queue.end() && (*i).time < start )
            ++i;
        queue.insert(i,Event(start,chan,key,0));
        //qDebug() << "added off" << start << queue.size();
    }

    void timerEvent(QTimerEvent * event)
    {
        const quint32 time = timer.elapsed();
        while( !queue.isEmpty() && time >= queue.front().time )
        {
            quint8 msg[3];
            Event e = queue.front();
            if( e.velo > 0 )
                msg[0] = 0x90 | e.chan;
            else
                msg[0] = 0x80 | e.chan;
            msg[1] = e.key;
            msg[2] = e.velo;
            out.sendMessage(msg,3);
            //qDebug() << "play & remove" <<  queue.front().time << queue.size();
            queue.pop_front();
        }
    }
    QStringList getPorts()
    {
        const size_t nPorts = out.getPortCount();
        QStringList res;
        for( int i = 0; i < nPorts; i++ )
            res << out.getPortName(i).c_str();
        return res;
    }
    void loadSound(const QString& name)
    {
        const size_t nPorts = out.getPortCount();
        for( int i = 0; i < nPorts; i++ )
        {
            const QString port = out.getPortName(i).c_str();
            if( port == name )
            {
                out.closePort();
                out.openPort(i);
                return;
            }
        }
        qCritical() << "could not open MIDI output" << name;
    }
};
#endif

ScaleViewer::ScaleViewer(QWidget *parent) : QWidget(parent), pane(0), cur(0)
{
    imp = new Imp();

    QVBoxLayout* vbox = new QVBoxLayout(this);

    QHBoxLayout* hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    notesPerScale = new QSpinBox(this);
    notesPerScale->setMaximum(12);
    notesPerScale->setMinimum(0);
    notesPerScale->setValue(7);
    hbox->addWidget(new QLabel(tr("Notes per scale (0..any):"))); // cardinality
    hbox->addWidget(notesPerScale);

    halfStepsPerScale = new QSpinBox(this);
    halfStepsPerScale->setMaximum(12);
    halfStepsPerScale->setMinimum(-1);
    halfStepsPerScale->setValue(2);
    hbox->addWidget(new QLabel(tr("Half steps per scale (-1..any):")));
    hbox->addWidget(halfStepsPerScale);

#if 0
    stepString = new QLineEdit(this);
    stepString->setMaxLength(24);
    hbox->addWidget(new QLabel(tr("Step list:")));
    hbox->addWidget(stepString);
#endif

    uniqueOnly = new QCheckBox(tr("No rotation-equivalents"),this);
    hbox->addWidget(uniqueOnly);
    connect(uniqueOnly,SIGNAL(toggled(bool)),this,SLOT(onNoRotation()));

    QPushButton* search = new QPushButton("Search",this);
    connect(search,SIGNAL(clicked(bool)),this,SLOT(onSearch()));
    hbox->addWidget(search);

    QPushButton* queries = new QPushButton("Queries",this);
    hbox->addWidget(queries);

    QMenu* m = new QMenu(this);
    m->addAction("Steps to first rotation-equivalent < number of notes",this,SLOT(onQuery1()));
    queries->setMenu(m);

    QPushButton* select = new QPushButton("Select",this);
    connect(select,SIGNAL(clicked(bool)),this,SLOT(onSelect()));
    hbox->addWidget(select);

    hbox->addStretch();

    hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    toneLen = new QSpinBox(this);
    toneLen->setMaximum(1000);
    toneLen->setMinimum(MIN_EVENT_DURATION);
    toneLen->setValue(180);
    hbox->addWidget(new QLabel(tr("Note length [ms]:")));
    hbox->addWidget(toneLen);

    playOctaves = new QSpinBox(this);
    playOctaves->setMaximum(5);
    playOctaves->setMinimum(1);
    playOctaves->setValue(1);
    hbox->addWidget(new QLabel(tr("Octaves to play:")));
    hbox->addWidget(playOctaves);

    playKeys = new QCheckBox(tr("play clicked"), this);
    hbox->addWidget(playKeys);

#ifdef USING_FLUID_SYNTH
    QPushButton* sf = new QPushButton("Sound",this);
    connect(sf,SIGNAL(clicked(bool)),this,SLOT(onSelectSf()));
    hbox->addWidget(sf);
    QSettings set;
    const QString path = set.value("SoundFont").toString();
    if( !path.isEmpty() )
    {
        imp->loadSound(path);
        sf->setToolTip(tr("Sound: %1").arg(path));
    }else
        sf->setToolTip(tr("No sound selected"));
#else
    QPushButton* mo = new QPushButton("MIDI Out",this);
    connect(mo,SIGNAL(clicked(bool)),this,SLOT(onSelectOut()));
    hbox->addWidget(mo);
    QSettings set;
    const QString path = set.value("MidiOut").toString();
    if( !path.isEmpty() )
    {
        imp->loadSound(path);
        mo->setToolTip(tr("MIDI Out: %1").arg(path));
    }else
        mo->setToolTip(tr("No output selected"));
#endif

    text = new QLabel(this);
    hbox->addWidget(text);

    hbox->addStretch();

    scroller = new QScrollArea(this);
    scroller->setWidgetResizable(true);
    vbox->addWidget(scroller);

    sa.analyze();

}

ScaleViewer::~ScaleViewer()
{
    delete imp;
}

void ScaleViewer::listScales()
{
#if 0
    if( pane )
    {
        keyboards.clear();
        delete pane;
    }
    const ScaleAnalyzer::Scale id = 0; // ScaleAnalyzer::fromSteps(stepString->text().toLatin1());
    pane = new QWidget(this);
    FlowLayout* vbox = new FlowLayout(pane,1,2,2);
    const int idx = notesPerScale->value();
    QList<ScaleAnalyzer::Scale> scales;
    if( idx )
        scales = sa.getScales(idx);
    else
        scales = sa.allScales();
    for( int i = 0; i < scales.size(); i++ )
    {
        if( halfStepsPerScale->value() >= 0 && ScaleAnalyzer::HalfStep(scales[i]) != halfStepsPerScale->value() )
            continue;
        if( id && scales[i] != id )
            continue;
        Keyboard* k = new Keyboard(2,scales[i],pane);
        k->setFixedSize(197,40);
        vbox->addWidget(k);
        connect(k,SIGNAL(activated(ScaleAnalyzer::Scale)),this,SLOT(onActivated(ScaleAnalyzer::Scale)));
        connect(k,SIGNAL(selected(ScaleAnalyzer::Scale)),this, SLOT(onSelected(ScaleAnalyzer::Scale)));
        connect(k,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this, SLOT(onKey(ScaleAnalyzer::Scale,quint8,bool)));
        keyboards.append(k);
    }
    scroller->setWidget(pane);
    text->setText(tr("%1 scales found").arg(keyboards.size()));
#else
    QList<ScaleAnalyzer::Scale> scales;
    const int idx = notesPerScale->value();
    if( idx )
        scales = sa.getScales(idx);
    else
        scales = sa.allScales();
    if( halfStepsPerScale->value() >= 0 )
    {
        QList<ScaleAnalyzer::Scale> filtered;
        for( int i = 0; i < scales.size(); i++ )
        {
            if( ScaleAnalyzer::HalfStep(scales[i]) == halfStepsPerScale->value() )
                filtered << scales[i];
        }
        scales = filtered;
    }
    fillList(scales);
#endif
}

void ScaleViewer::onActivated(ScaleAnalyzer::Scale scale)
{
    int t = 0;
    const int len = toneLen->value();
    int i;
    for( i = 0; i < (12 * playOctaves->value()); i++ )
    {
        if( ScaleAnalyzer::isOn(scale,i) )
        {
            imp->playNote(0, 60 + i, 100, t, len );
            t += len;
        }
    }
    if( scale & 0x1 )
        imp->playNote(0, 60 + i, 100, t, len );
}

void ScaleViewer::onSelected(ScaleAnalyzer::Scale s)
{
    cur = s;
    int count = 0;
    if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        s = ScaleAnalyzer::Inverted(s);
    else if( QApplication::keyboardModifiers() == ( Qt::ControlModifier | Qt::ShiftModifier ) )
        s = ScaleAnalyzer::Inverted(s,1);
    for( int i = 0; i < keyboards.size(); i++ )
    {
        const int rot = ScaleAnalyzer::Rotation(s, keyboards[i]->getScale());
        const bool select = rot >= 0;
        if( rot == 0 )
        {
            keyboards[i]->setFocus();
            scroller->ensureWidgetVisible(keyboards[i]);
        }
        keyboards[i]->setMark( select );
        if( select )
            count++;

    }
    text->setText(tr("%1 rotation-equivalent scales selected").arg(count));
}

void ScaleViewer::onKey(ScaleAnalyzer::Scale scale, quint8 key, bool on)
{
    if( playKeys->isChecked() )
        imp->playNote(0,60+key,on?100:0,0,0);
}

void ScaleViewer::onSearch()
{
    listScales();
}

void ScaleViewer::onSelect()
{
    KeyboardSelector dlg(this, cur);
    connect(&dlg,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this,SLOT(onKey(ScaleAnalyzer::Scale,quint8,bool)));
    if( dlg.exec() == QDialog::Accepted )
        onSelected(dlg.getScale());
}

void ScaleViewer::onNoRotation()
{
    sa.analyze(uniqueOnly->isChecked());
}

void ScaleViewer::onQuery1()
{
    ScaleAnalyzer sa;
    sa.analyze(true);
    QList<ScaleAnalyzer::Scale> scales = sa.allScales();
    QList<ScaleAnalyzer::Scale> res;
    for( int i = 0; i < scales.size(); i++ )
    {
        ScaleAnalyzer::Scale s = scales[i];
        const int count = ScaleAnalyzer::OneCount(s);
        if( notesPerScale->value() && notesPerScale->value() != count )
            continue;
        if( halfStepsPerScale->value() >= 0 && halfStepsPerScale->value() != ScaleAnalyzer::HalfStep(s) )
            continue;
        int off = 1;
        while( off < count )
        {
            if( ScaleAnalyzer::Rotated(s,off) == s )
                break;
            off++;
        }
        if( off < count )
            res << s;
    }
    qSort(res);
    fillList(res);
}

void ScaleViewer::onSelectSf()
{
    const QString  path = QFileDialog::getOpenFileName(this,tr("Select Sound Font"), QString(), "Sound Font (*.sf*)");
    if( path.isEmpty() )
        return;
    QSettings set;
    set.setValue("SoundFont", path);
    imp->loadSound(path);
    QWidget* w = (QWidget*)sender();
    w->setToolTip(tr("Sound: %1").arg(path));
}

void ScaleViewer::onSelectOut()
{
    const QStringList list = imp->getPorts();
    bool ok;
    const QString port = QInputDialog::getItem(this,tr("Select Output Port"),tr("Port:"),list,0,false,&ok);
    if(!ok)
        return;
    QSettings set;
    set.setValue("MidiOut", port);
    imp->loadSound(port);
    QWidget* w = (QWidget*)sender();
    w->setToolTip(tr("MIDI Out: %1").arg(port));
}

void ScaleViewer::fillList(const QList<ScaleAnalyzer::Scale>& scales)
{
    cur = 0;
    if( pane )
    {
        keyboards.clear();
        delete pane;
    }
    pane = new QWidget(this);
    FlowLayout* vbox = new FlowLayout(pane,1,2,2);
    for( int i = 0; i < scales.size(); i++ )
    {
        Keyboard* k = new Keyboard(2,scales[i],pane);
        k->setFixedSize(197,40);
        vbox->addWidget(k);
        connect(k,SIGNAL(activated(ScaleAnalyzer::Scale)),this,SLOT(onActivated(ScaleAnalyzer::Scale)));
        connect(k,SIGNAL(selected(ScaleAnalyzer::Scale)),this, SLOT(onSelected(ScaleAnalyzer::Scale)));
        connect(k,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this, SLOT(onKey(ScaleAnalyzer::Scale,quint8,bool)));
        keyboards.append(k);
    }
    scroller->setWidget(pane);
    text->setText(tr("%1 scales found").arg(keyboards.size()));
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ScaleViewer v;
    v.show();

    return a.exec();
}

Keyboard::Keyboard(quint8 octaves, ScaleAnalyzer::Scale scale, QWidget* p):
    QWidget(p),octaves(octaves),scale(0),marked(false),pressed(-1)
{
    setFocusPolicy(Qt::StrongFocus);
    rects.resize(octaves*12);
    setScale(scale);
}

void Keyboard::setMark(bool on)
{
    marked = on;
    update();
}

void Keyboard::setScale(ScaleAnalyzer::Scale s)
{
    scale = s;
    QString str;
    QTextStream out(&str);
    out << "Scale #" << QByteArray::number(s,16).toUpper() << endl;
    out << "Half steps: " << ScaleAnalyzer::toSteps(s) << endl;
    out << "Pattern: " << ScaleAnalyzer::toBinString(s) << endl;
    out << "Pitch-class set: " << ScaleAnalyzer::toPcSet(s);
    setToolTip(str);

    update();
}

void Keyboard::paintEvent(QPaintEvent* )
{
    QPainter p(this);

    const int whiteKeyCount = octaves * 7;
    const int whiteKeyWidth = width() / whiteKeyCount;
    const int blackKeyWidth = whiteKeyWidth * 0.7;
    const int blackKeyHeight = height() * 0.6;
    const int radius = whiteKeyWidth * 0.6 / 2.0 - 1;

    // background
    p.fillRect(0,0,width(),height(), Qt::white);
    p.drawRect(0,0,whiteKeyCount * whiteKeyWidth - 1, height()-1 );

    // white keys
    int x = 0;
    for( int i = 0; i < whiteKeyCount; i++ )
    {
        const QRect r(x,0,whiteKeyWidth, height()-1);
        if( marked )
            p.setBrush(Qt::yellow);
        else
            p.setBrush(Qt::white);
        p.setPen(Qt::black);
        p.drawRect(r);
        const quint8 key = ScaleAnalyzer::whiteToChromatic(scale,i);
        rects[key] = r;
        if( ScaleAnalyzer::isOn(scale,key) )
        {
            const QRect r2(r.x()+2,r.bottom()-r.width()+3,r.width()-3,r.width()-3);
            p.setBrush(Qt::red);
            p.setPen(Qt::red);
            p.drawEllipse(r2.center(),radius,radius);
        }
        if( pressed == key )
            p.fillRect(r,Qt::blue);
        x += whiteKeyWidth;
    }

    // black keys
    static const int blacks[] = { 1,2,4,5,6 };
    for( int octave = 0; octave < octaves; octave++ )
    {
        for( int i = 0; i < 5; i++ )
        {
            const int black = blacks[i] + octave * 7;
            const QRect r( black * whiteKeyWidth - blackKeyWidth/2, 0, blackKeyWidth-1, blackKeyHeight-1 );
            p.setPen(Qt::black);
            p.fillRect(r, Qt::black);
            const quint8 key = ScaleAnalyzer::blackToChromatic(scale,i) + octave*12;
            rects[key] = r;
            if( ScaleAnalyzer::isOn(scale,key) )
            {
                const QRect r2(r.x()+3,r.bottom()-r.width()+3,r.width()-4,r.width()-4);
                p.setBrush(Qt::red);
                p.setPen(Qt::red);
                p.drawEllipse(r2.center(),radius,radius);
            }
            if( pressed == key )
                p.fillRect(r,Qt::blue);
        }
    }
    if( hasFocus() )
    {
        p.setPen(QPen(Qt::blue,2));
        p.setBrush(Qt::transparent);
        p.drawRect(1,1,whiteKeyCount * whiteKeyWidth - 2, height()-2 );
    }
    if( scale )
    {
        const QByteArray id = QByteArray::number(scale,16).toUpper();
        QFont f = font();
        f.setPixelSize(blackKeyHeight*0.4);
        p.setFont(f);
        p.setPen(Qt::darkRed);
        const QRect r3(rects[3].topRight(),rects[6].bottomLeft());
        p.fillRect(r3.adjusted(2,blackKeyHeight*0.3,-2,-blackKeyHeight*0.3), Qt::white);
        p.drawText(r3,Qt::AlignCenter,id);
    }
}

void Keyboard::mousePressEvent(QMouseEvent* e)
{
    emit selected(scale);
    QList<QPair<int,QRect> > hits;
    for( int i = 0; i < rects.size(); i++ )
    {
        if( rects[i].contains(e->pos()) )
            hits << qMakePair(i,rects[i]);
    }
    if( !hits.isEmpty() )
    {
        int res = 0;
        if( hits.size() > 1 )
        {
            // if more than one is hit - which is possible if black is hit - then prioritize black
            int h = height();
            for( int i = 0; i < hits.size(); i++ )
            {
                if( hits[i].second.height() < h )
                {
                    res = i;
                    h = hits[i].second.height();
                }
            }
        }
        if( pressed != hits[res].first )
            update();
        pressed = hits[res].first;
        emit keyState(scale,pressed,true);
    }
}

void Keyboard::mouseReleaseEvent(QMouseEvent* e)
{
    if( pressed != -1 )
    {
        update();
        emit keyState(scale,pressed,false);
        pressed = -1;
    }
}

void Keyboard::mouseDoubleClickEvent(QMouseEvent*)
{
    emit activated(scale);
}

KeyboardSelector::KeyboardSelector(QWidget* p, ScaleAnalyzer::Scale s):QDialog(p)
{
    setWindowTitle(tr("Select Scale"));

    QVBoxLayout* vbox = new QVBoxLayout(this);
    kb = new Keyboard(1,s,this);
    kb->setFixedHeight(90);
    vbox->addWidget(kb);
    connect(kb,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this,SLOT(onKey(ScaleAnalyzer::Scale,quint8,bool)));
    connect(kb,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)));

    stepString = new QLineEdit(this);
    stepString->setText(ScaleAnalyzer::toSteps(s));
    vbox->addWidget(stepString);
    connect(stepString,SIGNAL(textEdited(QString)),this,SLOT(onEdit()));

    c = new ChromaticCircle(this);
    c->setScale(s);
    vbox->addWidget(c);
    connect(c,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this,SLOT(onKey(ScaleAnalyzer::Scale,quint8,bool)));
    connect(c,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)),this,SIGNAL(keyState(ScaleAnalyzer::Scale,quint8,bool)));
    connect(c,SIGNAL(flipped()),this,SLOT(flipped()));

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vbox->addWidget(bb);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    /*
    QPushButton* b = bb->addButton(tr("left"), QDialogButtonBox::ActionRole);
    connect(b,SIGNAL(clicked(bool)),this,SLOT(left()));
    b = bb->addButton(tr("right"), QDialogButtonBox::ActionRole);
    connect(b,SIGNAL(clicked(bool)),this,SLOT(right()));
    */

    setMinimumSize(296,300);
}

void KeyboardSelector::flipped()
{
    kb->setScale(c->getScale());
    stepString->setText(ScaleAnalyzer::toSteps(c->getScale()));
}

ScaleAnalyzer::Scale KeyboardSelector::getScale() const
{
    return kb->getScale();
}

void KeyboardSelector::onKey(ScaleAnalyzer::Scale s, quint8 key, bool on)
{
    if( !on )
        return;
    std::bitset<16> b(s);
    b.flip(key);
    kb->setScale(b.to_ulong());
    c->setScale(b.to_ulong());
    stepString->setText(ScaleAnalyzer::toSteps(b.to_ulong()));
}

void KeyboardSelector::onEdit()
{
    const ScaleAnalyzer::Scale res = ScaleAnalyzer::fromSteps(stepString->text().toLatin1());
    kb->setScale(res);
    c->setScale(res);
    QPalette p = stepString->palette();
    if( res == 0 && !stepString->text().trimmed().isEmpty() )
        p.setBrush(QPalette::Text, Qt::red);
    else
        p.setBrush(QPalette::Text, Qt::black);
    stepString->setPalette(p);

}

static inline double toRad(double deg)
{
    static const double PI = 3.141592654;
    return deg * PI / 180.0;
}

ChromaticCircle::ChromaticCircle(QWidget* p):QWidget(p),scale(0),rects(24),pressed(-1),axis(-1)
{
    QToolBar* tb = new QToolBar(this);
    tb->setOrientation(Qt::Vertical);
    tb->addAction(tr("left"),this,SLOT(left()));
    tb->addAction(tr("right"),this,SLOT(right()));
    tb->addAction(tr("flip"),this,SLOT(flip()));
    tb->addAction(tr("clear"),this,SLOT(clear()));
    tb->move(0,0);
}

void ChromaticCircle::setScale(ScaleAnalyzer::Scale s)
{
    scale = s;
    update();
}

void ChromaticCircle::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QRect r = rect();
    const int mainRadius = qMin(r.width(),r.height()) * 0.415;
    const int smallRadius = mainRadius * 0.2;
    p.drawEllipse(r.center(),mainRadius,mainRadius);

    for( int i = 0; i < rects.size(); i++ )
    {
        const double rad = toRad((i-rects.size()/4) * 360.0 / double(rects.size()));
        const int x = mainRadius * cos(rad) + 0.5;
        const int y = mainRadius * sin(rad) + 0.5;
        QRect r2(0,0,smallRadius*2,smallRadius*2);
        r2.moveCenter(QPoint(x+r.center().x(),y+r.center().y()));
        rects[i] = r2;
    }
    if( axis >= 0 && axis < 12)
    {
        p.drawLine( rects[axis].center(), rects[axis+12].center() );
    }
    for( int i = 0; i < rects.size(); i++ )
    {
        if( i % 2 != 0 )
            continue;
        const quint8 key = i/2;
        const bool on = ScaleAnalyzer::isOn(scale,key);
        p.setBrush(on ? Qt::black : Qt::white);
        p.setPen(Qt::black);
        p.drawEllipse(rects[i].center(),smallRadius,smallRadius);
        QFont f = font();
        f.setPixelSize(smallRadius*1.4);
        p.setFont(f);
        p.setPen(on ? Qt::white : Qt::black);
        p.drawText(rects[i],Qt::AlignCenter,QByteArray::number(key));
    }
}

void ChromaticCircle::mousePressEvent(QMouseEvent* e)
{
    for( int i = 0; i < rects.size(); i++ )
    {
        const int off = rects[i].width() * 0.2;
        const QRect r = rects[i].adjusted(off, off, -off, -off);
        if( r.contains(e->pos()) )
        {
            if( i % 2 == 0 )
            {
                pressed = i/2;
                emit keyState(scale,pressed,true);
            }
            axis = i % 12;
            update();
            return;
        }
    }
}

void ChromaticCircle::mouseReleaseEvent(QMouseEvent*)
{
    if( pressed != -1 )
    {
        update();
        emit keyState(scale,pressed,false);
        pressed = -1;
    }
}

void ChromaticCircle::left()
{
    scale = ScaleAnalyzer::Rotated(scale,1);
    update();
    emit flipped();
}

void ChromaticCircle::right()
{
    scale = ScaleAnalyzer::Rotated(scale,-1);
    update();
    emit flipped();
}

void ChromaticCircle::flip()
{
    if( axis < 0 )
        return;
    scale = ScaleAnalyzer::Inverted(scale,axis);
    update();
    emit flipped();
}

void ChromaticCircle::clear()
{
    scale = 0;
    update();
    emit flipped();
}
