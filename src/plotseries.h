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

#ifndef PLOTSERIES_H
#define PLOTSERIES_H

// Neutral value types describing what to draw on a chart.
// They depend only on Qt value types (QtCore / QtGui).
// PlotWidget consumes these directly.

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>

/** @brief Whether a PlotSeries is drawn as a connected line or as markers */
enum class PlotSeriesType { Line, Scatter };

/**
 * @brief Where a reference-line label sits along its line
 *
 * For a vertical line: Start = top, Center = middle, End = bottom.
 * For a horizontal line: Start = left, Center = center, End = right.
 */
enum class RefAnchor { Start, Center, End };

/**
 * @brief One data series in the neutral chart model
 *
 * Carries the points plus the minimal styling the native renderer needs.
 */
struct PlotSeries {
    PlotSeriesType type = PlotSeriesType::Line; ///< line vs. scatter rendering
    QList<QPointF> points;                      ///< data points in axis coordinates
    QList<double> yerr;                         ///< upper y error per point (empty = none)
    QList<double> yerrLo;                       ///< lower y error per point (empty = symmetric)
    QColor errColor;                            ///< error bar color (invalid = series color)
    qreal errWidth     = 1.5;                   ///< error bar line width
    QColor color       = Qt::black;             ///< line / marker color
    qreal width        = 1.0;                   ///< line width (Line series)
    Qt::PenStyle style = Qt::SolidLine;         ///< line style, e.g. dashed reference lines
    qreal markerSize   = 8.0;                   ///< marker diameter (Scatter series)
    QString name;                               ///< series label (shown in the legend)
    bool visible     = true;                    ///< whether the series is drawn
    bool isReference = false;                   ///< draw as a labeled reference line
    QString refLabel;                           ///< text drawn next to a reference line
    RefAnchor refAnchor = RefAnchor::Start;     ///< where the label sits along the line

    // convenience accessors for the chart code, so callers need not poke the
    // points list directly
    /** @brief Append one (x, y) point */
    void append(double x, double y) { points.append(QPointF(x, y)); }
    /** @brief Replace all points (and drop any error bars, which no longer fit) */
    void replace(const QList<QPointF> &p)
    {
        points = p;
        yerr.clear();
        yerrLo.clear();
    }
    /** @brief Whether the series carries usable error bars */
    bool hasErrors() const { return yerr.size() == points.size(); }
    /** @brief Whether the bars extend by different amounts up and down */
    bool hasAsymErrors() const { return hasErrors() && (yerrLo.size() == points.size()); }
    /** @brief How far the bar of point @p i reaches above the value */
    double errHigh(int i) const { return hasErrors() ? qAbs(yerr[i]) : 0.0; }
    /** @brief How far the bar of point @p i reaches below the value */
    double errLow(int i) const
    {
        if (hasAsymErrors()) return qAbs(yerrLo[i]);
        return errHigh(i);
    }
    /** @brief Number of points */
    int count() const { return static_cast<int>(points.size()); }
    /** @brief Point at index i */
    QPointF at(int i) const { return points.at(i); }
    /** @brief Set visibility */
    void setVisible(bool v) { visible = v; }
    /** @brief Whether the series is visible */
    bool isVisible() const { return visible; }
};

/**
 * @brief One linear value axis in the neutral chart model
 */
struct PlotAxis {
    double min = 0.0;                             ///< lower end of the displayed range
    double max = 1.0;                             ///< upper end of the displayed range
    QString title;                                ///< axis title
    QString labelFormat   = QStringLiteral("%g"); ///< printf-style tick label format
    int subTicks          = 4;                    ///< minor subdivisions between major ticks
    bool gridVisible      = true;                 ///< draw major gridlines
    bool minorGridVisible = true;                 ///< draw minor gridlines
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
