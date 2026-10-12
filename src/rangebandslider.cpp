// -*- c++ -*- /////////////////////////////////////////////////////////////////////////
// LAMMPS-GUI - A Graphical Tool to Learn and Explore the LAMMPS MD Simulation Software
//
// Copyright (c) 2023, 2024, 2025, 2026  Axel Kohlmeyer
//
// Documentation: https://lammps-gui.lammps.org/
// Contact: akohlmey@gmail.com
//
// This software is distributed under the GNU General Public License version 2 or later.
////////////////////////////////////////////////////////////////////////////////////////

#include "rangebandslider.h"

#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStylePainter>

#include <utility>

void RangeBandSlider::setActiveRange(int low, int high)
{
    if (low > high) std::swap(low, high);
    if ((low == activeLow) && (high == activeHigh)) return;
    activeLow  = low;
    activeHigh = high;
    update();
}

void RangeBandSlider::paintEvent(QPaintEvent *event)
{
    // only the horizontal track gets the custom two-color treatment
    if (orientation() != Qt::Horizontal) {
        QSlider::paintEvent(event);
        return;
    }

    QStylePainter painter(this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const QRect groove =
        style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle =
        style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    // map a slider value to the pixel x of the handle center at that value
    const int available = groove.width() - handle.width();
    auto xForValue      = [&](int value) {
        const int pos =
            QStyle::sliderPositionFromValue(minimum(), maximum(), value, available, opt.upsideDown);
        return groove.left() + pos + handle.width() / 2;
    };

    int lo = qBound(minimum(), activeLow, maximum());
    int hi = qBound(minimum(), activeHigh, maximum());
    if (hi < lo) std::swap(lo, hi);

    // a thin, vertically centered track spanning the reachable positions
    const int thickness = qMax(4, groove.height() / 3);
    const int top       = groove.center().y() - thickness / 2 + 1;
    const QRect track(groove.left() + handle.width() / 2, top, available, thickness);

    painter.setRenderHint(QPainter::Antialiasing, true);
    const double radius = thickness / 2.0;
    painter.setPen(Qt::NoPen);

    if ((lo <= minimum()) && (hi >= maximum())) {
        // the active range covers the whole sequence (e.g. a single image):
        // paint the entire track as active so it never shows up as all-skipped
        painter.setBrush(activeColor);
        painter.drawRoundedRect(track, radius, radius);
    } else {
        // skipped (out-of-range) track first, active band painted on top
        painter.setBrush(skippedColor);
        painter.drawRoundedRect(track, radius, radius);

        int axl = xForValue(lo);
        int axr = xForValue(hi);
        // guarantee a visible marker even when the active range is a single frame
        if (axr - axl < thickness) {
            const int mid = (axl + axr) / 2;
            axl           = mid - thickness / 2;
            axr           = axl + thickness;
        }
        axl = qBound(track.left(), axl, track.right());
        axr = qBound(track.left(), axr, track.right());
        if (axr > axl) {
            painter.setBrush(activeColor);
            painter.drawRoundedRect(QRect(axl, top, axr - axl, thickness), radius, radius);
        }
    }

    // draw only the handle, using the native style for its appearance
    opt.subControls = QStyle::SC_SliderHandle;
    painter.drawComplexControl(QStyle::CC_Slider, opt);
}

// Local Variables:
// c-basic-offset: 4
// End:
