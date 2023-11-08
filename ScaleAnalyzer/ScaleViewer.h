#ifndef SCALEVIEWER_H
#define SCALEVIEWER_H

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

#include <QDialog>
#include <QScrollArea>
#include "ScaleAnalyzer.h"

class Keyboard : public QWidget
{
    Q_OBJECT
public:
    Keyboard(quint8 octaves, ScaleAnalyzer::Scale scale = 0, QWidget* p = 0);
    ScaleAnalyzer::Scale getScale() const { return scale; }
    void setMark(bool on = true);
    void setScale(ScaleAnalyzer::Scale s);
signals:
    void activated(ScaleAnalyzer::Scale);
    void selected(ScaleAnalyzer::Scale);
    void keyState(ScaleAnalyzer::Scale, quint8, bool);
protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent *);
private:
    quint8 octaves;
    bool marked;
    qint8 pressed;
    ScaleAnalyzer::Scale scale;
    QVector<QRect> rects;
};

class QSpinBox;
class QLabel;
class QLineEdit;
class QCheckBox;

class ScaleViewer : public QWidget
{
    Q_OBJECT
public:
    explicit ScaleViewer(QWidget *parent = 0);
    ~ScaleViewer();

    void listScales();

protected slots:
    void onActivated(ScaleAnalyzer::Scale);
    void onSelected(ScaleAnalyzer::Scale);
    void onKey(ScaleAnalyzer::Scale, quint8, bool);
    void onSearch();
    void onSelect();
    void onNoRotation();
    void onQuery1();
    void onSelectSf();
    void onSelectOut();

protected:
    void fillList(const QList<ScaleAnalyzer::Scale>&);

private:
    ScaleAnalyzer sa;
    QScrollArea* scroller;
    QSpinBox* notesPerScale;
    QSpinBox* halfStepsPerScale;
    QSpinBox* toneLen;
    QSpinBox* playOctaves;
    QCheckBox* playKeys;
    QCheckBox* uniqueOnly;
    QLabel* text;
    QLineEdit* stepString;
    QWidget* pane;
    class Imp;
    Imp* imp;
    QList<Keyboard*> keyboards;
    ScaleAnalyzer::Scale cur;
};

class ChromaticCircle : public QWidget
{
    Q_OBJECT
public:
    ChromaticCircle(QWidget* p);
    void setScale(ScaleAnalyzer::Scale);
    ScaleAnalyzer::Scale getScale() const { return scale; }

signals:
    void keyState(ScaleAnalyzer::Scale, quint8, bool);
    void flipped();

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);

protected slots:
    void left();
    void right();
    void flip();
    void clear();

private:
    ScaleAnalyzer::Scale scale;
    QVector<QRect> rects;
    qint8 pressed, axis;
};

class KeyboardSelector : public QDialog
{
    Q_OBJECT
public:
    KeyboardSelector(QWidget* p, ScaleAnalyzer::Scale = 0);
    ScaleAnalyzer::Scale getScale() const;

signals:
    void keyState(ScaleAnalyzer::Scale, quint8, bool);

protected slots:
    void onKey(ScaleAnalyzer::Scale, quint8, bool);
    void onEdit();
    void flipped();

private:
    Keyboard* kb;
    ChromaticCircle* c;
    QLineEdit* stepString;
};

#endif // SCALEVIEWER_H
