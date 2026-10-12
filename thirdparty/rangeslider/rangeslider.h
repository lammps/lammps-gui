/** -*- C++ -*-
 ** Copyright Hoyoung Lee (2024-07-14).
 **
 ** hoyoung.yi@gmail.com
 **
 ** This software is a computer program whose purpose is to provide
 ** "Qt widget of range slider with two handles".
 **
 ** This software is governed by the CeCILL-A license under French law
 ** and abiding by the rules of distribution of free software. You can
 ** use, modify and/or redistribute the software under the terms of the
 ** CeCILL-A license as circulated by CEA, CNRS and INRIA at the following
 ** URL: "http://www.cecill.info".
 **
 ** As a counterpart to the access to the source code and rights to copy,
 ** modify and redistribute granted by the license, users are provided
 ** only with a limited warranty and the software's author, the holder of
 ** the economic rights, and the successive licensors have only limited
 ** liability.
 **
 ** In this respect, the user's attention is drawn to the risks associated
 ** with loading, using, modifying and/or developing or reproducing the
 ** software by the user in light of its specific status of free software,
 ** that may mean that it is complicated to manipulate, and that also
 ** therefore means that it is reserved for developers and experienced
 ** professionals having in-depth computer knowledge. Users are therefore
 ** encouraged to load and test the software's suitability as regards
 ** their requirements in conditions enabling the security of their
 ** systems and/or data to be ensured and, more generally, to use and
 ** operate it in the same conditions as regards security.
 **
 ** The fact that you are presently reading this means that you have had
 ** knowledge of the CeCILL-A license and that you accept its terms.
 */

#ifndef RANGESLIDER_H
#define RANGESLIDER_H

#include <QPoint>
#include <QSlider>
#include <QStyle>

// forward declarations

class QPaintEvent;
class QMouseEvent;

/*
 * Originated from
 * https://www.mail-archive.com/pyqt@riverbankcomputing.com/msg22889.html
 * Modification refered from
 * https://gist.github.com/Riateche/27e36977f7d5ea72cf4f
 */

/**
 * @brief Custom Qt Widget implementing a variant of QSlider that has two instead of one handle
 */
class RangeSlider : public QSlider {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param ot Slider orientation (default: Horizontal)
     * @param parent Parent widget
     */
    RangeSlider(Qt::Orientation ot = Qt::Horizontal, QWidget *parent = nullptr);

    /// Return the rangeslider's current low handle value
    int low() const { return this->lowLimit; }
    /// Set the rangeslider's current low handle value
    void setLow(int low_limit)
    {
        this->lowLimit = low_limit;
        update();
    }
    /// Return the rangeslider's current high handle value
    int high() const { return this->highLimit; }
    /// Set the rangeslider's current high handle value
    void setHigh(int high_limit)
    {
        this->highLimit = high_limit;
        update();
    }

signals:
    /// This signal is emitted when sliderDown is true and one of the two sliders moves.
    // This usually happens when the user is dragging the slider.
    // The values are the new slider positions.
    void sliderMoved(int, int);

protected:
    int lowLimit;                        ///< position of rangeslider's lower handle
    int highLimit;                       ///< position of rangeslider's upper handle
    QStyle::SubControl pressed_control;  ///< currently pressed sub-control (handle)
    int tick_interval;                   ///< interval between tick marks
    QSlider::TickPosition tick_position; ///< position of tick marks relative to slider
    QStyle::SubControl hover_control;    ///< sub-control currently under the mouse cursor
    int click_offset;                    ///< offset from handle center to click position
    int active_slider;                   ///< index of the currently active slider handle

    // based on http://qt.gitorious.org/qt/qt/blobs/master/src/gui/widgets/qslider.cpp

    void paintEvent(QPaintEvent *ev) override;      ///< handle paint event
    void mousePressEvent(QMouseEvent *ev) override; ///< handle mouse press event
    void mouseMoveEvent(QMouseEvent *ev) override;  ///< handle mouse move event

    /// Extract the relevant coordinate from a point based on slider orientation
    int pick(QPoint const &pt) const
    {
        return this->orientation() == Qt::Horizontal ? pt.x() : pt.y();
    }
    /// Convert a pixel position along the slider to a range value
    int pixelPosToRangeValue(int pos);
};

#endif
