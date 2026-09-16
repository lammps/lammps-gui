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

#include "plotwidget.h"

#include "plotaxismath.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>

namespace {

// layout constants (logical pixels)
constexpr double OUTER      = 10.0; ///< outer padding around the whole chart
constexpr double TITLE_GAP  = 8.0;  ///< gap between an axis title and the adjacent labels/plot
constexpr double TITLE_VPAD = 12.0; ///< padding above and below the chart title
constexpr double LABEL_GAP  = 5.0;  ///< gap between tick labels and the axis
constexpr double TICK_LEN   = 5.0;  ///< length of axis tick marks

// legend box layout (logical pixels)
constexpr double LEGEND_INSET  = 8.0;  ///< inset from the top-left plot corner
constexpr double LEGEND_PAD    = 5.0;  ///< padding inside the legend box
constexpr double LEGEND_SWATCH = 20.0; ///< width of a legend color swatch
constexpr double LEGEND_GAP    = 6.0;  ///< gap between swatch and label text

// gridline / frame colors
const QColor MAJOR_GRID(160, 160, 160);
const QColor MINOR_GRID(208, 208, 208);
const QColor FRAME_COLOR(80, 80, 80);

// Ensure a strictly positive span so coordinate mapping never divides by zero.
void ensureSpan(double &min, double &max)
{
    if (!(max > min)) {
        const double mid = 0.5 * (min + max);
        min              = mid - 0.5;
        max              = mid + 0.5;
    }
}

QString labelText(double value, const QString &format)
{
    return QString::fromStdString(PlotAxisMath::formatAxisLabel(value, format.toStdString()));
}

// Pick a tick-label format: keep an explicit integer format when ticks are at
// least 1 apart (e.g. integer time steps); otherwise derive the number of
// decimals from the tick spacing so closely spaced ticks do not collapse to
// identical labels.
QString effectiveFormat(const QString &fmt, double interval)
{
    const bool integerFmt = fmt.contains('d') || fmt.contains('i');
    if (integerFmt && interval >= 1.0) return fmt;
    return QStringLiteral("%.%1f").arg(PlotAxisMath::tickDecimals(interval));
}

} // namespace

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent)
{
    // we paint our own (white) background
    setAutoFillBackground(false);
}

void PlotWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void PlotWidget::setXTitle(const QString &title)
{
    m_xaxis.title = title;
    update();
}

QString PlotWidget::xTitle() const
{
    return m_xaxis.title;
}

void PlotWidget::setYTitle(const QString &title)
{
    m_yaxis.title = title;
    update();
}

QString PlotWidget::yTitle() const
{
    return m_yaxis.title;
}

void PlotWidget::setXRange(double min, double max)
{
    m_xaxis.min = min;
    m_xaxis.max = max;
    update();
}

void PlotWidget::setYRange(double min, double max)
{
    m_yaxis.min = min;
    m_yaxis.max = max;
    update();
}

void PlotWidget::setXLabelFormat(const QString &fmt)
{
    m_xaxis.labelFormat = fmt;
    update();
}

void PlotWidget::setGrid(bool major, bool minor)
{
    m_xaxis.gridVisible = m_yaxis.gridVisible = major;
    m_xaxis.minorGridVisible = m_yaxis.minorGridVisible = minor;
    update();
}

void PlotWidget::setLegendPos(LegendPos pos)
{
    if (m_legendPos != pos) {
        m_legendPos = pos;
        update();
    }
}

void PlotWidget::setRefLabelStyle(double pointSize, double distance, bool boxed)
{
    m_refLabelSize  = pointSize;
    m_refLabelDist  = distance;
    m_refLabelBoxed = boxed;
    update();
}

void PlotWidget::addSeries(const PlotSeries *series)
{
    if (series && !m_series.contains(series)) {
        m_series.append(series);
        update();
    }
}

void PlotWidget::removeSeries(const PlotSeries *series)
{
    if (m_series.removeAll(series) > 0) update();
}

bool PlotWidget::hasSeries(const PlotSeries *series) const
{
    return m_series.contains(series);
}

void PlotWidget::clearSeries()
{
    if (!m_series.isEmpty()) {
        m_series.clear();
        update();
    }
}

void PlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    doRender(p, QRectF(rect()));
}

QSize PlotWidget::sizeHint() const
{
    return {400, 300};
}

void PlotWidget::doRender(QPainter &p, const QRectF &target) const
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(target, Qt::white);

    const QFont baseFont = p.font();
    const QFontMetricsF fm(p.font());
    const double labelH = fm.height();

    // bold font for the axis titles
    QFont axisTitleFont = p.font();
    axisTitleFont.setBold(true);
    const QFontMetricsF fmAT(axisTitleFont);
    const double axisTitleH = fmAT.height();

    // the chart title is bold and a step larger than the axis labels/titles
    QFont chartTitleFont = axisTitleFont;
    if (chartTitleFont.pointSizeF() > 0.0)
        chartTitleFont.setPointSizeF(chartTitleFont.pointSizeF() * 1.25);
    else if (chartTitleFont.pixelSize() > 0)
        chartTitleFont.setPixelSize(static_cast<int>(chartTitleFont.pixelSize() * 1.25));
    const QFontMetricsF fmCT(chartTitleFont);
    const double chartTitleH = fmCT.height();

    // axis ranges (guarded against a zero span)
    double xmin = m_xaxis.min, xmax = m_xaxis.max;
    double ymin = m_yaxis.min, ymax = m_yaxis.max;
    ensureSpan(xmin, xmax);
    ensureSpan(ymin, ymax);

    // major tick values for both axes
    const double xMajor              = PlotAxisMath::niceTickInterval(xmax - xmin);
    const double yMajor              = PlotAxisMath::niceTickInterval(ymax - ymin);
    const std::vector<double> xticks = PlotAxisMath::tickValues(xmin, xmax, xMajor);
    const std::vector<double> yticks = PlotAxisMath::tickValues(ymin, ymax, yMajor);

    // tick label formats chosen from the spacing so adjacent ticks stay distinct
    const QString xfmt = effectiveFormat(m_xaxis.labelFormat, xMajor);
    const QString yfmt = effectiveFormat(m_yaxis.labelFormat, yMajor);

    // the tick labels, formatted and measured once: they are needed for the
    // margins here and for drawing below, and this runs on every repaint
    std::vector<QString> xlabels, ylabels;
    std::vector<double> xlabelW, ylabelW;
    xlabels.reserve(xticks.size());
    xlabelW.reserve(xticks.size());
    ylabels.reserve(yticks.size());
    ylabelW.reserve(yticks.size());
    for (double v : xticks) {
        xlabels.push_back(labelText(v, xfmt));
        xlabelW.push_back(fm.horizontalAdvance(xlabels.back()));
    }
    for (double v : yticks) {
        ylabels.push_back(labelText(v, yfmt));
        ylabelW.push_back(fm.horizontalAdvance(ylabels.back()));
    }

    // widest Y tick label drives the left margin
    double maxYLabelW = 0.0;
    for (double w : ylabelW)
        maxYLabelW = std::max(maxYLabelW, w);

    // margins
    const bool hasTitle  = !m_title.isEmpty();
    const bool hasXTitle = !m_xaxis.title.isEmpty();
    const bool hasYTitle = !m_yaxis.title.isEmpty();

    const double leftMargin =
        OUTER + (hasYTitle ? axisTitleH + TITLE_GAP : 0.0) + maxYLabelW + LABEL_GAP + TICK_LEN;
    const double bottomMargin =
        OUTER + (hasXTitle ? axisTitleH + TITLE_GAP : 0.0) + labelH + LABEL_GAP + TICK_LEN;
    const double topMargin = hasTitle ? (TITLE_VPAD + chartTitleH + TITLE_VPAD)
                                      : (OUTER + 0.5 * labelH);
    // leave room so the last X tick label is not clipped at the right edge
    const double lastXLabelW = xlabelW.empty() ? 0.0 : xlabelW.back();
    const double rightMargin = OUTER + 0.5 * lastXLabelW;

    QRectF plot(target.left() + leftMargin, target.top() + topMargin,
                target.width() - leftMargin - rightMargin,
                target.height() - topMargin - bottomMargin);
    if (plot.width() <= 1.0 || plot.height() <= 1.0) return; // too small to draw

    // coordinate mapping
    auto mapX = [&](double vx) {
        return plot.left() + (vx - xmin) / (xmax - xmin) * plot.width();
    };
    auto mapY = [&](double vy) {
        return plot.bottom() - (vy - ymin) / (ymax - ymin) * plot.height();
    };

    // gridlines: dashed minor (lighter) first, then solid major (darker)
    auto drawVLines = [&](const std::vector<double> &vals, const QPen &pen) {
        p.setPen(pen);
        for (double v : vals) {
            const double x = mapX(v);
            if (x < plot.left() - 0.5 || x > plot.right() + 0.5) continue;
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    };
    auto drawHLines = [&](const std::vector<double> &vals, const QPen &pen) {
        p.setPen(pen);
        for (double v : vals) {
            const double y = mapY(v);
            if (y < plot.top() - 0.5 || y > plot.bottom() + 0.5) continue;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
    };

    QPen majorPen(MAJOR_GRID);
    majorPen.setWidth(0); // cosmetic 1px, solid
    QPen minorPen(MINOR_GRID);
    minorPen.setWidth(0);
    minorPen.setStyle(Qt::DashLine);

    const int xsub = std::max(0, m_xaxis.subTicks);
    const int ysub = std::max(0, m_yaxis.subTicks);
    if (m_xaxis.minorGridVisible && xsub > 0)
        drawVLines(PlotAxisMath::tickValues(xmin, xmax, xMajor / (xsub + 1)), minorPen);
    if (m_yaxis.minorGridVisible && ysub > 0)
        drawHLines(PlotAxisMath::tickValues(ymin, ymax, yMajor / (ysub + 1)), minorPen);
    if (m_xaxis.gridVisible) drawVLines(xticks, majorPen);
    if (m_yaxis.gridVisible) drawHLines(yticks, majorPen);

    // plot frame
    {
        QPen pen(FRAME_COLOR);
        pen.setWidth(0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(plot);
    }

    // ticks and tick labels
    p.setPen(Qt::black);
    for (std::size_t i = 0; i < xticks.size(); ++i) {
        const double x = mapX(xticks[i]);
        if (x < plot.left() - 0.5 || x > plot.right() + 0.5) continue;
        p.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + TICK_LEN));
        p.drawText(
            QPointF(x - 0.5 * xlabelW[i], plot.bottom() + TICK_LEN + LABEL_GAP + fm.ascent()),
            xlabels[i]);
    }
    for (std::size_t i = 0; i < yticks.size(); ++i) {
        const double y = mapY(yticks[i]);
        if (y < plot.top() - 0.5 || y > plot.bottom() + 0.5) continue;
        p.drawLine(QPointF(plot.left() - TICK_LEN, y), QPointF(plot.left(), y));
        p.drawText(QPointF(plot.left() - TICK_LEN - LABEL_GAP - ylabelW[i],
                           y + 0.5 * fm.ascent() - 0.5 * fm.descent()),
                   ylabels[i]);
    }

    // axis titles: bold, base size
    p.setPen(Qt::black);
    p.setFont(axisTitleFont);
    if (hasXTitle) {
        const double w = fmAT.horizontalAdvance(m_xaxis.title);
        p.drawText(QPointF(plot.center().x() - 0.5 * w, target.bottom() - OUTER - fmAT.descent()),
                   m_xaxis.title);
    }
    if (hasYTitle) {
        p.save();
        const double w = fmAT.horizontalAdvance(m_yaxis.title);
        p.translate(target.left() + OUTER + fmAT.ascent(), plot.center().y() + 0.5 * w);
        p.rotate(-90.0);
        p.drawText(QPointF(0.0, 0.0), m_yaxis.title);
        p.restore();
    }
    // chart title: bold, a step larger, with extra padding above and below
    if (hasTitle) {
        p.setFont(chartTitleFont);
        const double w = fmCT.horizontalAdvance(m_title);
        p.drawText(QPointF(plot.center().x() - 0.5 * w, target.top() + TITLE_VPAD + fmCT.ascent()),
                   m_title);
    }

    // series (clipped to the plot area)
    p.save();
    p.setClipRect(plot);

    // error bars first, so that they stay behind every data curve.  They are
    // drawn in their own color and width (the chart style dialog sets both), so
    // that they stay readable where they overlap the curve they belong to.
    for (const PlotSeries *s : m_series) {
        if (!s || !s->visible || !s->hasErrors()) continue;
        const QColor barColor = s->errColor.isValid() ? s->errColor : s->color;
        const double barWidth = qMax(0.5, s->errWidth);
        // caps grow with the bar so that thick bars do not end in a stub
        const double cap = qMax(3.0, 2.0 * barWidth);
        p.setPen(QPen(barColor, barWidth, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < s->points.size(); ++i) {
            const double eup = s->errHigh(i);
            const double edn = s->errLow(i);
            if (!(eup > 0.0) && !(edn > 0.0)) continue;
            const double px = mapX(s->points[i].x());
            const double lo = mapY(s->points[i].y() - edn);
            const double hi = mapY(s->points[i].y() + eup);
            p.drawLine(QPointF(px, lo), QPointF(px, hi));
            p.drawLine(QPointF(px - cap, lo), QPointF(px + cap, lo));
            p.drawLine(QPointF(px - cap, hi), QPointF(px + cap, hi));
        }
    }

    for (const PlotSeries *s : m_series) {
        if (!s || !s->visible || s->points.isEmpty()) continue;
        if (s->type == PlotSeriesType::Line) {
            QPolygonF poly;
            poly.reserve(s->points.size());
            for (const QPointF &pt : s->points)
                poly << QPointF(mapX(pt.x()), mapY(pt.y()));
            QPen pen(s->color, s->width, s->style, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(poly);
        } else {
            const double r = 0.5 * s->markerSize;
            p.setPen(Qt::NoPen);
            p.setBrush(s->color);
            for (const QPointF &pt : s->points)
                p.drawEllipse(QPointF(mapX(pt.x()), mapY(pt.y())), r, r);
        }
    }

    // reference-line labels: each line's RefAnchor positions the label along the
    // line (top/center/bottom for vertical, left/center/right for horizontal); the
    // window-wide style sets the font size, the perpendicular gap from the line,
    // and an optional framed opaque background for readability over plot lines.
    QFont refFont = baseFont;
    if (m_refLabelSize > 0.0) refFont.setPointSizeF(m_refLabelSize);
    const QFontMetricsF fmRef(refFont);
    p.setFont(refFont);
    for (const PlotSeries *s : m_series) {
        if (!s || !s->visible || !s->isReference || s->refLabel.isEmpty()) continue;
        if (s->points.size() < 2) continue;
        const QString lbl   = s->refLabel;
        const double tw     = fmRef.horizontalAdvance(lbl);
        const bool vertical = s->points.first().x() == s->points.last().x();
        double tx = 0.0, ty = 0.0; // text baseline origin
        if (vertical) {
            const double px = mapX(s->points.first().x());
            if (px < plot.left() - 0.5 || px > plot.right() + 0.5) continue;
            tx = px + m_refLabelDist;
            if (tx + tw > plot.right()) tx = px - m_refLabelDist - tw; // flip near right edge
            if (s->refAnchor == RefAnchor::Start)
                ty = plot.top() + fmRef.ascent() + 2.0;
            else if (s->refAnchor == RefAnchor::End)
                ty = plot.bottom() - fmRef.descent() - 2.0;
            else
                ty = plot.center().y() + 0.5 * (fmRef.ascent() - fmRef.descent());
        } else {
            const double py = mapY(s->points.first().y());
            if (py < plot.top() - 0.5 || py > plot.bottom() + 0.5) continue;
            if (s->refAnchor == RefAnchor::Start)
                tx = plot.left() + 2.0;
            else if (s->refAnchor == RefAnchor::End)
                tx = plot.right() - tw - 2.0;
            else
                tx = plot.center().x() - 0.5 * tw;
            ty = py - m_refLabelDist;
            if (ty - fmRef.ascent() < plot.top()) ty = py + fmRef.ascent() + m_refLabelDist; // flip
        }
        if (m_refLabelBoxed) {
            const QRectF box(tx - 2.0, ty - fmRef.ascent() - 1.0, tw + 4.0, fmRef.height() + 2.0);
            p.setPen(QPen(s->color, 0));
            p.setBrush(QColor(255, 255, 255)); // opaque white for readability
            p.drawRect(box);
        }
        p.setPen(s->color);
        p.drawText(QPointF(tx, ty), lbl);
    }

    // optional legend in a chosen plot corner: one row per visible, named data
    // series (raw / processed / fit / overlays). Reference lines and the unnamed
    // marker-only mirror series are excluded; duplicate names (e.g. a line and its
    // scatter twin in "Both" mode) collapse to a single row.
    if (m_legendPos != LegendPos::Off) {
        struct LegendEntry {
            QString name;
            QColor color;
            bool line;
        };
        QList<LegendEntry> entries;
        QList<QString> seen;
        for (const PlotSeries *s : m_series) {
            if (!s || !s->visible || s->isReference || s->name.isEmpty()) continue;
            if (seen.contains(s->name)) continue;
            seen.append(s->name);
            entries.append({s->name, s->color, s->type == PlotSeriesType::Line});
        }
        if (!entries.isEmpty()) {
            p.setFont(baseFont);
            double maxTextW = 0.0;
            for (const auto &e : entries)
                maxTextW = std::max(maxTextW, fm.horizontalAdvance(e.name));
            const double boxW = LEGEND_PAD + LEGEND_SWATCH + LEGEND_GAP + maxTextW + LEGEND_PAD;
            const double boxH = LEGEND_PAD + entries.size() * labelH + LEGEND_PAD;
            const bool right =
                (m_legendPos == LegendPos::TopRight || m_legendPos == LegendPos::BottomRight);
            const bool bottom =
                (m_legendPos == LegendPos::BottomLeft || m_legendPos == LegendPos::BottomRight);
            const double bx = right ? (plot.right() - LEGEND_INSET - boxW)
                                    : (plot.left() + LEGEND_INSET);
            const double by = bottom ? (plot.bottom() - LEGEND_INSET - boxH)
                                     : (plot.top() + LEGEND_INSET);
            QPen border(FRAME_COLOR);
            border.setWidth(0);
            p.setPen(border);
            p.setBrush(QColor(255, 255, 255, 230)); // slightly translucent over the grid
            p.drawRect(QRectF(bx, by, boxW, boxH));
            double y = by + LEGEND_PAD;
            for (const auto &e : entries) {
                const double cy = y + 0.5 * labelH;
                if (e.line) {
                    p.setPen(QPen(e.color, 2.0));
                    p.drawLine(QPointF(bx + LEGEND_PAD, cy),
                               QPointF(bx + LEGEND_PAD + LEGEND_SWATCH, cy));
                } else {
                    p.setPen(Qt::NoPen);
                    p.setBrush(e.color);
                    p.drawEllipse(QPointF(bx + LEGEND_PAD + 0.5 * LEGEND_SWATCH, cy), 3.5, 3.5);
                }
                p.setPen(Qt::black);
                p.drawText(QPointF(bx + LEGEND_PAD + LEGEND_SWATCH + LEGEND_GAP, y + fm.ascent()),
                           e.name);
                y += labelH;
            }
        }
    }
    p.restore();
}

// Local Variables:
// c-basic-offset: 4
// End:
