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

#ifndef RANGEBANDSLIDER_H
#define RANGEBANDSLIDER_H

#include <QColor>
#include <QSlider>

class QPaintEvent;

/**
 * @brief Horizontal QSlider that colors an active sub-range of its track
 *
 * RangeBandSlider behaves like an ordinary horizontal QSlider for selecting the
 * current value (e.g. an image index), but paints its track in two colors:
 * values inside the active [low, high] range use the "active" color, while
 * values outside it (the skipped images) use the "skipped" color. The handle is
 * still drawn by the platform style, so it keeps its native appearance. The
 * active range is expressed in the same units as the slider value and is
 * updated with setActiveRange().
 */
class RangeBandSlider : public QSlider {
public:
    /**
     * @brief Constructor
     * @param parent Parent widget (optional)
     */
    explicit RangeBandSlider(QWidget *parent = nullptr) : QSlider(Qt::Horizontal, parent) {}

    /**
     * @brief Destructor
     */
    ~RangeBandSlider() override = default;

    RangeBandSlider(const RangeBandSlider &)            = delete;
    RangeBandSlider(RangeBandSlider &&)                 = delete;
    RangeBandSlider &operator=(const RangeBandSlider &) = delete;
    RangeBandSlider &operator=(RangeBandSlider &&)      = delete;

    /**
     * @brief Set the active value range to highlight
     * @param low  First in-range value (inclusive, in slider value units)
     * @param high Last in-range value (inclusive, in slider value units)
     */
    void setActiveRange(int low, int high);

protected:
    /**
     * @brief Paint a two-color track plus the native handle
     * @param event The paint event (unused)
     */
    void paintEvent(QPaintEvent *event) override;

private:
    int activeLow  = 0;         ///< first in-range value
    int activeHigh = (1 << 30); ///< last in-range value (large => fully active by default)
    QColor activeColor{0x2a, 0x82, 0xda};  ///< color of the active range
    QColor skippedColor{0xb0, 0x4a, 0x4a}; ///< color of the skipped ranges
};
#endif
// Local Variables:
// c-basic-offset: 4
// End:
