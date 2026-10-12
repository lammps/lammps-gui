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

#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

// Lightweight 2D line / scatter chart drawn directly with QPainter, with
// no dependency on any Qt charts module.  It consumes the neutral PlotSeries / PlotAxis
// model and reproduces the subset of chart features that LAMMPS-GUI actually uses:
// linear axes with nice-number major ticks, minor subticks, major / minor gridlines,
// printf-style tick labels, axis and chart titles, and multiple line / scatter
// series including dashed reference lines and capped vertical error bars.
// Zoom is programmatic (setXRange / setYRange);
// there is no in-widget mouse interaction (the chart window drives ranges externally).

#include "plotseries.h"

#include <QList>
#include <QSize>
#include <QString>
#include <QWidget>

class QPainter;
class QPaintEvent;
class QRectF;

/** @brief Legend placement: off, or one of the four plot corners */
enum class LegendPos { Off, TopLeft, TopRight, BottomLeft, BottomRight };

/**
 * @brief QPainter-based renderer for the neutral chart model
 *
 * Series objects are referenced, not owned: the caller owns each PlotSeries and
 * registers / unregisters it here, then calls update() after changing its data or style.
 */
class PlotWidget : public QWidget {
public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit PlotWidget(QWidget *parent = nullptr);

    /** @brief Set the chart title (drawn centered at the top) */
    void setTitle(const QString &title);

    /** @brief Set the X-axis title */
    void setXTitle(const QString &title);
    /** @brief Get the X-axis title */
    QString xTitle() const;
    /** @brief Set the Y-axis title */
    void setYTitle(const QString &title);
    /** @brief Get the Y-axis title */
    QString yTitle() const;

    /** @brief Set the displayed X-axis range */
    void setXRange(double min, double max);
    /** @brief Set the displayed Y-axis range */
    void setYRange(double min, double max);

    /** @brief Set the printf-style tick label format of the X-axis */
    void setXLabelFormat(const QString &fmt);

    /** @brief Toggle major and minor gridline visibility on both axes */
    void setGrid(bool major, bool minor);

    /**
     * @brief Place a legend in one of the plot corners (or turn it off)
     *
     * The legend lists each visible, named data series (raw / processed / fit /
     * overlays); reference lines and unnamed marker-only mirrors are excluded.
     */
    void setLegendPos(LegendPos pos);

    /**
     * @brief Style applied to all reference-line labels
     * @param pointSize Label font point size (<= 0 keeps the default size)
     * @param distance  Perpendicular gap between the label and its line, in px
     * @param boxed     Draw a frame + opaque background behind each label
     *
     * The per-line label position along the line comes from each reference
     * series' RefAnchor; this sets the window-wide font/offset/box appearance.
     */
    void setRefLabelStyle(double pointSize, double distance, bool boxed);

    /**
     * @brief Register a series for drawing (non-owning; ignored if already present)
     * @param series Series owned by the caller
     */
    void addSeries(const PlotSeries *series);
    /** @brief Remove a previously registered series (does not delete it) */
    void removeSeries(const PlotSeries *series);
    /** @brief Whether a series is currently registered */
    bool hasSeries(const PlotSeries *series) const;
    /** @brief Unregister all series (does not delete them) */
    void clearSeries();

protected:
    /** @brief Paint the chart onto the widget */
    void paintEvent(QPaintEvent *event) override;
    /** @brief Recommended size */
    QSize sizeHint() const override;

private:
    /** @brief Painting routine used by paintEvent() */
    void doRender(QPainter &p, const QRectF &target) const;

    PlotAxis m_xaxis;                       ///< X-axis configuration
    PlotAxis m_yaxis;                       ///< Y-axis configuration
    QString m_title;                        ///< chart title
    QList<const PlotSeries *> m_series;     ///< registered series (not owned)
    LegendPos m_legendPos = LegendPos::Off; ///< legend placement (corner, or off)
    double m_refLabelSize = 0.0;            ///< reference-label font point size (0 = default)
    double m_refLabelDist = 4.0;            ///< reference-label gap from its line (px)
    bool m_refLabelBoxed  = false;          ///< frame + opaque background behind ref labels
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
