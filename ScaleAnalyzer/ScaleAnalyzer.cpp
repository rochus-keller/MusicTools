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

#include "ScaleAnalyzer.h"
#include <bitset>

ScaleAnalyzer::ScaleAnalyzer():scales(ScaleWidth)
{

}

void ScaleAnalyzer::analyze(bool removeRotationSymmetricals)
{
    for( int i = 0; i < ScaleWidth; i++ )
        scales[i].clear();

    for( Scale s = 1; s <= MaxScale; s++ ) // not interesated in NullScale
    {
        // keep all scales which include the first note
        if( s & 0x1 )
            // this includes all possible rotations of scales which contain the first note
            // (i.e. only the subset of all chromatically possible rotations where each scale
            // contains the first not), e.g. all diatonic modes. There are OneCount() such rotations,
            // i.e. of the maximum 12 possible chromatic rotations only OneCount() include the first note.
            scales[OneCount(s)-1] << s;
    }

    if( removeRotationSymmetricals )
    {
        // remove all rotation symmetric patterns
        for( int i = 0; i < ScaleWidth; i++ )
        {
            QList<Scale>& sm = scales[i];
            for( int j = 0; j < sm.size(); j++ )
            {
                const Scale cur = sm[j];
                if( cur == 0 )
                    continue; // already removed
                for( int k = j + 1; k < sm.size(); k++ )
                {
                    const int rot = Rotation(cur,sm[k]);
                    if( rot >= 0 )
                        sm[k] = 0; // remove all rotation symmetric equivalents of cur
                }
            }
            QList<Scale> res;
            for( int j = 0; j < sm.size(); j++ )
                if( sm[j] )
                    res << sm[j];
            qSort(res);
            sm = res;
        }
    }else
    {
        for( int i = 0; i < ScaleWidth; i++ )
            qSort(scales[i]);
    }
#if 0
    qDebug() << "*** Scale counts:";
    for( int i = 0; i < ScaleWidth; i++ )
        qDebug() << "  " << i+1 << "notes:" << scales[i].size() << "different scales";
#endif
}

QList<ScaleAnalyzer::Scale> ScaleAnalyzer::allScales() const
{
    QList<Scale> res;
    for( int i = 0; i < ScaleWidth; i++ )
        res += scales[i];
    return res;
}

int ScaleAnalyzer::OneCount(unsigned int u)
{
    // http://blogs.msdn.com/b/jeuge/archive/2005/06/08/hakmem-bit-count.aspx
    unsigned int uCount;

    uCount = u - ((u >> 1) & 033333333333) - ((u >> 2) & 011111111111);
    return ((uCount + (uCount >> 3)) & 030707070707) % 63;
}

static inline int fixN(int n)
{
    if( n < 0 )
        n += ( -n / ScaleAnalyzer::ScaleWidth + 1) * ScaleAnalyzer::ScaleWidth;
    if( n >= ScaleAnalyzer::ScaleWidth )
        n = n % ScaleAnalyzer::ScaleWidth;
    return n;
}

ScaleAnalyzer::Scale ScaleAnalyzer::Rotated(ScaleAnalyzer::Scale s, int n)
{
    n = fixN(n);
    return (s >> n) | ((s << (ScaleAnalyzer::ScaleWidth - n)) & 0xfff);
}

ScaleAnalyzer::Scale ScaleAnalyzer::Inverted(ScaleAnalyzer::Scale s, int n)
{
    if( n < 0 )
        return s;
    n = fixN(n);
    std::bitset<16> bs(s);

    if( n % 2 == 0 )
    {
        // 0<=>0, 1<=>11, 2<=>10, ... , 6<=>6
        n = n / 2;
        for( int i = 1; i < ScaleWidth/2; i++ )
        {
            const int pos1 = (i+n) % ScaleWidth;
            const int pos2 = (12-i+n) % ScaleWidth;
            const bool tmp = bs.test( pos1 );
            bs.set(pos1,bs.test(pos2));
            bs.set(pos2,tmp);
        }
    }else
    {
        // axis==1: 0<=>1,11<=>2,10<=>3,9<=>4,8<=>5,7<=>6
        n = n / 2 + 1;
        for( int i = 0; i < ScaleWidth/2; i++ )
        {
            const int pos1 = (i+n) % ScaleWidth;
            const int pos2 = (12-i+n-1) % ScaleWidth;
            const bool tmp = bs.test( pos1 );
            bs.set(pos1,bs.test(pos2));
            bs.set(pos2,tmp);
        }
    }
    //qDebug() << "inverted" << toBinString(s) << toSteps(s) << "to" << toBinString(bs.to_ulong()) << toSteps(bs.to_ulong());
    return bs.to_ulong();
}

int ScaleAnalyzer::Rotation(ScaleAnalyzer::Scale ref, ScaleAnalyzer::Scale other)
{
    for( int i = 0; i < ScaleAnalyzer::ScaleWidth; i++ )
    {
        if( Rotated(other,i) == ref )
            return i;
    }
    return -1;
}

int ScaleAnalyzer::HalfStep(Scale s)
{
    int res = 0;
    const bool firstBit = s & 0x1;
    bool lastBit = firstBit;
    for( int i = 1; i < ScaleWidth; i++ )
    {
        s = s >> 1;
        if( s & 0x1 )
        {
            if( lastBit )
                res++;
            lastBit = true;
        }else
            lastBit = false;
    }
    if( firstBit && s & 0x1 )
        res++; // if scale starts with C, if B is also included, this is a half step modulo 12
    return res;
}

QByteArray ScaleAnalyzer::toBinString(ScaleAnalyzer::Scale s)
{
    const QByteArray bin = QByteArray::number(s,2);
    QByteArray res;
    for( int i = bin.size() - 1; i >= 0; i-- )
        res += bin[i];
    return res + QByteArray(ScaleWidth - bin.size(),'0');
}

QByteArray ScaleAnalyzer::toPcSet(ScaleAnalyzer::Scale s)
{
    QByteArray res = "[";
    bool first = true;
    for( int i = 0; i < ScaleWidth; i++ )
    {
        if( ( s >> i ) & 0x1 )
        {
            if( !first )
                res += ",";
            first = false;
            res += QByteArray::number(i);
        }
    }
    res += "]";
    return res;
}

ScaleAnalyzer::Scale ScaleAnalyzer::fromSteps(QByteArray str)
{
    str = str.simplified();
    str.replace('-',"");
    str.replace(' ',"");
    if( str.isEmpty() )
        return 0;
    int count = 0;
    QByteArray binary;
    binary.prepend('1');
    for( int i = 0; i < str.size(); i++ )
    {
        if( !::isdigit(str[i]) )
            return 0;
        const int diff = str[i] - '0';
        count += diff;
        for( int j = 0; j < diff - 1; j++ )
            binary.prepend('0');
        binary.prepend('1');
    }
    if( binary.size() > 13 || count > 12 )
        return 0;
    return binary.toInt(0,2) & 0xfff;
}

QByteArray ScaleAnalyzer::toSteps(ScaleAnalyzer::Scale s)
{
    QByteArray res;
    if( (s & 0x1) == 0 )
        return res; // TODO: should we rotate instead?
    int last = 0;
    for( int i = 1; i < ScaleWidth; i++ )
    {
        const bool one = (s >> i) & 0x1;
        if( one )
        {
            if( !res.isEmpty() )
                res += "-";
            res += QByteArray::number(i-last);
            last = i;
            if( i == ScaleWidth-1 )
                res += "-1";
        }
    }
    return res;
}

bool ScaleAnalyzer::isOn(ScaleAnalyzer::Scale s, quint8 semitone)
{
    semitone = semitone % 12;
    return (s >> semitone) & 0x1;
}

bool ScaleAnalyzer::isWhiteOn(ScaleAnalyzer::Scale s, quint8 whiteNr)
{
#if 0
    whiteNr = whiteNr % 7;
    static const int whites[] = { 0,2,4,5,7,9,11 };
    return isOn(s, whites[whiteNr]);
#else
    const quint8 key = whiteToChromatic(s, whiteNr);
    return isOn(s, key);
#endif
}

bool ScaleAnalyzer::isBlackOn(ScaleAnalyzer::Scale s, quint8 blackNr)
{
#if 0
    blackNr = blackNr % 5;
    static const int blacks[] = { 1,3,6,8,10 };
    return isOn(s, blacks[blackNr]);
#else
    const quint8 key = blackToChromatic(s, blackNr);
    return isOn(s, key);
#endif
}

quint8 ScaleAnalyzer::whiteToChromatic(ScaleAnalyzer::Scale s, quint8 whiteNr)
{
    const quint8 modulo = whiteNr % 7;
    const quint8 divide = whiteNr / 7;
    static const int whites[] = { 0,2,4,5,7,9,11 };
    return whites[modulo] + divide * 12;
}

quint8 ScaleAnalyzer::blackToChromatic(ScaleAnalyzer::Scale s, quint8 blackNr)
{
    const quint8 modulo = blackNr % 5;
    const quint8 divide = blackNr / 5;
    static const int blacks[] = { 1,3,6,8,10 };
    return blacks[modulo] + divide * 12;
}


