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

#include "chartviewer.h"
#include "chartstyle.h"

#include "analysis.h"
#include "constants.h"
#include "customfunc.h"
#include "fitting.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "leastsquares.h"
#include "plotdata.h"
#include "plotdatadialog.h"
#include "qaddon.h"
#include "rangeslider.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QKeySequence>
#include <QScrollArea>

#include <QLabel>
#include <QLayout>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaMethod>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>
#include <utility>

#include "plotwidget.h"

#include <cmath>

namespace {

// Set RangeSlider resolution to 1000 steps
constexpr int SLIDER_RANGE       = 1000;
constexpr double SLIDER_FRACTION = 1.0 / static_cast<double>(SLIDER_RANGE);
constexpr int LAYOUT_SPACING     = 6;

// Translate the smoothing-choice index (Raw/Smooth/Both, kept in sync with
// the preferences dialog) into the raw/smooth display flags.
void smoothFlagsFromChoice(int choice, bool &doRaw, bool &doSmooth)
{
    switch (choice) {
        case 0:
            doRaw    = true;
            doSmooth = false;
            break;
        case 1:
            doRaw    = false;
            doSmooth = true;
            break;
        case 2: // fallthrough
        default:
            doRaw    = true;
            doSmooth = true;
            break;
    }
}

// Widen a (near) empty [lo, hi] range to a small symmetric/relative band so the
// axis is never degenerate. Shared by the X and Y branches of getMinMax().
void padEmptyRange(double &lo, double &hi)
{
    // compare against the magnitude: dividing by a signed hi made the test
    // true for every all-negative range, padding ranges that were not empty
    const double delta = hi - lo;
    if ((delta / ((hi == 0.0) ? 1.0 : fabs(hi))) < 1.0e-10) {
        if ((lo == 0.0) || (hi == 0.0)) {
            lo = -0.025;
            hi = 0.025;
        } else {
            lo -= 0.025 * fabs(lo);
            hi += 0.025 * fabs(hi);
        }
    }
}

// Parse a "name=value, name=value, ..." string of nonlinear-fit parameters and
// their initial guesses into an ordered list. Sets *ok to false on any empty or
// malformed token (so the caller can report a usage hint).
QList<FitParam> parseFitParams(const QString &text, bool *ok)
{
    QList<FitParam> params;
    *ok                      = true;
    const QStringList tokens = text.split(',', Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const QStringList kv = token.split('=');
        if (kv.size() != 2) {
            *ok = false;
            return {};
        }
        const QString name = kv[0].trimmed();
        bool valueOk       = false;
        const double value = kv[1].trimmed().toDouble(&valueOk);
        if (name.isEmpty() || !valueOk) {
            *ok = false;
            return {};
        }
        params.append(FitParam{name, value});
    }
    if (params.isEmpty()) *ok = false;
    return params;
}

} // namespace

// Forward declarations of the data-only column helpers (defined in the column
// rendering-pipeline namespace further down) that ChartWindow needs before that
// block appears; they are pure (no PlotWidget), so no other helper is required.
namespace {
bool appendColumnPoint(ChartColumn &col, double x, double y);
void setColumnData(ChartColumn &col, const QList<QPointF> &points, const QList<double> &yerr = {},
                   const QList<double> &yerrLo = {});
void setColumnSmoothFlags(ChartColumn &col, bool doRaw, bool doSmooth, int window, int order);
void applyColumnStyleDefaults(ChartColumn &col);
QList<QPointF> calc_sgsmooth(const QList<QPointF> &input, std::size_t window, int order);

// Value at x of a curve given by its sampled points, by linear interpolation.
// Used to write a fit curve -- which is sampled on a dense grid of its own over
// the data range -- against the x values of the data it was fitted to.  The
// points are in increasing x, as every curve the fits produce is, and x is
// inside their range, as every data x is.
double interpolateCurve(const QList<QPointF> &pts, double x)
{
    if (pts.isEmpty()) return 0.0;
    if (x <= pts.first().x()) return pts.first().y();
    if (x >= pts.last().x()) return pts.last().y();
    // the first point at or past x; the interval before it brackets x
    const auto hi = std::lower_bound(pts.cbegin(), pts.cend(), x, [](const QPointF &p, double v) {
        return p.x() < v;
    });
    const QPointF &b = *hi;
    const QPointF &a = *(hi - 1);
    const double dx  = b.x() - a.x();
    if (dx <= 0.0) return a.y(); // coincident samples: nothing to interpolate
    return a.y() + (b.y() - a.y()) * (x - a.x()) / dx;
}

// Pick the error bars of one data column out of a PlotErrors and convert them
// for a PlotSeries. Bars of the wrong length are dropped rather than padded, so
// a mismatch can only lose the annotation, never shift it onto other points.
void columnErrors(const PlotErrors &errors, int column, int nrow, QList<double> &yerr,
                  QList<double> &yerrLo)
{
    yerr.clear();
    yerrLo.clear();
    const auto col = static_cast<std::size_t>(column);
    if (col >= errors.upper.size()) return;
    const std::vector<double> &up = errors.upper[col];
    if (up.size() != static_cast<std::size_t>(nrow)) return;
    yerr = QList<double>(up.cbegin(), up.cend());
    if (col >= errors.lower.size()) return;
    const std::vector<double> &lo = errors.lower[col];
    if (lo.size() == static_cast<std::size_t>(nrow)) yerrLo = QList<double>(lo.cbegin(), lo.cend());
}
} // namespace

/* -------------------------------------------------------------------- */

ChartViewer *ChartWindow::currentChart()
{
    // a single view, kept bound by changeChart() to the combo's current column
    return cols.empty() ? nullptr : viewer;
}

// position in `cols` of the column whose index matches the combo selection (-1 if none)
int ChartWindow::activeIndex() const
{
    const int choice = columns->currentData().toInt();
    for (std::size_t i = 0; i < cols.size(); ++i)
        if (cols[i]->index == choice) return static_cast<int>(i);
    return cols.empty() ? -1 : 0;
}

void ChartWindow::setProcessedLabel(const QString &label)
{
    if (active >= 0) cols[active]->procLabel = label;
    smooth->setItemText(1, label);
}

void ChartWindow::presentResultWindow(ChartWindow *win, const QString &title)
{
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // a minimum size becomes a floor the dock area cannot get below
    if (!dockedLayout()) win->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
    if (isSignalConnected(QMetaMethod::fromSignal(&ChartWindow::resultWindowCreated)))
        emit resultWindowCreated(win, title);
    else
        win->show();
}

void ChartWindow::resetRangeSliders()
{
    // setLow/setHigh only repaint the handles; callers update the plot separately
    xrange->setLow(0);
    xrange->setHigh(SLIDER_RANGE);
    yrange->setLow(0);
    yrange->setHigh(SLIDER_RANGE);
}

void ChartWindow::applySliderWindow()
{
    if (cols.empty()) return;
    updateXRange(xrange->low(), xrange->high());
    updateYRange(yrange->low(), yrange->high());
}

ChartWindow::ChartWindow(const QString &_filename, LammpsGui *_lammpsgui, QWidget *parent) :
    // the menu bar does not take ownership of the added file menu, so it must
    // be created with the menu bar as its parent to be freed along with it
    QWidget(parent), lammpsgui(_lammpsgui), menu(new QMenuBar), file(new QMenu("&File", menu)),
    smooth(nullptr), window(nullptr), order(nullptr), chartTitle(nullptr), chartYlabel(nullptr),
    chartXlabel(nullptr), units(nullptr), norm(nullptr), filename(_filename), viewer(nullptr),
    active(-1)
{
    QSettings settings;
    auto *top  = new QVBoxLayout;
    auto *row1 = new QHBoxLayout;
    auto *row2 = new QHBoxLayout;
    top->addLayout(row1);
    top->addWidget(new QHline);
    top->addLayout(row2);
    top->addWidget(new QHline);
    row1->setSpacing(LAYOUT_SPACING);
    row2->setSpacing(LAYOUT_SPACING);
    top->setSpacing(LAYOUT_SPACING);

    file->setObjectName(Cfg::VIEW_FILE_MENU);
    if (dockedLayout()) {
        // docked, the main window carries one menu bar for all panels and puts
        // this menu at its front while the panel has the focus
        retireViewMenuBar(menu);
    } else {
        menu->addMenu(file);
        // the application-wide menus are the main window's own objects, so a run
        // can be started or stopped from here without a second set of actions to
        // keep in step (and without a second binding for their accelerators)
        if (lammpsgui)
            for (auto *shared : lammpsgui->sharedMenus())
                menu->addMenu(shared);
    }
    menu->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // workaround for incorrect highlight bug on macOS
    auto *dummy = new QPushButton(QIcon(), "");
    dummy->hide();

    // plot title and axis labels
    // the settings-derived contents of these widgets are filled in by the
    // applyChartSettings() call further down (shared with reset())
    chartTitle  = new QLineEdit;
    chartYlabel = new QLineEdit("");
    if (!lammpsgui) chartXlabel = new QLineEdit("");

    // plot smoothing; the processed-series slot always holds the smoothed data
    // ("Smooth"), and a post-process fit/function overrides that label with its name
    smooth = makePlotChoiceCombo(0);
    window = new QSpinBox;
    window->setRange(Cfg::SMOOTH_WINDOW_MIN, Cfg::SMOOTH_WINDOW_MAX);
    window->setToolTip("Smoothing Window Size");
    // no keyboard tracking: valueChanged then fires once per committed edit
    // instead of re-smoothing on every typed digit
    window->setKeyboardTracking(false);
    order = new QSpinBox;
    order->setRange(Cfg::SMOOTH_ORDER_MIN, Cfg::SMOOTH_ORDER_MAX);
    order->setToolTip("Smoothing Order");
    order->setKeyboardTracking(false);

    columns = new QComboBox;
    row1->addWidget(menu);
    // the second addWidget() would reparent the button out of row1 again, so
    // the hidden macOS-workaround button lives in row2 only
    row2->addWidget(dummy);
    row1->addWidget(new QLabel("Title:"));
    // in standalone plot mode give the title half the stretch to make room for X-Axis label
    row1->addWidget(chartTitle, lammpsgui ? 2 : 1);
    if (!lammpsgui) {
        row1->addWidget(new QLabel("X-Axis:"));
        row1->addWidget(chartXlabel, 1);
    }
    row1->addWidget(new QLabel("Y-Axis:"));
    row1->addWidget(chartYlabel, 1);
    auto *unitsLabel = new QLabel("Units:");
    row1->addWidget(unitsLabel);
    units = new QLabel("[lj]");
    units->setFrameStyle(QFrame::Panel | QFrame::Raised);
    row1->addWidget(units);
    auto *normLabel = new QLabel("Norm:");
    row1->addWidget(normLabel);
    norm = new QCheckBox("");
    norm->setChecked(false);
    norm->setEnabled(false);
    row1->addWidget(norm);
    // units and normalization are LAMMPS thermo settings we do not know when
    // plotting external data files (no live simulation), so hide them then
    if (!lammpsgui) {
        unitsLabel->hide();
        units->hide();
        normLabel->hide();
        norm->hide();
    }
    row1->addWidget(new QLabel(" Data:"));
    row1->addWidget(columns, 1);

    xrange = new RangeSlider;
    xrange->setMinimum(0);
    xrange->setMaximum(SLIDER_RANGE);
    xrange->setLow(0);
    xrange->setHigh(SLIDER_RANGE);
    xrange->setToolTip("Adjust x-axis data range");
    xrange->setTickPosition(QSlider::TicksBothSides);
    xrange->setTickInterval(100);
    yrange = new RangeSlider;
    yrange->setMinimum(0);
    yrange->setMaximum(SLIDER_RANGE);
    yrange->setLow(0);
    yrange->setHigh(SLIDER_RANGE);
    yrange->setToolTip("Adjust y-axis data range");
    yrange->setTickPosition(QSlider::TicksBothSides);
    yrange->setTickInterval(100);
    auto makeToolBtn = [](const QString &icon, const QString &tip) {
        auto *btn = new QPushButton(QIcon(icon), "");
        btn->setToolTip(tip);
        return btn;
    };
    auto *styleBtn = makeToolBtn(":/icons/preferences-desktop-personal.svg", "Chart Style...");
    auto *refBtn   = makeToolBtn(":/icons/reference-lines.svg", "Reference Lines...");
    auto *ppBtn    = makeToolBtn(":/icons/chart-smooth.svg", "Postprocess...");
    // square toolbar buttons with a snug, uniform icon (shared policy)
    styleToolButtons(toolButtonSize(styleBtn), {styleBtn, refBtn, ppBtn});
    connect(styleBtn, &QPushButton::clicked, this, &ChartWindow::changeStyle);
    connect(refBtn, &QPushButton::clicked, this, &ChartWindow::referenceLines);
    connect(ppBtn, &QPushButton::clicked, this, &ChartWindow::postProcess);
    row2->addWidget(styleBtn);
    row2->addWidget(refBtn);
    row2->addWidget(ppBtn);
    row2->addWidget(new QLabel("X:"));
    row2->addWidget(xrange);
    row2->addWidget(new QLabel("Y:"));
    row2->addWidget(yrange);
    row2->addWidget(new QLabel("Plot:"));
    row2->addWidget(smooth);
    row2->addWidget(new QLabel(" Smooth:"));
    row2->addWidget(window);
    row2->addWidget(order);
    addMenuAction(file, "&Save Graph As...", ":/icons/document-save-as.svg", this,
                  &ChartWindow::saveAs);
    auto *copyAct = addMenuAction(file, "Copy &Graph to Clipboard", ":/icons/edit-copy.svg", this,
                                  &ChartWindow::copy);
    scopeShortcut(this, copyAct, QKeySequence(QKeySequence::Copy));
    addMenuAction(file, "&Export data to CSV...", ":/icons/csv-file-icon.svg", this,
                  &ChartWindow::exportCsv);
    addMenuAction(file, "Export data to &Gnuplot...", ":/icons/txt-file-icon.svg", this,
                  &ChartWindow::exportDat);
    addMenuAction(file, "Export data to &YAML...", ":/icons/yaml-file-icon.svg", this,
                  &ChartWindow::exportYaml);
    file->addSeparator();
    addMenuAction(file, "Chart &Style...", ":/icons/preferences-desktop-personal.svg", this,
                  &ChartWindow::changeStyle);
    addMenuAction(file, "&Reference Lines...", ":/icons/reference-lines.svg", this,
                  &ChartWindow::referenceLines);
    addMenuAction(file, "&Postprocess...", ":/icons/chart-smooth.svg", this,
                  &ChartWindow::postProcess);
    // "Add Data from File..." is only relevant in standalone file-plot mode
    if (!lammpsgui) {
        addMenuAction(file, "&Add Data from File...", ":/icons/application-plot.svg", this,
                      &ChartWindow::addDataFile);
    }
    file->addSeparator();
    auto *stopAct =
        addMenuAction(file, "Stop &Run", ":/icons/process-stop.svg", this, &ChartWindow::stopRun);
    scopeShortcut(this, stopAct, QKeySequence(Qt::CTRL | Qt::Key_Slash));
    // without a live simulation there is nothing to stop
    if (!lammpsgui) stopAct->setVisible(false);
    auto *closeAct =
        addMenuAction(file, "&Close", ":/icons/window-close.svg", this, &QWidget::close);
    scopeShortcut(this, closeAct, QKeySequence(Qt::CTRL | Qt::Key_W));
    auto *quitAct =
        addMenuAction(file, "&Quit", ":/icons/application-exit.svg", this, &ChartWindow::quit);
    scopeShortcut(this, quitAct, QKeySequence(Qt::CTRL | Qt::Key_Q));
    if (!lammpsgui) quitAct->setVisible(false); // quit == close in standalone mode
    auto *layout = new QVBoxLayout;
    layout->addLayout(top);
    layout->setSpacing(LAYOUT_SPACING);
    // the single shared chart view; it renders whichever column is active
    viewer = new ChartViewer;
    // seed the settings-derived widget contents; must stay ahead of the
    // connect() calls below so it does not trigger the change slots
    applyChartSettings();
    layout->addWidget(viewer);
    setLayout(layout);

    connect(chartTitle, &QLineEdit::editingFinished, this, &ChartWindow::updateTLabel);
    connect(chartYlabel, &QLineEdit::editingFinished, this, &ChartWindow::updateYLabel);
    if (chartXlabel)
        connect(chartXlabel, &QLineEdit::editingFinished, this, &ChartWindow::updateXLabel);
    connect(smooth, &QComboBox::currentIndexChanged, this, &ChartWindow::selectSmooth);
    connect(window, QOverload<int>::of(&QSpinBox::valueChanged), this, &ChartWindow::updateSmooth);
    connect(order, QOverload<int>::of(&QSpinBox::valueChanged), this, &ChartWindow::updateSmooth);
    connect(columns, &QComboBox::currentIndexChanged, this, &ChartWindow::changeChart);
    connect(xrange, &RangeSlider::sliderMoved, this, &ChartWindow::updateXRange);
    connect(yrange, &RangeSlider::sliderMoved, this, &ChartWindow::updateYRange);

    applyWindowFlags(this);
    // in a docked layout the dock area decides the size, and the remembered
    // one belongs to a free-floating window, so it is neither read nor written
    if (!dockedLayout())
        resize(settings.value(Keys::CHARTX, Cfg::CHART_DEFAULT_WIDTH).toInt(),
               settings.value(Keys::CHARTY, Cfg::CHART_DEFAULT_HEIGHT).toInt());
}

int ChartWindow::getStep() const
{
    if (!cols.empty()) {
        const auto &series = cols[0]->series;
        if (series && series->count() > 0)
            return static_cast<int>(series->at(series->count() - 1).x());
    }
    return -1;
}

void ChartWindow::applyChartSettings()
{
    QSettings settings;
    settings.beginGroup(Keys::GROUP_CHARTS);

    if (lammpsgui) {
        // live simulation: use the configured title template
        chartTitle->setText(settings.value(Keys::TITLE, Cfg::CHART_TITLE_DEFAULT)
                                .toString()
                                .replace("%f", filename));
    } else {
        // standalone/plot mode: just the base filename, no "Thermo:" prefix
        chartTitle->setText(QFileInfo(filename).fileName());
    }

    // plot smoothing; block the change slots, the derived state is applied here
    const int smoothchoice = settings.value(Keys::SMOOTHCHOICE, 0).toInt();
    smoothFlagsFromChoice(smoothchoice, doRaw, doSmooth);
    {
        const QSignalBlocker blockSmooth(smooth);
        const QSignalBlocker blockWindow(window);
        const QSignalBlocker blockOrder(order);
        smooth->setCurrentIndex(smoothchoice);
        window->setValue(settings.value(Keys::SMOOTHWINDOW, Cfg::SMOOTH_WINDOW_DEFAULT).toInt());
        order->setValue(settings.value(Keys::SMOOTHORDER, Cfg::SMOOTH_ORDER_DEFAULT).toInt());
    }
    window->setEnabled(doSmooth);
    order->setEnabled(doSmooth);

    legendPos       = static_cast<LegendPos>(settings.value(Keys::LEGEND, 0).toInt());
    double defRefPt = font().pointSizeF();
    if (defRefPt <= 0.0) defRefPt = 9.0; // pixel-size app fonts report <= 0 pt
    refLabelSize  = settings.value(Keys::REFLABELSIZE, defRefPt).toDouble();
    refLabelDist  = settings.value(Keys::REFLABELDIST, 4.0).toDouble();
    refLabelBoxed = settings.value(Keys::REFLABELBOX, false).toBool();
    settings.endGroup();

    viewer->setLegendPos(legendPos);
    viewer->setRefLabelStyle(refLabelSize, refLabelDist, refLabelBoxed);
}

void ChartWindow::reset(const QString &_filename)
{
    filename = _filename;
    resetCharts();
    refLines.clear();
    // chart preferences are read when a window is created, so a reused window
    // has to pick up any edits made since the previous run here
    applyChartSettings();
    chartYlabel->clear();
}

void ChartWindow::resetCharts()
{
    viewer->setColumn(nullptr); // unregister the active column's series from the plot
    cols.clear();
    columns->clear();
    active = -1;
}

void ChartWindow::resetZoom()
{
    if (!cols.empty()) viewer->resetZoom();
}

void ChartWindow::addChart(const QString &title, int index)
{
    auto c          = std::make_unique<ChartColumn>();
    c->index        = index;
    c->series       = std::make_unique<PlotSeries>();
    c->series->name = title;
    c->yTitle       = title;
    c->lastUpdate.start();
    applyColumnStyleDefaults(*c); // the configured defaults, until the style dialog overrides them
    cols.push_back(std::move(c));
    columns->addItem(title, index);
    columns->show();
    if (cols.size() == 1) {
        // first column: make it active, seed the Y-label field, and bind the view
        active = 0;
        chartYlabel->setText(title);
        viewer->setColumn(cols[0].get());
    }
    updateTLabel();
    selectSmooth(0);
}

void ChartWindow::addData(int step, double data, int index)
{
    for (std::size_t i = 0; i < cols.size(); ++i) {
        if (cols[i]->index != index) continue;
        if (static_cast<int>(i) == active)
            viewer->addPoint(step, data); // appends + throttled redraw of the active column
        else
            appendColumnPoint(*cols[i], step, data); // accumulate only; drawn when selected
        return;
    }
}

void ChartWindow::setUnits(const QString &_units)
{
    units->setText(_units);
}

void ChartWindow::setNorm(bool _norm)
{
    norm->setChecked(_norm);
}

void ChartWindow::setRangeEnabled(bool enabled)
{
    xrange->setEnabled(enabled);
    yrange->setEnabled(enabled);
    smooth->setEnabled(enabled);
    window->setEnabled(enabled && doSmooth);
    order->setEnabled(enabled && doSmooth);
}

void ChartWindow::loadData(const PlotData &data, int xcol, const QList<int> &ycols,
                           const PlotErrors &yerrs)
{
    resetCharts();
    if (data.isEmpty() || ycols.isEmpty()) return;
    if ((xcol < 0) || (xcol >= data.columnCount())) return;

    const std::vector<double> &xvals = data.column(xcol);
    const int nrow                   = data.rowCount();
    const QString xlabel             = data.columnName(xcol);

    int idx = 0;
    for (int ycol : ycols) {
        if ((ycol < 0) || (ycol >= data.columnCount())) continue;
        addChart(data.columnName(ycol), idx); // the first one binds the view
        const std::vector<double> &yvals = data.column(ycol);
        QList<QPointF> points;
        points.reserve(nrow);
        for (int r = 0; r < nrow; ++r)
            points.append(QPointF(xvals[r], yvals[r]));
        QList<double> errs, errsLo;
        columnErrors(yerrs, ycol, nrow, errs, errsLo);
        // data only; the active one is drawn below
        setColumnData(*cols.back(), points, errs, errsLo);
        ++idx;
    }
    // shared X-axis labeling on the single plot (standalone uses %.6g)
    viewer->setXLabel(xlabel);
    viewer->setXLabelFormat("%.6g");
    // now that data is loaded, (re)render the active column
    if (!cols.empty()) viewer->setColumn(cols[active >= 0 ? active : 0].get());
    // pre-fill the X-axis label field in standalone plot mode
    if (chartXlabel) chartXlabel->setText(xlabel);
    setRangeEnabled(true);
    resetZoom();
}

void ChartWindow::copy()
{
#if QT_CONFIG(clipboard)
    auto *clip = QGuiApplication::clipboard();
    if (clip && !cols.empty()) {
        // the single view renders the active column
        auto image = viewer->grab().toImage();
        if (!image.isNull()) {
            clip->setImage(image, QClipboard::Clipboard);
            if (clip->supportsSelection()) clip->setImage(image, QClipboard::Selection);
            return;
        }
    }
    fprintf(stderr, "Copy graph to clipboard currently not available\n");
#else
    fprintf(stderr, "Copy graph to clipboard not supported on this platform\n");
#endif
}

void ChartWindow::quit()
{
    // in the live chart window Quit exits the whole application; a standalone
    // file-plot window (no LammpsGui) has nothing to quit, so just close it
    if (lammpsgui)
        lammpsgui->quit();
    else
        close();
}

void ChartWindow::stopRun()
{
    if (lammpsgui) lammpsgui->stopRun();
}

void ChartWindow::changeStyle()
{
    // the single view is bound to the currently selected column
    ChartViewer *chart = currentChart();
    if (!chart) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Chart Style");
    auto *layout = new QVBoxLayout(&dialog);

    // build a colored push button that edits the referenced color in place
    auto colorButton = [&dialog](QColor &chosen) {
        auto *btn        = new QPushButton;
        auto setBtnColor = [btn](const QColor &c) {
            btn->setText(c.name());
            btn->setStyleSheet(QString("background-color: %1; color: %2;")
                                   .arg(c.name(), (c.lightness() < 128) ? "white" : "black"));
        };
        setBtnColor(chosen);
        QObject::connect(btn, &QPushButton::clicked, &dialog, [&chosen, btn, setBtnColor]() {
            const QColor c = QColorDialog::getColor(chosen, btn, "Series Color");
            if (c.isValid()) {
                chosen = c;
                setBtnColor(c);
            }
        });
        return btn;
    };

    // the mode, width, and size widgets are the ones the Preferences dialog
    // builds too (chartstyle.h); a display mode is preset by its enum index
    auto modeBox = [](ChartDisplayMode mode) {
        return makeChartModeCombo(static_cast<int>(mode));
    };

    // raw data section
    QColor rawChosen = chart->displayColor();
    if (!rawChosen.isValid())
        rawChosen = configuredChartColor(Keys::RAWBRUSH, Cfg::RAWBRUSH_DEFAULT);
    auto *rawMode      = modeBox(chart->displayMode());
    auto *rawColorBtn  = colorButton(rawChosen);
    auto *rawWidthSpin = makeLineWidthSpin(chart->displayWidth());
    auto *rawPointSpin = makePointSizeSpin(chart->displayPointSize());
    auto *rawBox       = new QGroupBox("Raw data");
    auto *rawForm      = new QFormLayout(rawBox);
    rawForm->addRow("Display:", rawMode);
    rawForm->addRow("Color:", rawColorBtn);
    rawForm->addRow("Line width:", rawWidthSpin);
    rawForm->addRow("Point size:", rawPointSpin);
    layout->addWidget(rawBox);

    // processed data section
    QColor procChosen = chart->smoothColor();
    if (!procChosen.isValid())
        procChosen = configuredChartColor(Keys::SMOOTHBRUSH, Cfg::SMOOTHBRUSH_DEFAULT);
    auto *procMode      = modeBox(chart->smoothMode());
    auto *procColorBtn  = colorButton(procChosen);
    auto *procWidthSpin = makeLineWidthSpin(chart->smoothWidth());
    auto *procPointSpin = makePointSizeSpin(chart->smoothPointSize());
    auto *procBox       = new QGroupBox("Processed data");
    auto *procForm      = new QFormLayout(procBox);
    procForm->addRow("Display:", procMode);
    procForm->addRow("Color:", procColorBtn);
    procForm->addRow("Line width:", procWidthSpin);
    procForm->addRow("Point size:", procPointSpin);
    layout->addWidget(procBox);

    // error bar section; the bars of every series of this chart share one style
    QColor errChosen = chart->errorColor();
    if (!errChosen.isValid())
        errChosen = configuredChartColor(Keys::ERRBRUSH, Cfg::ERRBRUSH_DEFAULT);
    auto *errColorBtn  = colorButton(errChosen);
    auto *errWidthSpin = makeLineWidthSpin(chart->errorWidth());
    auto *errBox       = new QGroupBox("Error bars");
    errBox->setToolTip("Applies to the error bars of every series of this chart.\n"
                       "Only imported data can carry error bars.");
    auto *errForm = new QFormLayout(errBox);
    errForm->addRow("Color:", errColorBtn);
    errForm->addRow("Line width:", errWidthSpin);
    layout->addWidget(errBox);

    // in-plot legend section
    auto *legendCombo = new QComboBox;
    legendCombo->addItem("Off", static_cast<int>(LegendPos::Off));
    legendCombo->addItem("Top left", static_cast<int>(LegendPos::TopLeft));
    legendCombo->addItem("Top right", static_cast<int>(LegendPos::TopRight));
    legendCombo->addItem("Bottom right", static_cast<int>(LegendPos::BottomRight));
    legendCombo->addItem("Bottom left", static_cast<int>(LegendPos::BottomLeft));
    const int legendIdx = legendCombo->findData(static_cast<int>(legendPos));
    legendCombo->setCurrentIndex(legendIdx < 0 ? 0 : legendIdx);
    auto *legendBox  = new QGroupBox("Legend");
    auto *legendForm = new QFormLayout(legendBox);
    legendForm->addRow("Placement:", legendCombo);
    layout->addWidget(legendBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    styleDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        chart->setDisplayStyle(static_cast<ChartDisplayMode>(rawMode->currentData().toInt()),
                               rawChosen, rawWidthSpin->value(), rawPointSpin->value());
        chart->setSmoothStyle(static_cast<ChartDisplayMode>(procMode->currentData().toInt()),
                              procChosen, procWidthSpin->value(), procPointSpin->value());
        chart->setErrorStyle(errChosen, errWidthSpin->value());
        legendPos = static_cast<LegendPos>(legendCombo->currentData().toInt());
        // viewer is created unconditionally in the constructor and never reset to null
        viewer->setLegendPos(legendPos);
        QSettings settings;
        settings.beginGroup(Keys::GROUP_CHARTS);
        settings.setValue(Keys::LEGEND, static_cast<int>(legendPos));
        settings.endGroup();
        // a style change is view-only: restore the slider window the setters reset
        applySliderWindow();
    }
}

// for the Nyquist default of the Fourier output grid (M_PI needs feature-test
// macros on some of the platforms the packaging cross-compiles for)
static constexpr double pi_const = 3.14159265358979323846;

// Identifiers of the post-processing analyses, stored as the item data of the
// analysis combo.  The Overlay entry exists only when the window has more than
// one column, so combo positions are not stable identifiers.
enum PostAnalysis {
    AnaAcf = 0,
    AnaPoly,
    AnaEos,
    AnaFunc,
    AnaFit,
    AnaMaxBolt,
    AnaFourier,
    AnaSq,
    AnaOverlay,
    AnaSmooth,
};

void ChartWindow::postProcess()
{
    // the single view is bound to the currently selected column
    ChartViewer *chart = currentChart();
    if (!chart) return;

    const int npoints = chart->getCount();
    if (npoints < 2) {
        warning(this, "Postprocess", "Not enough data points to analyze.");
        return;
    }

    // pre-compute x range for fit-range spinbox initialization
    double dataXmin = chart->getStep(0), dataXmax = chart->getStep(0);
    for (int i = 1; i < npoints; ++i) {
        const double x = chart->getStep(i);
        if (x < dataXmin) dataXmin = x;
        if (x > dataXmax) dataXmax = x;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Postprocess Chart Data");
    auto *form = new QFormLayout(&dialog);

    auto *analysisbox = new QComboBox;
    analysisbox->addItem("Custom function", AnaFunc);
    // overlaying another column needs another column to exist
    if (cols.size() > 1) analysisbox->addItem("Overlay other data column", AnaOverlay);
    analysisbox->addItem("Birch-Murnaghan EOS fit", AnaEos);
    analysisbox->addItem("Maxwell-Boltzmann fit", AnaMaxBolt);
    analysisbox->addItem("Polynomial fit", AnaPoly);
    analysisbox->addItem("Custom fit", AnaFit);
    analysisbox->insertSeparator(99);
    analysisbox->addItem("Autocorrelation", AnaAcf);
    analysisbox->addItem("Fourier transform", AnaFourier);
    analysisbox->addItem("Structure factor", AnaSq);
    // the way back: a fit or overlay takes the processed-series slot, and this
    // entry vacates it again, so the offer is only made while one is in place
    if (chart->hasCustom()) {
        analysisbox->insertSeparator(99);
        analysisbox->addItem("Smoothed data (restore)", AnaSmooth);
    }
    form->addRow("Analysis:", analysisbox);

    // note shown only for the restore entry, whose whole effect it states
    auto *restoreNote = new QLabel("Removes the fitted or overlaid curve and returns\n"
                                   "the plot to smoothing the raw data.");
    form->addRow(restoreNote);

    // source selector for the column-overlay entry (data of the choices is the
    // position in cols, which lines up with the position in the columns combo)
    auto *overlayLabel = new QLabel("Column:");
    auto *overlayCombo = new QComboBox;
    overlayCombo->setToolTip("The column whose data is copied onto the current chart.\n"
                             "The copy takes the overlay slot a fitted curve would use,\n"
                             "so the next fit or overlay replaces it.");
    const int overlaySelf = activeIndex();
    for (int i = 0; i < static_cast<int>(cols.size()); ++i)
        if (i != overlaySelf) overlayCombo->addItem(columns->itemText(i), i);
    form->addRow(overlayLabel, overlayCombo);

    auto *paramLabel = new QLabel;
    auto *paramSpin  = new QSpinBox;
    form->addRow(paramLabel, paramSpin);

    // expression field, shown for both the custom-function plot and fit
    auto *exprLabel = new QLabel("f(x) =");
    auto *exprEdit  = new QLineEdit;
    exprEdit->setPlaceholderText("e.g. 2*x^2 + 3*sin(x)");
    exprEdit->setMinimumWidth(Cfg::POSTPROCESS_EXPR_WIDTH);
    form->addRow(exprLabel, exprEdit);

    // parameter (initial-guess) and label fields, shown only for the custom fit
    auto *paramsLabel = new QLabel("Parameters:");
    auto *paramsEdit  = new QLineEdit;
    paramsEdit->setPlaceholderText("name=guess, e.g. a=1, b=0.5");
    paramsEdit->setMinimumWidth(Cfg::POSTPROCESS_EXPR_WIDTH);
    form->addRow(paramsLabel, paramsEdit);

    // which part of the data a fit follows when the model cannot describe all
    // of it, shown for the distribution fit where the choice actually decides
    // whether the peak or the tail is matched
    auto *weightLabel = new QLabel("Weighting:");
    auto *weightCombo = new QComboBox;
    weightCombo->addItem("by bin population");
    weightCombo->addItem("by error bars (1/sigma^2)");
    weightCombo->addItem("none (uniform)");
    weightCombo->setToolTip("Which residuals the fit cares about most.\n"
                            "By bin population: every bin counts in proportion to the samples it\n"
                            "holds, so the fit follows the bulk of the distribution and its peak.\n"
                            "By error bars: the textbook weighting, which favors the points with\n"
                            "the smallest uncertainty -- usually the sparse tail of a histogram.");
    form->addRow(weightLabel, weightCombo);

    // Fourier transform kind; the k values are angular (rad per x unit)
    auto *ftKindLabel = new QLabel("Transform:");
    auto *ftKindCombo = new QComboBox;
    ftKindCombo->addItem("Cosine (spectral density)", static_cast<int>(FourierKind::Cosine));
    ftKindCombo->addItem("Sine", static_cast<int>(FourierKind::Sine));
    ftKindCombo->addItem("Power spectrum", static_cast<int>(FourierKind::Power));
    ftKindCombo->setToolTip("Cosine: 2 Int y(x) cos(kx) dx -- the spectral density when the data\n"
                            "is a correlation function (Wiener-Khinchin).\n"
                            "Sine: 2 Int y(x) sin(kx) dx.\n"
                            "Power: |Int y(x) exp(-ikx) dx|^2, insensitive to where the data\n"
                            "starts on the x axis.\n"
                            "k is the angular frequency, in rad per x unit.");
    form->addRow(ftKindLabel, ftKindCombo);

    // taper shared by the Fourier transform and the structure factor
    auto *taperLabel = new QLabel("Window:");
    auto *taperCombo = new QComboBox;
    taperCombo->addItem("none", static_cast<int>(FourierWindow::None));
    taperCombo->addItem("Hann taper", static_cast<int>(FourierWindow::Hann));
    taperCombo->setToolTip("A Hann taper fades the data to zero toward the end of the range,\n"
                           "suppressing the ringing caused by truncating data (a correlation\n"
                           "function, or g(r)-1 at the compute's cutoff) before it has decayed\n"
                           "to zero, at the price of some broadening.");
    form->addRow(taperLabel, taperCombo);

    // output grid of the transforms; the default reaches the Nyquist limit of
    // the data's mean spacing
    const double meanDx = (dataXmax - dataXmin) / static_cast<double>(npoints - 1);
    auto *gridLabel     = new QLabel("Output grid:");
    auto *gridWidget    = new QWidget;
    auto *gridRow       = new QHBoxLayout(gridWidget);
    gridRow->setContentsMargins(0, 0, 0, 0);
    auto *gridFromSpin = new QDoubleSpinBox;
    gridFromSpin->setDecimals(6);
    gridFromSpin->setRange(0.0, 1e15);
    gridFromSpin->setValue(0.0);
    auto *gridToSpin = new QDoubleSpinBox;
    gridToSpin->setDecimals(6);
    gridToSpin->setRange(0.0, 1e15);
    gridToSpin->setValue((meanDx > 0.0) ? (pi_const / meanDx) : 1.0);
    auto *gridPointsSpin = new QSpinBox;
    gridPointsSpin->setRange(2, 100000);
    gridPointsSpin->setValue(Cfg::POSTPROCESS_GRID_POINTS);
    gridRow->addWidget(new QLabel("from"));
    gridRow->addWidget(gridFromSpin, 1);
    gridRow->addWidget(new QLabel("to"));
    gridRow->addWidget(gridToSpin, 1);
    gridRow->addWidget(new QLabel("points"));
    gridRow->addWidget(gridPointsSpin);
    form->addRow(gridLabel, gridWidget);

    // number density scaling S(q) - 1
    auto *rhoLabel = new QLabel("Density:");
    auto *rhoSpin  = new QDoubleSpinBox;
    rhoSpin->setDecimals(6);
    rhoSpin->setRange(1e-15, 1e15);
    rhoSpin->setValue(1.0);
    rhoSpin->setToolTip("Number density N/V of the system, in the units of the r axis\n"
                        "cubed.  It scales S(q) - 1, so getting it wrong stretches the\n"
                        "structure away from 1 but moves no peak.");
    form->addRow(rhoLabel, rhoSpin);

    auto *fitLabelLabel = new QLabel("Label:");
    auto *fitLabelEdit  = new QLineEdit;
    fitLabelEdit->setPlaceholderText("optional name for the fitted curve");
    fitLabelEdit->setMinimumWidth(Cfg::POSTPROCESS_EXPR_WIDTH);
    form->addRow(fitLabelLabel, fitLabelEdit);

    // fit x-range (hidden for autocorrelation, shown for all fitting analyses)
    auto *fitRangeLabel  = new QLabel("Fit x-range:");
    auto *fitRangeWidget = new QWidget;
    auto *fitRangeRow    = new QHBoxLayout(fitRangeWidget);
    fitRangeRow->setContentsMargins(0, 0, 0, 0);
    auto *fitFromSpin = new QDoubleSpinBox;
    fitFromSpin->setDecimals(6);
    fitFromSpin->setRange(-1e15, 1e15);
    fitFromSpin->setValue(dataXmin);
    auto *fitToSpin = new QDoubleSpinBox;
    fitToSpin->setDecimals(6);
    fitToSpin->setRange(-1e15, 1e15);
    fitToSpin->setValue(dataXmax);
    fitRangeRow->addWidget(new QLabel("from"));
    fitRangeRow->addWidget(fitFromSpin, 1);
    fitRangeRow->addWidget(new QLabel("to"));
    fitRangeRow->addWidget(fitToSpin, 1);
    form->addRow(fitRangeLabel, fitRangeWidget);

    // swap the parameter widgets to match the selected analysis
    auto configure = [=, &dialog](int idx) {
        const int id         = analysisbox->itemData(idx).toInt();
        const bool plot      = (id == AnaFunc); // custom-function plotting
        const bool fit       = (id == AnaFit);  // custom-function nonlinear fit
        const bool expr      = plot || fit;
        const bool eos       = (id == AnaEos);
        const bool maxbolt   = (id == AnaMaxBolt); // Maxwell-Boltzmann distribution fit
        const bool fourier   = (id == AnaFourier); // generic Fourier transform
        const bool sq        = (id == AnaSq);      // structure factor from g(r)
        const bool transform = fourier || sq;
        const bool overlay   = (id == AnaOverlay); // copy of another column as overlay
        const bool restore   = (id == AnaSmooth);  // vacate the processed-series slot
        // analyses on an x-range of the data
        const bool showRange = (id != AnaAcf) && !overlay && !restore;
        restoreNote->setVisible(restore);
        exprLabel->setVisible(expr);
        exprEdit->setVisible(expr);
        paramsLabel->setVisible(fit);
        paramsEdit->setVisible(fit);
        fitLabelLabel->setVisible(fit);
        fitLabelEdit->setVisible(fit);
        weightLabel->setVisible(maxbolt);
        weightCombo->setVisible(maxbolt);
        ftKindLabel->setVisible(fourier);
        ftKindCombo->setVisible(fourier);
        taperLabel->setVisible(transform);
        taperCombo->setVisible(transform);
        gridLabel->setVisible(transform);
        gridWidget->setVisible(transform);
        rhoLabel->setVisible(sq);
        rhoSpin->setVisible(sq);
        overlayLabel->setVisible(overlay);
        overlayCombo->setVisible(overlay);
        if (plot)
            fitRangeLabel->setText("Plot x-range:");
        else if (transform)
            fitRangeLabel->setText("Data x-range:");
        else
            fitRangeLabel->setText("Fit x-range:");
        fitRangeLabel->setVisible(showRange);
        fitRangeWidget->setVisible(showRange);
        paramLabel->setVisible(!expr && !eos && !overlay && !transform && !restore);
        if (id == AnaPoly) { // polynomial degree
            paramLabel->setText("Degree:");
            paramSpin->setVisible(true);
            paramSpin->setRange(1, qMin(npoints - 1, 8));
            paramSpin->setValue(qMin(3, qMin(npoints - 1, 8)));
        } else if (maxbolt) { // spatial dimensions the energies were drawn in
            paramLabel->setText("Dimensions:");
            paramSpin->setVisible(true);
            paramSpin->setRange(1, 3);
            paramSpin->setValue(3);
            paramSpin->setToolTip("Degrees of freedom per atom: the exponent of the prefactor is\n"
                                  "d/2 - 1, so the familiar sqrt(E) shape is the free three-\n"
                                  "dimensional case.  Constrained or rigid molecules have fewer:\n"
                                  "a rigid 3-site water has 6 per molecule, so 2 per atom.");
        } else if (eos) { // EOS: only show the x-axis confirmation
            paramSpin->setVisible(false);
        } else if (expr) { // custom function/fit: expression field(s) only
            paramSpin->setVisible(false);
        } else if (transform) { // Fourier transforms: their own rows only
            paramSpin->setVisible(false);
        } else if (overlay) { // column overlay: source selector only
            paramSpin->setVisible(false);
        } else if (restore) { // restore smoothing: the note says it all
            paramSpin->setVisible(false);
        } else { // autocorrelation max lag
            paramLabel->setText("Max lag:");
            paramSpin->setVisible(true);
            paramSpin->setRange(1, npoints - 1);
            paramSpin->setValue(qMin(npoints - 1, npoints / 2));
        }
        dialog.adjustSize();
    };
    configure(0);
    connect(analysisbox, &QComboBox::currentIndexChanged, &dialog, configure);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    styleDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted) return;

    const int which = analysisbox->currentData().toInt();

    if (which == AnaSmooth) { // back to the Savitzky-Golay smooth of the raw data
        chart->clearFitCurve();
        setProcessedLabel(QStringLiteral("Smooth"));
        resetRangeSliders(); // the fit or overlay may have stretched the data range
        selectSmooth(0);     // re-enables the smoothing parameter boxes and redraws
        return;
    }

    // gather the (x, y) data of the selected chart, with its error bars where
    // it has them (a weighted fit can use those as standard deviations)
    std::vector<double> xs, ys, es;
    xs.reserve(npoints);
    ys.reserve(npoints);
    es.reserve(npoints);
    for (int i = 0; i < npoints; ++i) {
        xs.push_back(chart->getStep(i));
        ys.push_back(chart->getData(i));
        es.push_back(chart->getError(i));
    }

    if (which == AnaOverlay) { // overlay a snapshot of another column of this window
        const int src = overlayCombo->currentData().toInt();
        if ((src < 0) || (src >= static_cast<int>(cols.size()))) return;
        const auto &series = cols[src]->series;
        if (!series || (series->count() < 2)) {
            warning(this, "Postprocess", "The selected column has too few data points.");
            return;
        }
        // a copy, deliberately: the overlay is a snapshot for comparison and
        // does not follow the source column afterwards
        const QString title = columns->itemText(src);
        chart->setFitCurve(series->points, title);
        setProcessedLabel(title.length() > 12 ? QStringLiteral("Overlay") : title);
        resetRangeSliders();        // the overlay may extend the data range
        smooth->setCurrentIndex(2); // "Both" = raw data + overlay
        return;
    }

    // filter to the user-specified x-range for fitting analyses (not autocorrelation)
    if (which != AnaAcf) {
        const double fitXmin = fitFromSpin->value();
        const double fitXmax = fitToSpin->value();
        // The spin boxes start out holding the data range rounded to their own
        // decimals, and that rounding can land a hair *inside* the data, which
        // would drop the first or the last point from a range the user never
        // touched -- silently, and enough to move a fitted parameter.  A bound
        // is therefore only honored to the precision the widget can express:
        // half of its last digit, plus the resolution of a double this large.
        const double fitEps = 0.5 * std::pow(10.0, -fitFromSpin->decimals()) +
                              1.0e-12 * qMax(qAbs(fitXmin), qAbs(fitXmax));
        if (fitXmin < fitXmax) {
            std::vector<double> fxs, fys, fes;
            for (std::size_t i = 0; i < xs.size(); ++i) {
                if ((xs[i] >= fitXmin - fitEps) && (xs[i] <= fitXmax + fitEps)) {
                    fxs.push_back(xs[i]);
                    fys.push_back(ys[i]);
                    fes.push_back(es[i]);
                }
            }
            if (fxs.size() >= 2) {
                xs = std::move(fxs);
                ys = std::move(fys);
                es = std::move(fes);
            } else {
                warning(this, "Postprocess",
                        "Fewer than 2 data points in the selected x-range; using full data.");
            }
        }
    }

    if (which == AnaAcf) { // autocorrelation -> new window (the abscissa becomes lag)
        const std::vector<double> acf = autocorrelation(ys, paramSpin->value());
        if (acf.empty()) {
            warning(this, "Postprocess",
                    "Could not compute the autocorrelation (constant or insufficient data).");
            return;
        }
        PlotData result;
        result.setColumnNames({"lag", "ACF: " + chart->getName()});
        for (std::size_t k = 0; k < acf.size(); ++k)
            result.appendRow({static_cast<double>(k), acf[k]});

        auto *win = new ChartWindow(filename + " (ACF)", nullptr);
        win->setWindowTitle("Autocorrelation - LAMMPS-GUI");
        win->loadData(result, 0, {1});
        presentResultWindow(win, "ACF: " + chart->getName());
        return;
    }

    if ((which == AnaFourier) || (which == AnaSq)) { // transform -> new window
        // the quadrature integrates left to right; chart data usually is
        // ascending in x, but an imported table need not be
        if (!std::is_sorted(xs.begin(), xs.end())) {
            std::vector<std::pair<double, double>> pts;
            pts.reserve(xs.size());
            for (std::size_t i = 0; i < xs.size(); ++i)
                pts.emplace_back(xs[i], ys[i]);
            std::sort(pts.begin(), pts.end());
            for (std::size_t i = 0; i < pts.size(); ++i) {
                xs[i] = pts[i].first;
                ys[i] = pts[i].second;
            }
        }

        double kmin = gridFromSpin->value();
        double kmax = gridToSpin->value();
        if (kmin > kmax) std::swap(kmin, kmax);
        if (kmax <= kmin) {
            warning(this, "Postprocess", "The output grid has zero extent.");
            return;
        }
        const int nk = gridPointsSpin->value();
        std::vector<double> kgrid;
        kgrid.reserve(nk);
        for (int i = 0; i < nk; ++i)
            kgrid.push_back(kmin + (kmax - kmin) * static_cast<double>(i) / (nk - 1.0));
        const auto taperKind = static_cast<FourierWindow>(taperCombo->currentData().toInt());

        std::vector<double> values;
        QString xname, yname, wtitle;
        if (which == AnaSq) {
            values = structureFactor(xs, ys, rhoSpin->value(), kgrid, taperKind);
            xname  = "q";
            yname  = "S(q): " + chart->getName();
            wtitle = "Structure Factor";
        } else {
            const auto kind = static_cast<FourierKind>(ftKindCombo->currentData().toInt());
            values          = fourierTransform(xs, ys, kgrid, kind, taperKind);
            xname           = "omega";
            switch (kind) {
                case FourierKind::Cosine:
                    yname = "FT cos: ";
                    break;
                case FourierKind::Sine:
                    yname = "FT sin: ";
                    break;
                case FourierKind::Power:
                    yname = "FT power: ";
                    break;
            }
            yname += chart->getName();
            wtitle = "Fourier Transform";
        }
        if (values.empty()) {
            warning(this, "Postprocess", "Could not compute the transform (insufficient data).");
            return;
        }

        PlotData result;
        result.setColumnNames({xname, yname});
        for (std::size_t i = 0; i < values.size(); ++i)
            result.appendRow({kgrid[i], values[i]});

        auto *win = new ChartWindow(filename + ((which == AnaSq) ? " (Sq)" : " (FT)"), nullptr);
        win->setWindowTitle(wtitle + " - LAMMPS-GUI");
        win->loadData(result, 0, {1});
        presentResultWindow(win, ((which == AnaSq) ? "S(q): " : "FT: ") + chart->getName());
        return;
    }

    // fits: build a smooth curve over the data x range and overlay it
    const auto mm        = std::minmax_element(xs.begin(), xs.end());
    const double xmin    = *mm.first;
    const double xmax    = *mm.second;
    constexpr int Ncurve = 200;

    if (which == AnaFunc) { // custom function f(x) evaluated over the data x range
        const QString expr       = exprEdit->text().trimmed();
        const CustomCurve result = evalCustomCurve(expr, xmin, xmax, Ncurve);
        if (!result.ok) {
            warning(this, "Custom Function",
                    QString("Could not evaluate the expression:\n%1").arg(result.error));
            return;
        }
        if (result.points.size() < 2) {
            warning(this, "Custom Function",
                    "The expression did not produce a usable curve over the data range.");
            return;
        }
        chart->setFitCurve(result.points, expr);
        setProcessedLabel("Custom f(x)");
        resetRangeSliders();        // a fit re-fits to the whole data set; match the sliders
        smooth->setCurrentIndex(2); // "Both" = raw data + function overlay
        information(this, "Custom Function",
                    QString("Plotted f(x) = %1\nover x in [%2, %3].")
                        .arg(expr)
                        .arg(xmin, 0, 'g', 6)
                        .arg(xmax, 0, 'g', 6));
        return;
    }

    if (which == AnaFit) { // custom nonlinear least-squares fit of f(x) to the data
        const QString expr            = exprEdit->text().trimmed();
        bool paramsOk                 = false;
        const QList<FitParam> initial = parseFitParams(paramsEdit->text(), &paramsOk);
        if (!paramsOk) {
            warning(this, "Custom Fit",
                    "Enter fit parameters as name=guess pairs, e.g. \"a=1, b=0.5\".");
            return;
        }
        const CustomFit fit = fitCustomCurve(expr, initial, xs, ys, xmin, xmax, Ncurve);
        if (!fit.ok) {
            warning(this, "Custom Fit",
                    QString("The fit could not be completed:\n%1").arg(fit.error));
            return;
        }
        const QString label   = fitLabelEdit->text().trimmed();
        const QString fitName = label.isEmpty() ? expr : label;
        chart->setFitCurve(fit.curve, fitName);
        setProcessedLabel(fitName.length() > 12 ? "Custom fit" : fitName);
        resetRangeSliders();        // a fit re-fits to the whole data set; match the sliders
        smooth->setCurrentIndex(2); // "Both" = raw data + fit overlay

        QString report = QString("Custom fit of f(x) = %1\n").arg(expr);
        if (!label.isEmpty()) report += QString("(%1)\n").arg(label);
        report += "\n";
        for (const auto &p : fit.params)
            report += QString("  %1 = %2\n").arg(p.name).arg(p.value, 0, 'g', 8);
        report += QString("\n  RMS residual = %1\n  iterations   = %2")
                      .arg(fit.rms, 0, 'g', 6)
                      .arg(fit.iterations);
        information(this, "Custom Fit", report);
        return;
    }

    if (which == AnaMaxBolt) { // Maxwell-Boltzmann distribution of per-atom energies
        // f(E) = A * E^(d/2 - 1) * exp(-E/kT), the distribution of the kinetic
        // energy of d degrees of freedom.  The amplitude is fitted rather than
        // derived, because a histogram carries an arbitrary normalization: raw
        // counts, a normalized fraction, and a density all differ by a constant
        // that says nothing about the temperature.
        const int ndim = paramSpin->value();

        // E = 0 is outside the domain for d = 1, and negative energies are not
        // in it at all; dropping them keeps the model finite everywhere it is
        // evaluated instead of letting one point poison the residuals
        std::vector<double> exs, eys, ees;
        for (std::size_t i = 0; i < xs.size(); ++i)
            if (xs[i] > 0.0) {
                exs.push_back(xs[i]);
                eys.push_back(ys[i]);
                ees.push_back(es[i]);
            }
        const std::size_t dropped = xs.size() - exs.size();
        if (exs.size() < 3) {
            warning(this, "Maxwell-Boltzmann Fit",
                    "Fewer than 3 data points with a positive energy.\n"
                    "The x axis has to be the energy, not the bin index.");
            return;
        }

        // The initial guesses come from the data rather than from constants:
        // the distribution's mean is <E> = (d/2) kT, and the histogram weights
        // give that mean directly, which puts kT within a factor of two even
        // for a badly cut histogram.  The amplitude then follows from matching
        // the model's peak to the tallest bin.
        double sumy = 0.0, sumxy = 0.0, ymax = 0.0;
        for (std::size_t i = 0; i < exs.size(); ++i) {
            const double w = qMax(0.0, eys[i]); // negative weights are not counts
            sumy += w;
            sumxy += w * exs[i];
            ymax = qMax(ymax, eys[i]);
        }
        const double power = 0.5 * ndim - 1.0;
        double kt0         = (sumy > 0.0) ? (2.0 * sumxy / (sumy * ndim)) : 1.0;
        if (!(kt0 > 0.0)) kt0 = 1.0;
        double shape = 0.0; // the model's own peak height at kT = kt0, A = 1
        for (double x : exs)
            shape = qMax(shape, std::pow(x, power) * std::exp(-x / kt0));
        const double a0 = (shape > 0.0 && ymax > 0.0) ? (ymax / shape) : 1.0;

        // built for LeptonMini, whose "^" is exponentiation
        QString expr = QStringLiteral("A*exp(-x/kT)");
        if (ndim == 3) expr = QStringLiteral("A*sqrt(x)*exp(-x/kT)");
        if (ndim == 1) expr = QStringLiteral("A*exp(-x/kT)/sqrt(x)");

        // Which residuals the fit should care about.  A measured distribution
        // is rarely a Maxwell-Boltzmann distribution exactly, and then the
        // weighting decides which part of it the one curve follows.  Counting
        // every bin in proportion to its population keeps the fit on the bulk
        // of the distribution and its peak; it is also scale-free, so it works
        // the same whether the histogram holds counts, fractions, or a density.
        // Weighting by the error bars instead is the textbook choice, but on a
        // histogram it favors the sparse tail, whose bars are the smallest.
        const int weighting = weightCombo->currentIndex();
        std::vector<double> weights;
        QString weightNote;
        if (weighting == 0) { // by bin population
            weights.reserve(eys.size());
            for (double y : eys)
                weights.push_back(qMax(0.0, y));
            weightNote = QStringLiteral("weighted by bin population");
        } else if (weighting == 1) { // by the error bars, as 1/sigma^2
            double smallest = 0.0;
            for (double s : ees)
                if ((s > 0.0) && ((smallest == 0.0) || (s < smallest))) smallest = s;
            if (smallest > 0.0) {
                weights.reserve(ees.size());
                // a bin that came out identical in every block would otherwise
                // carry infinite weight; the smallest real spread stands in
                for (double s : ees)
                    weights.push_back(1.0 / ((s > 0.0) ? (s * s) : (smallest * smallest)));
                weightNote = QStringLiteral("weighted by 1/sigma^2 of the error bars");
            } else {
                weightNote = QStringLiteral("unweighted (the data carries no error bars)");
            }
        } else {
            weightNote = QStringLiteral("unweighted");
        }

        const QList<FitParam> initial = {{QStringLiteral("A"), a0}, {QStringLiteral("kT"), kt0}};
        const auto emm                = std::minmax_element(exs.begin(), exs.end());
        const CustomFit fit = fitCustomCurve(expr, initial, exs, eys, *emm.first, *emm.second,
                                             Ncurve, QStringLiteral("x"), weights);
        if (!fit.ok) {
            warning(this, "Maxwell-Boltzmann Fit",
                    QString("The fit could not be completed:\n%1").arg(fit.error));
            return;
        }

        double kt = 0.0, amp = 0.0;
        for (const auto &p : fit.params) {
            if (p.name == QLatin1String("kT")) kt = p.value;
            if (p.name == QLatin1String("A")) amp = p.value;
        }
        const QString fitName = QStringLiteral("Maxwell-Boltzmann");
        chart->setFitCurve(fit.curve, fitName);
        setProcessedLabel("M-B fit");
        resetRangeSliders();        // a fit re-fits to the whole data set; match the sliders
        smooth->setCurrentIndex(2); // "Both" = raw data + fit overlay

        // Equipartition fixes <E> = (d/2) kT whatever the shape of the
        // distribution, so the measured mean is a second, model-free estimate
        // of kT.  Reporting both turns a wrong d -- or a histogram that is not
        // the distribution being fitted -- from an invisible bias into a
        // visible disagreement.  It is the mean of the fitted points, so a
        // histogram whose tail was cut off makes it come out low.
        const double meanE    = (sumy > 0.0) ? (sumxy / sumy) : 0.0;
        const double ktMoment = 2.0 * meanE / ndim;

        QString report = QString("Maxwell-Boltzmann fit in %1 dimension(s), %2:\n"
                                 "  f(E) = %3\n\n"
                                 "  kT  = %4   (from the fitted shape)\n"
                                 "  A   = %5\n\n"
                                 "  <E> = %6   (measured)\n"
                                 "  kT  = %7   (from <E> = (d/2) kT alone)\n\n"
                                 "  RMS residual = %8\n  iterations   = %9\n")
                             .arg(ndim)
                             .arg(weightNote)
                             .arg(expr)
                             .arg(kt, 0, 'g', 8)
                             .arg(amp, 0, 'g', 8)
                             .arg(meanE, 0, 'g', 8)
                             .arg(ktMoment, 0, 'g', 8)
                             .arg(fit.rms, 0, 'g', 6)
                             .arg(fit.iterations);
        // a disagreement between the two is the data telling us that it is not
        // the distribution being fitted, which is worth saying out loud
        if ((kt > 0.0) && (ktMoment > 0.0) && (qAbs(kt - ktMoment) > 0.1 * qMax(kt, ktMoment))) {
            report += "\nThe two disagree by more than 10%, so this data is not quite the "
                      "distribution being fitted to it. Check the degrees of freedom: "
                      "constrained or rigid molecules have fewer than three per atom, and a "
                      "rigid 3-site water has two. Check as well that the histogram covers "
                      "the whole distribution, since a tail beyond its range is missing from "
                      "<E> and lowers it.\n";
        }
        if (dropped > 0)
            report += QString("\n%1 point(s) at E <= 0 were left out of the fit.\n")
                          .arg(static_cast<int>(dropped));
        // kT is in the energy units of the data, and the file does not say what
        // those are, so converting it to a temperature is left to the reader
        report += "\nkT is in the energy units of the plotted data; divide by the\n"
                  "Boltzmann constant in those units to obtain a temperature.";
        information(this, "Maxwell-Boltzmann Fit", report);
        return;
    }

    if (which == AnaPoly) { // polynomial fit
        const PolynomialFit f = polynomialFit(xs, ys, paramSpin->value());
        if (!f.ok) {
            warning(this, "Postprocess", "Polynomial fit failed (too few points).");
            return;
        }
        QList<QPointF> curve;
        for (int k = 0; k <= Ncurve; ++k) {
            const double x = xmin + (xmax - xmin) * k / Ncurve;
            curve.append(QPointF(x, evalPolynomial(f.coeffs, x)));
        }
        const QString polyName = QString("Poly deg %1").arg(static_cast<int>(f.coeffs.size()) - 1);
        chart->setFitCurve(curve, polyName);
        setProcessedLabel(polyName);
        resetRangeSliders();        // a fit re-fits to the whole data set; match the sliders
        smooth->setCurrentIndex(2); // "Both" = raw data + fit overlay

        QString report =
            QString("Polynomial fit of degree %1\n\n").arg(static_cast<int>(f.coeffs.size()) - 1);
        for (int i = 0; i < static_cast<int>(f.coeffs.size()); ++i)
            report += QString("  c[%1] = %2\n").arg(i).arg(f.coeffs[i], 0, 'g', 8);
        report += QString("\n  RMS residual = %1").arg(f.rms, 0, 'g', 6);
        information(this, "Polynomial Fit", report);
        return;
    }

    // Birch-Murnaghan EOS fit: second dialog — confirm columns + atoms per unit cell
    {
        const QString xLabel = chart->getXLabel();
        const QString yLabel = chart->getYLabel().isEmpty() ? chart->getName() : chart->getYLabel();

        QDialog eosConfirm(this);
        eosConfirm.setWindowTitle("Birch-Murnaghan EOS Fit — Column Setup");
        auto *eosLayout = new QVBoxLayout(&eosConfirm);
        eosLayout->addWidget(new QLabel(
            "The Birch-Murnaghan EOS fit expects volume on the x-axis and cohesive energy "
            "on the y-axis.\n\nThis chart has:"));
        auto *eosInfo = new QFormLayout;
        eosInfo->addRow("x-axis:", new QLabel("<b>" + xLabel + "</b>"));
        eosInfo->addRow("y-axis:", new QLabel("<b>" + yLabel + "</b>"));
        eosLayout->addLayout(eosInfo);
        eosLayout->addWidget(
            new QLabel("\nAtoms per unit cell N: the lattice constant is derived as\n"
                       "  a₀ = ∛(N × V₀)\n"
                       "Use the conventional unit cell (e.g. N=4 for FCC, N=2 for BCC/HCP).\n"
                       "Set N=1 only when the x-axis is already the conventional cell volume."));
        auto *natSpin = new QSpinBox;
        natSpin->setRange(1, 1000);
        natSpin->setValue(1);
        natSpin->setToolTip("Number of atoms in the conventional unit cell\n"
                            "(e.g. 4 for FCC, 2 for BCC/HCP).\n"
                            "Use N=1 when x is already the conventional cell volume.");
        auto *natForm = new QFormLayout;
        natForm->addRow("Atoms per unit cell N:", natSpin);
        eosLayout->addLayout(natForm);
        auto *eosBtns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        styleDialogButtons(eosBtns);
        connect(eosBtns, &QDialogButtonBox::accepted, &eosConfirm, &QDialog::accept);
        connect(eosBtns, &QDialogButtonBox::rejected, &eosConfirm, &QDialog::reject);
        eosLayout->addWidget(eosBtns);
        if (eosConfirm.exec() != QDialog::Accepted) return;

        const int natoms = natSpin->value();
        const EosFit f   = birchMurnaghanFit(xs, ys);
        if (!f.ok) {
            warning(this, "Postprocess",
                    "Birch-Murnaghan fit failed (needs >= 4 points, positive volumes, "
                    "and a minimum within the data).");
            return;
        }

        QList<QPointF> curve;
        for (int k = 0; k <= Ncurve; ++k) {
            const double x = xmin + (xmax - xmin) * k / Ncurve;
            if (x > 0.0) curve.append(QPointF(x, evalBirchMurnaghan(f, x)));
        }
        // EOS fit: hide in Raw mode, visible in EOS-fit/Both modes; raw data as points
        chart->setFitCurve(curve, "EOS fit");
        chart->setDisplayStyle(ChartDisplayMode::Points, chart->displayColor(),
                               chart->displayWidth(), chart->displayPointSize());
        setProcessedLabel("EOS fit");
        resetRangeSliders();        // a fit re-fits to the whole data set; match the sliders
        smooth->setCurrentIndex(2); // "Both" = raw points + EOS fit line

        // derive lattice constant: a0 = cbrt(N * V0)
        const double a0 = std::cbrt(static_cast<double>(natoms) * f.v0);

        // Show the result in a dialog with the rendered formula
        auto *resultDlg = new QDialog(this);
        resultDlg->setWindowTitle("Birch-Murnaghan EOS Fit");
        resultDlg->setAttribute(Qt::WA_DeleteOnClose);
        auto *dlgLayout = new QVBoxLayout(resultDlg);

        auto *fmtLabel = new QLabel;
        fmtLabel->setPixmap(QPixmap(":/icons/birch-murnaghan-eos.png"));
        fmtLabel->setAlignment(Qt::AlignCenter);
        dlgLayout->addWidget(fmtLabel);

        auto *legend = new QLabel("where <i>V</i> is the unit cell volume "
                                  "and <i>V</i><sub>0</sub> the equilibrium volume.");
        legend->setAlignment(Qt::AlignCenter);
        dlgLayout->addWidget(legend);

        auto *resultForm = new QFormLayout;
        auto makeVal     = [](double v, int prec) {
            auto *l = new QLabel(QString::number(v, 'g', prec));
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            return l;
        };
        resultForm->addRow("<b>V<sub>0</sub></b> &mdash; Equilibrium volume (from fit):",
                           makeVal(f.v0, 8));
        resultForm->addRow(
            QString("<b>a<sub>0</sub></b> &mdash; Lattice constant ∛(%1 &times; V<sub>0</sub>):")
                .arg(natoms),
            makeVal(a0, 8));
        resultForm->addRow("<b>E<sub>0</sub></b> &mdash; Cohesive energy at V<sub>0</sub>:",
                           makeVal(f.e0, 8));
        resultForm->addRow("<b>B<sub>0</sub></b> &mdash; Bulk modulus (&minus;V<sub>0</sub> dP/dV "
                           "at V<sub>0</sub>):",
                           makeVal(f.b0, 8));
        resultForm->addRow("<b>B<sub>0</sub>'</b> &mdash; Pressure derivative dB/dP at P=0:",
                           makeVal(f.b0prime, 6));
        resultForm->addRow("RMS residual:", makeVal(f.rms, 6));
        dlgLayout->addLayout(resultForm);

        auto *closeBtn = new QDialogButtonBox(QDialogButtonBox::Ok);
        styleDialogButtons(closeBtn);
        connect(closeBtn, &QDialogButtonBox::accepted, resultDlg, &QDialog::accept);
        dlgLayout->addWidget(closeBtn);
        resultDlg->exec();
    }
}

void ChartWindow::addDataFile()
{
    if (cols.empty()) return;

    const QString fileName = QFileDialog::getOpenFileName(this, "Add Data from File",
                                                          QDir::currentPath(), Cfg::FILTER_DATA);
    if (fileName.isEmpty()) return;

    QString error;
    auto dialog = PlotDataDialog::fromFile(fileName, this, &error);
    if (!dialog) {
        critical(this, "Add Data from File", "Could not read data from file:", error);
        return;
    }
    if (dialog->exec() != QDialog::Accepted) return;
    const PlotData plotData  = dialog->buildData();
    const PlotErrors plotErr = dialog->buildErrors();
    const QList<int> ycols   = dialog->yColumns();
    const int xcol           = dialog->xColumn();
    if (ycols.isEmpty() || xcol < 0 || xcol >= plotData.columnCount()) return;

    ChartViewer *chart = currentChart();
    if (!chart) return;

    const std::vector<double> &xvals = plotData.column(xcol);
    const int nrow                   = plotData.rowCount();

    // auto-color palette for overlay series (avoids primary raw/smooth colors)
    static const QList<QColor> palette = {
        QColor(220, 80, 40),  // red-orange
        QColor(40, 160, 40),  // green
        QColor(160, 40, 220), // purple
        QColor(180, 140, 0),  // amber
        QColor(0, 160, 180),  // teal
    };
    int colorIdx = chart->overlaySeriesCount();

    for (int ycol : ycols) {
        if (ycol < 0 || ycol >= plotData.columnCount()) continue;
        QList<QPointF> pts;
        pts.reserve(nrow);
        const std::vector<double> &yvals = plotData.column(ycol);
        for (int r = 0; r < nrow; ++r)
            pts.append(QPointF(xvals[r], yvals[r]));
        QList<double> errs, errsLo;
        columnErrors(plotErr, ycol, nrow, errs, errsLo);
        chart->addOverlaySeries(pts, plotData.columnName(ycol), palette[colorIdx % palette.size()],
                                errs, errsLo);
        ++colorIdx;
    }
    // new data was added (and re-fit to the full range): match the sliders to it
    resetRangeSliders();
}

void ChartWindow::referenceLines()
{
    if (cols.empty()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Reference Lines");
    dialog.setMinimumWidth(680); // room for the label field plus the anchor selector
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(
        new QLabel("Reference lines (vertical at an x value or horizontal at a y value) are\n"
                   "applied to every chart. Labels are drawn next to the line."));

    // scrollable list of (x, label, color) rows
    auto *listWidget = new QWidget;
    auto *listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(4, 4, 4, 4);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(listWidget);
    scroll->setMinimumHeight(100);
    // keep rows within the viewport width; only scroll vertically as lines are added
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(scroll, 1);

    // helper to build one color-button (same pattern as changeStyle)
    struct RowData {
        QComboBox *orientCombo;
        QDoubleSpinBox *xSpin;
        QLineEdit *labelEdit;
        QComboBox *anchorCombo;
        QColor color;
    };
    QList<RowData *> rows;
    QList<QPushButton *> colorBtns;

    auto addRow = [&](RefOrient orient, double val, const QString &lbl, const QColor &col,
                      RefAnchor anchor) {
        auto *rd        = new RowData;
        rd->orientCombo = new QComboBox;
        rd->orientCombo->addItems({"Vertical", "Horizontal"});
        rd->orientCombo->setCurrentIndex(orient == RefOrient::Horizontal ? 1 : 0);
        rd->xSpin = new QDoubleSpinBox;
        rd->xSpin->setDecimals(6);
        rd->xSpin->setRange(-1e15, 1e15);
        rd->xSpin->setValue(val);
        // keep the value field compact so the label field has room
        rd->xSpin->setMaximumWidth(110);
        rd->labelEdit = new QLineEdit(lbl);
        rd->labelEdit->setPlaceholderText("label");
        rd->color = col.isValid() ? col : QColor(80, 80, 80);

        // position label tracks the orientation: "x =" for vertical, "y =" for horizontal
        auto *posLabel = new QLabel;
        auto updatePos = [posLabel](int idx) {
            posLabel->setText(idx == 1 ? "y =" : "x =");
        };
        updatePos(rd->orientCombo->currentIndex());
        QObject::connect(rd->orientCombo, &QComboBox::currentIndexChanged, &dialog, updatePos);

        // label anchor along the line; the item texts track the orientation
        rd->anchorCombo = new QComboBox;
        rd->anchorCombo->addItem("Top", static_cast<int>(RefAnchor::Start));
        rd->anchorCombo->addItem("Center", static_cast<int>(RefAnchor::Center));
        rd->anchorCombo->addItem("Bottom", static_cast<int>(RefAnchor::End));
        rd->anchorCombo->setCurrentIndex(static_cast<int>(anchor));
        auto *anchorCombo = rd->anchorCombo;
        auto updateAnchor = [anchorCombo](int idx) {
            const bool horiz = (idx == 1);
            anchorCombo->setItemText(0, horiz ? "Left" : "Top");
            anchorCombo->setItemText(2, horiz ? "Right" : "Bottom");
        };
        updateAnchor(rd->orientCombo->currentIndex());
        QObject::connect(rd->orientCombo, &QComboBox::currentIndexChanged, &dialog, updateAnchor);

        auto *colorBtn = new QPushButton;
        auto updateBtn = [colorBtn](const QColor &c) {
            colorBtn->setText(c.name());
            colorBtn->setStyleSheet(QString("background-color: %1; color: %2;")
                                        .arg(c.name(), c.lightness() < 128 ? "white" : "black"));
        };
        updateBtn(rd->color);
        QObject::connect(colorBtn, &QPushButton::clicked, &dialog, [rd, colorBtn, updateBtn]() {
            const QColor c = QColorDialog::getColor(rd->color, colorBtn, "Line Color");
            if (c.isValid()) {
                rd->color = c;
                updateBtn(c);
            }
        });

        auto *delBtn = new QPushButton("×");
        delBtn->setFixedWidth(24);

        auto *row = new QHBoxLayout;
        row->addWidget(rd->orientCombo);
        row->addWidget(posLabel);
        row->addWidget(rd->xSpin, 0);
        row->addWidget(new QLabel("Label:"));
        row->addWidget(rd->labelEdit, 1);
        row->addWidget(new QLabel("Pos:"));
        row->addWidget(rd->anchorCombo);
        row->addWidget(new QLabel("Color:"));
        row->addWidget(colorBtn);
        row->addWidget(delBtn);
        listLayout->addLayout(row);

        rows.append(rd);
        colorBtns.append(colorBtn);

        // remove this row when "×" is clicked
        QObject::connect(delBtn, &QPushButton::clicked, &dialog,
                         [rd, &rows, &colorBtns, colorBtn, row]() {
                             rows.removeOne(rd);
                             colorBtns.removeOne(colorBtn);
                             delete rd;
                             // hide all widgets in the row
                             QLayoutItem *item;
                             while ((item = row->takeAt(0)) != nullptr) {
                                 if (item->widget()) item->widget()->hide();
                                 delete item;
                             }
                             delete row;
                         });
    };

    // populate with existing lines
    for (const auto &rl : refLines)
        addRow(rl.orient, rl.value, rl.label, rl.color, rl.anchor);

    auto *addBtn = new QPushButton("Add line");
    QObject::connect(addBtn, &QPushButton::clicked, &dialog, [&]() {
        addRow(RefOrient::Vertical, 0.0, QString(), QColor(80, 80, 80), RefAnchor::Start);
    });
    layout->addWidget(addBtn);

    // window-wide label style: font size, gap from the line, and a framed/opaque background
    auto *styleRow = new QHBoxLayout;
    auto *fontSpin = new QDoubleSpinBox;
    fontSpin->setRange(5.0, 30.0);
    fontSpin->setSingleStep(0.5);
    fontSpin->setValue(refLabelSize);
    auto *distSpin = new QSpinBox;
    distSpin->setRange(0, 50);
    distSpin->setValue(static_cast<int>(refLabelDist));
    auto *boxedCheck = new QCheckBox("Boxed labels");
    boxedCheck->setChecked(refLabelBoxed);
    styleRow->addWidget(new QLabel("Label font:"));
    styleRow->addWidget(fontSpin);
    styleRow->addWidget(new QLabel("Gap:"));
    styleRow->addWidget(distSpin);
    styleRow->addWidget(boxedCheck);
    styleRow->addStretch(1);
    layout->addLayout(styleRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    styleDialogButtons(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        qDeleteAll(rows);
        return;
    }

    // rebuild the refLines list (window-wide) and apply to the active column;
    // changeChart re-applies them when switching to another column
    refLines.clear();
    for (const auto *rd : rows) {
        const RefOrient o = rd->orientCombo->currentIndex() == 1 ? RefOrient::Horizontal
                                                                 : RefOrient::Vertical;
        const auto a      = static_cast<RefAnchor>(rd->anchorCombo->currentData().toInt());
        refLines.append({o, rd->xSpin->value(), rd->labelEdit->text().trimmed(), rd->color, a});
    }
    qDeleteAll(rows);

    // store and apply the window-wide label style
    refLabelSize  = fontSpin->value();
    refLabelDist  = distSpin->value();
    refLabelBoxed = boxedCheck->isChecked();
    viewer->setRefLabelStyle(refLabelSize, refLabelDist, refLabelBoxed);
    QSettings rls;
    rls.beginGroup(Keys::GROUP_CHARTS);
    rls.setValue(Keys::REFLABELSIZE, refLabelSize);
    rls.setValue(Keys::REFLABELDIST, refLabelDist);
    rls.setValue(Keys::REFLABELBOX, refLabelBoxed);
    rls.endGroup();

    if (!cols.empty()) viewer->setReferenceLines(refLines);
}

void ChartWindow::selectSmooth(int)
{
    smoothFlagsFromChoice(smooth->currentIndex(), doRaw, doSmooth);
    // the processed-slot label does not depend on the Raw/Smooth/Both choice; it
    // is "Smooth" unless a post-process fit overrode it (set in postProcess and
    // restored on column switch in changeChart)
    const bool hasCustom = currentChart() && currentChart()->hasCustom();
    // SG smooth parameters are only relevant when smoothing without a fit overlay
    const bool sgEnabled = doSmooth && !hasCustom;
    window->setEnabled(sgEnabled);
    order->setEnabled(sgEnabled);
    updateSmooth();
    // toggling Raw/Smooth/Both is a view-only change: keep the current slider
    // window, just re-derive the displayed range from it (the data range may have
    // grown/shrunk as the smoothed series was shown/hidden)
    applySliderWindow();
}

void ChartWindow::updateSmooth()
{
    int wval = window->value();
    int oval = order->value();

    // update every column's flags (so a hidden column is correct when selected),
    // but only the active column is on the plot and needs a redraw
    for (auto &c : cols)
        setColumnSmoothFlags(*c, doRaw, doSmooth, wval, oval);
    if (!cols.empty()) viewer->updateSmooth();
}

void ChartWindow::updateTLabel()
{
    // the chart title is shared by all columns on the single plot
    if (chartTitle && !cols.empty()) viewer->setTLabel(chartTitle->text());
}

void ChartWindow::updateYLabel()
{
    // the Y-axis label is per-column; update the active column and remember it
    if (active >= 0) {
        const QString label  = chartYlabel->text();
        cols[active]->yTitle = label;
        // The in-plot legend labels the raw series by its name; keep that in
        // sync with the editable Y-axis title rather than the fixed thermo
        // column id.  The raw points share the line's name so the two dedup
        // into a single legend entry.
        cols[active]->series->name = label;
        if (cols[active]->scatter) cols[active]->scatter->name = label;
        viewer->setYLabel(label);
    }
}

void ChartWindow::updateXLabel()
{
    // the X-axis label is shared by all columns on the single plot
    if (!chartXlabel || cols.empty()) return;
    viewer->setXLabel(chartXlabel->text());
}

void ChartWindow::updateXRange(int low, int high)
{
    if (cols.empty()) return;
    auto ranges = viewer->getMinMax();
    double xmin = ranges.left() + static_cast<double>(low) * SLIDER_FRACTION * ranges.width();
    double xmax = ranges.left() + static_cast<double>(high) * SLIDER_FRACTION * ranges.width();
    viewer->setXAxisRange(xmin, xmax);
}

void ChartWindow::updateYRange(int low, int high)
{
    if (cols.empty()) return;
    auto ranges = viewer->getMinMax();
    double ymin = ranges.bottom() - static_cast<double>(low) * SLIDER_FRACTION * ranges.height();
    double ymax = ranges.bottom() - static_cast<double>(high) * SLIDER_FRACTION * ranges.height();
    viewer->setYAxisRange(ymin, ymax);
}

void ChartWindow::saveAs()
{
    if (cols.empty()) return;
    const QString defaultname = defaultFileStem(filename) + "." + columns->currentText() + ".png";
    QImage chartimage         = viewer->grab().toImage();
    exportImage(this, &chartimage, "ChartWindow", defaultname);
}

PlotData ChartWindow::chartsToPlotData() const
{
    PlotData data;
    if (cols.empty()) return data;

    // A flat table has one x column, so every exported series has to live on
    // one grid: the x values of the first chart.  The raw values are always
    // written -- they are the data, and losing them to a display setting would
    // be a poor trade -- and the results of the post-processing follow.
    const PlotSeries &ref = *cols.front()->series;
    const int nrow        = ref.count();
    if (nrow < 1) return data;

    std::vector<double> xs;
    xs.reserve(nrow);
    for (int i = 0; i < nrow; ++i)
        xs.push_back(ref.at(i).x());
    data.addColumn(QStringLiteral("Step"), xs);

    // whether a series can be written against those x values as they stand
    auto sameGrid = [&xs, nrow](const PlotSeries *s) {
        if (!s || (s->count() != nrow)) return false;
        for (int i = 0; i < nrow; ++i)
            if (s->at(i).x() != xs[static_cast<std::size_t>(i)]) return false;
        return true;
    };
    auto yValues = [nrow](const PlotSeries *s) {
        std::vector<double> v;
        v.reserve(nrow);
        for (int i = 0; i < nrow; ++i)
            v.push_back(s->at(i).y());
        return v;
    };
    // a column name has to survive whitespace-separated and comma-separated
    // formats alike, and fit labels are free text (an expression, say)
    auto exportName = [](QString name) {
        static const QRegularExpression separators(QStringLiteral("[\\s,]+"));
        return name.replace(separators, QStringLiteral("_"));
    };

    for (const auto &c : cols) {
        const PlotSeries *s = c->series.get();
        // a chart on a grid of its own cannot share this table; in practice all
        // charts of a window are filled from the same x values
        if (!sameGrid(s)) continue;
        const QString name = exportName(s->name);
        data.addColumn(name, yValues(s));

        // error bars go next to the values they belong to; re-importing the
        // file simply yields one more data column.  Bars that reach up and
        // down by different amounts need two.
        if (s->hasAsymErrors()) {
            std::vector<double> lo, hi;
            lo.reserve(nrow);
            hi.reserve(nrow);
            for (int i = 0; i < nrow; ++i) {
                lo.push_back(s->errLow(i));
                hi.push_back(s->errHigh(i));
            }
            data.addColumn(name + "-errlo", std::move(lo));
            data.addColumn(name + "-errhi", std::move(hi));
        } else if (s->hasErrors()) {
            std::vector<double> err;
            err.reserve(nrow);
            for (int i = 0; i < nrow; ++i)
                err.push_back(s->errHigh(i));
            data.addColumn(name + "-err", std::move(err));
        }

        // The smoothed curve shares the raw x values by construction.  It is
        // computed here rather than read off the column, because the single
        // shared view only ever computes it for the chart it is showing, and
        // which chart that is should not decide what a file contains.
        if (c->doSmooth && !c->custom && (s->count() > 2 * c->window)) {
            const QList<QPointF> sm = calc_sgsmooth(s->points, c->window, c->order);
            if (sm.size() == nrow) {
                std::vector<double> ys;
                ys.reserve(nrow);
                for (const QPointF &p : sm)
                    ys.push_back(p.y());
                data.addColumn(name + "-smooth", std::move(ys));
            }
        }

        // A fit curve is sampled on a dense grid of its own over the data
        // range, so it is written as the fitted function evaluated at each data
        // x -- which is also what makes it comparable to the values beside it.
        if (c->fit && c->fit->isVisible() && (c->fit->count() > 1)) {
            std::vector<double> fit;
            fit.reserve(nrow);
            for (int i = 0; i < nrow; ++i)
                fit.push_back(interpolateCurve(c->fit->points, xs[static_cast<std::size_t>(i)]));
            data.addColumn(exportName(c->fit->name.isEmpty() ? name + "-fit" : c->fit->name),
                           std::move(fit));
        }

        // overlay series carry their own x values, and resampling data that was
        // measured elsewhere would be inventing it, so only one that already
        // sits on this grid can join the table
        for (const auto &o : c->overlaySeries)
            if (o && o->isVisible() && sameGrid(o.get()))
                data.addColumn(exportName(o->name) + "-added", yValues(o.get()));
    }
    return data;
}

// write the already formatted chart data to a file
static void writeExport(QWidget *parent, const QString &caption, const QString &defaultname,
                        const QString &filter, const QString &suffix, const QString &text)
{
    QString fileName = QFileDialog::getSaveFileName(
        parent, caption, QDir::current().absoluteFilePath(defaultname), filter);
    if (fileName.isEmpty()) return;
    fileName = ensureFileSuffix(fileName, suffix);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << text;
        file.close();
    }
}

void ChartWindow::exportDat()
{
    if (cols.empty()) return;
    writeExport(this, "Save Chart as Gnuplot data", defaultFileStem(filename) + ".dat",
                Cfg::FILTER_GNUPLOT, "dat", writePlotDat(chartsToPlotData(), filename));
}

void ChartWindow::exportCsv()
{
    if (cols.empty()) return;
    writeExport(this, "Save Chart as CSV data", defaultFileStem(filename) + ".csv", Cfg::FILTER_CSV,
                "csv", writePlotCsv(chartsToPlotData()));
}

void ChartWindow::exportYaml()
{
    if (cols.empty()) return;
    writeExport(this, "Save Chart as YAML data", defaultFileStem(filename) + ".yaml",
                Cfg::FILTER_YAML, "yaml", writePlotYaml(chartsToPlotData()));
}

void ChartWindow::changeChart(int)
{
    // bind the single view to the newly selected column and render it. The chart
    // title and X-axis label are window-wide and stay put; only the per-column
    // Y-axis label is restored here.
    active = activeIndex();
    if (active >= 0) {
        viewer->setColumn(cols[active].get());
        viewer->setReferenceLines(refLines); // re-apply window reference lines to this column
        chartYlabel->setText(cols[active]->yTitle);
        // restore this column's processed-slot label ("Smooth" or its fit name)
        smooth->setItemText(1, cols[active]->procLabel);
    }

    // sync the SG parameter spinbox state (irrelevant while a fit overrides the slot)
    const bool hasCustom = currentChart() && currentChart()->hasCustom();
    const bool sgEnabled = doSmooth && !hasCustom;
    window->setEnabled(sgEnabled);
    order->setEnabled(sgEnabled);

    // a chart switch shows the new column at full range (setColumn re-fit it)
    resetRangeSliders();
}

void ChartWindow::closeEvent(QCloseEvent *event)
{
    if (!isMaximized() && !dockedLayout()) {
        QSettings settings;
        settings.setValue(Keys::CHARTX, width());
        settings.setValue(Keys::CHARTY, height());
    }
    QWidget::closeEvent(event);
}

/* -------------------------------------------------------------------- */

// ---- column rendering pipeline ------------------------------------------
// These free functions render a ChartColumn onto a given PlotWidget. They are
// deliberately independent of any particular ChartViewer instance so that the
// single shared PlotWidget can be pointed at any column. ChartViewer's methods
// below are thin forwarders onto them.

namespace {

// Savitzky-Golay smoothing of an (x,y) point series: the y values are smoothed
// via the shared least-squares core while the x values are preserved.
QList<QPointF> calc_sgsmooth(const QList<QPointF> &input, std::size_t window, int order)
{
    const std::size_t ndat = input.count();
    if (ndat < ((2 * window) + 2)) window = (ndat / 2) - 1;

    if (window > 1) {
        float_vect in(ndat);
        QList<QPointF> rv(input);

        for (std::size_t i = 0; i < ndat; ++i)
            in[i] = input[i].y();

        float_vect out = sg_smooth(in, window, order);

        for (std::size_t i = 0; i < ndat; ++i)
            rv[i].setY(out[i]);

        return rv;
    }
    return input;
}

// Min/max of a column: the cached raw bounds plus any smoothed, fit, or overlay
// curves, widened by a small y margin so extrema are not drawn on the plot
// frame itself. Pure -- touches no PlotWidget.
QRectF columnMinMax(const ChartColumn &col)
{
    qreal xmin = col.rawXmin;
    qreal xmax = col.rawXmax;
    qreal ymin = col.rawYmin;
    qreal ymax = col.rawYmax;

    // if plotting the smoothed data, include its range too
    if (col.doSmooth && col.smooth) {
        for (auto &p : col.smooth->points) {
            xmin = qMin(xmin, p.x());
            xmax = qMax(xmax, p.x());
            ymin = qMin(ymin, p.y());
            ymax = qMax(ymax, p.y());
        }
    }

    // include any visible fit/overlay curve (EOS, polynomial, custom)
    if (col.fit && col.fit->isVisible() && !col.fit->points.isEmpty()) {
        for (auto &p : col.fit->points) {
            xmin = qMin(xmin, p.x());
            xmax = qMax(xmax, p.x());
            ymin = qMin(ymin, p.y());
            ymax = qMax(ymax, p.y());
        }
    }

    // include extra overlay data series added from secondary files
    for (auto &s : col.overlaySeries) {
        if (s && s->isVisible()) {
            for (int i = 0; i < s->points.size(); ++i) {
                const QPointF &p = s->points[i];
                xmin             = qMin(xmin, p.x());
                xmax             = qMax(xmax, p.x());
                ymin             = qMin(ymin, p.y() - s->errLow(i));
                ymax             = qMax(ymax, p.y() + s->errHigh(i));
            }
        }
    }
    // note: vlines (reference lines) are decorative and excluded

    // avoid (nearly) empty ranges on either axis
    padEmptyRange(xmin, xmax);
    padEmptyRange(ymin, ymax);

    // add a little buffer space between the data extremes and the y-axis limits;
    // a tighter framing is still available through the range sliders
    const double ypad = Cfg::CHART_YPAD_FRACTION * (ymax - ymin);
    ymin -= ypad;
    ymax += ypad;

    return {xmin, ymax, xmax - xmin, ymin - ymax};
}

// Register a series on the plot with the given color/width.
void addColumnSeries(PlotWidget *plot, PlotSeries *s, const QColor &color, qreal width)
{
    s->color = color;
    if (s->type == PlotSeriesType::Line) s->width = width;
    plot->addSeries(s);
}

// Restyle an already-registered series and repaint.
void styleColumnSeries(PlotWidget *plot, PlotSeries *s, const QColor &color, qreal width)
{
    s->color = color;
    if (s->type == PlotSeriesType::Line) s->width = width;
    plot->update();
}

// Draw a line series and, per the display mode, an accompanying scatter series
// (created on demand and kept in sync with the line).
void renderColumnSeries(PlotWidget *plot, PlotSeries *line, std::unique_ptr<PlotSeries> &points,
                        ChartDisplayMode mode, const QColor &color, qreal width, qreal pointSize)
{
    const bool wantLines  = (mode != ChartDisplayMode::Points);
    const bool wantPoints = (mode != ChartDisplayMode::Lines);

    // line series
    if (!plot->hasSeries(line))
        addColumnSeries(plot, line, color, width);
    else
        styleColumnSeries(plot, line, color, width);
    line->setVisible(wantLines);

    // matching points, created on demand and kept in sync with the line
    if (wantPoints) {
        if (!points) {
            points       = std::make_unique<PlotSeries>();
            points->type = PlotSeriesType::Scatter;
        }
        points->name = line->name; // share the line's name so the legend dedups them
        points->replace(line->points);
        // exactly one of the two visible series carries the error bars, so they
        // are neither drawn twice nor lost when the line itself is hidden
        if (!wantLines) {
            points->yerr   = line->yerr;
            points->yerrLo = line->yerrLo;
        }
        if (!plot->hasSeries(points.get()))
            addColumnSeries(plot, points.get(), color, width);
        else
            styleColumnSeries(plot, points.get(), color, width);
        points->markerSize = pointSize;
        points->setVisible(true);
    } else if (points) {
        points->setVisible(false);
    }
}

// Give every series of a column that carries error bars the column's error bar
// style, so that the bars read as one annotation layer across raw, processed,
// and overlay data rather than as part of the curve they belong to.
void styleColumnErrors(ChartColumn &col, const QColor &color, qreal width)
{
    auto apply = [&color, width](PlotSeries *s) {
        if (!s) return;
        s->errColor = color;
        s->errWidth = width;
    };
    apply(col.series.get());
    apply(col.scatter.get());
    apply(col.smooth.get());
    apply(col.smoothScatter.get());
    for (auto &s : col.overlaySeries)
        apply(s.get());
}

// Recompute and (re)draw a column's raw and smoothed series onto the plot.
void refreshColumn(PlotWidget *plot, ChartColumn &col)
{
    // a column without a color of its own draws in the configured one
    const QColor rawcol = col.rawColor.isValid()
                              ? col.rawColor
                              : configuredChartColor(Keys::RAWBRUSH, Cfg::RAWBRUSH_DEFAULT);
    const QColor smcol  = col.smoothcolor.isValid()
                              ? col.smoothcolor
                              : configuredChartColor(Keys::SMOOTHBRUSH, Cfg::SMOOTHBRUSH_DEFAULT);
    const QColor errcol = col.errColor.isValid()
                              ? col.errColor
                              : configuredChartColor(Keys::ERRBRUSH, Cfg::ERRBRUSH_DEFAULT);

    if (col.doRaw)
        renderColumnSeries(plot, col.series.get(), col.scatter, col.dispmode, rawcol, col.rawWidth,
                           col.rawPointSize);

    if (col.doSmooth) {
        if (col.custom && col.fit && !col.fit->points.isEmpty()) {
            // the custom curve acts as the "processed" series; suppress the SG smooth
            col.fit->setVisible(true);
            if (col.smooth) col.smooth->setVisible(false);
            if (col.smoothScatter) col.smoothScatter->setVisible(false);
        } else if (!col.custom && col.series->count() > (2 * col.window)) {
            if (col.fit) col.fit->setVisible(false);
            if (!col.smooth) {
                col.smooth       = std::make_unique<PlotSeries>();
                col.smooth->name = QStringLiteral("Smooth"); // legend label for the SG series
            }
            col.smooth->replace(calc_sgsmooth(col.series->points, col.window, col.order));
            renderColumnSeries(plot, col.smooth.get(), col.smoothScatter, col.smoothmode, smcol,
                               col.smoothwidth, col.smoothpointsize);
        }
    } else {
        if (col.custom && col.fit) col.fit->setVisible(false);
    }
    // after rendering, so that series created on demand above are styled too
    styleColumnErrors(col, errcol, col.errWidth);
    plot->update();
}

// Reset the plot ranges to fit the column's data and re-anchor its reference lines.
void resetColumnZoom(PlotWidget *plot, ChartColumn &col)
{
    auto ranges = columnMinMax(col);
    // update reference lines to span the current data range along their axis
    const double ybot = ranges.bottom();
    const double ytop = ranges.top();
    for (std::size_t i = 0; i < col.vlines.size(); ++i) {
        const RefLine &rl = col.reflineDefs[static_cast<int>(i)];
        if (rl.orient == RefOrient::Vertical)
            col.vlines[i]->replace(QList<QPointF>{{rl.value, ybot}, {rl.value, ytop}});
        else
            col.vlines[i]->replace(
                QList<QPointF>{{ranges.left(), rl.value}, {ranges.right(), rl.value}});
    }
    plot->setXRange(ranges.left(), ranges.right());
    plot->setYRange(ybot, ytop);
    plot->update();
}

// Append a point to a column's raw series (monotonic in x) and update its cached
// bounds. Pure data: returns true if the point was appended (x advanced).
bool appendColumnPoint(ChartColumn &col, double x, double y)
{
    if (col.lastX >= x) return false;
    col.lastX = x;
    col.series->append(x, y);
    col.rawXmin = qMin(col.rawXmin, x);
    col.rawXmax = qMax(col.rawXmax, x);
    col.rawYmin = qMin(col.rawYmin, y);
    col.rawYmax = qMax(col.rawYmax, y);
    return true;
}

// Set the raw-series display style and redraw.
void setColumnDisplayStyle(PlotWidget *plot, ChartColumn &col, ChartDisplayMode mode,
                           const QColor &color, qreal width, qreal pointSize)
{
    col.dispmode     = mode;
    col.rawColor     = color;
    col.rawWidth     = width;
    col.rawPointSize = pointSize;
    refreshColumn(plot, col);
    resetColumnZoom(plot, col);
}

// Set the processed-series display style and redraw.
void setColumnSmoothStyle(PlotWidget *plot, ChartColumn &col, ChartDisplayMode mode,
                          const QColor &color, qreal width, qreal pointSize)
{
    col.smoothmode      = mode;
    col.smoothcolor     = color;
    col.smoothwidth     = width;
    col.smoothpointsize = pointSize;
    refreshColumn(plot, col);
    resetColumnZoom(plot, col);
}

// Set the error bar style of every series of the column and redraw.
void setColumnErrorStyle(PlotWidget *plot, ChartColumn &col, const QColor &color, qreal width)
{
    col.errColor = color;
    col.errWidth = width;
    refreshColumn(plot, col);
}

// Set or replace the custom curve (fit, function, or overlay) of the column.
void setColumnFitCurve(PlotWidget *plot, ChartColumn &col, const QList<QPointF> &points,
                       const QString &name)
{
    col.custom = true;
    if (!col.fit) {
        col.fit = std::make_unique<PlotSeries>();
        addColumnSeries(plot, col.fit.get(), QColor(220, 30, 30), 2.0); // distinct fit-curve color
    }
    if (!name.isEmpty()) col.fit->name = name;
    col.fit->replace(points);
    // visibility follows doSmooth: refreshColumn shows/hides it correctly
    refreshColumn(plot, col);
    resetColumnZoom(plot, col);
}

// Remove the fit-curve overlay and return the processed-series slot to the
// Savitzky-Golay smooth, which refreshColumn() recomputes on demand.
void clearColumnFitCurve(PlotWidget *plot, ChartColumn &col)
{
    col.custom = false;
    if (col.fit) {
        col.fit->replace({});
        col.fit->setVisible(false);
    }
    refreshColumn(plot, col);
    resetColumnZoom(plot, col);
}

// Add an extra overlay data series (from a secondary file) to the column.
void addColumnOverlay(PlotWidget *plot, ChartColumn &col, const QList<QPointF> &pts,
                      const QString &name, const QColor &color, const QList<double> &yerr,
                      const QList<double> &yerrLo)
{
    auto s  = std::make_unique<PlotSeries>();
    s->name = name;
    s->replace(pts);
    if (yerr.size() == pts.size()) {
        s->yerr = yerr;
        if (yerrLo.size() == pts.size()) s->yerrLo = yerrLo;
    }
    addColumnSeries(plot, s.get(), color, col.rawWidth);
    col.overlaySeries.push_back(std::move(s));
    refreshColumn(plot, col); // hands the new series the column's error bar style
    resetColumnZoom(plot, col);
}

// Remove all reference lines from the column and the plot.
void clearColumnVerticalLines(PlotWidget *plot, ChartColumn &col)
{
    for (auto &s : col.vlines)
        plot->removeSeries(s.get());
    col.vlines.clear();
    col.reflineDefs.clear();
}

// Replace the column's reference lines with the given definitions.
void setColumnReferenceLines(PlotWidget *plot, ChartColumn &col, const QList<RefLine> &lines)
{
    clearColumnVerticalLines(plot, col);
    if (lines.isEmpty()) return;
    auto ranges = columnMinMax(col);
    for (const auto &rl : lines) {
        auto s  = std::make_unique<PlotSeries>();
        s->name = rl.label;
        if (rl.orient == RefOrient::Vertical)
            s->replace(QList<QPointF>{{rl.value, ranges.bottom()}, {rl.value, ranges.top()}});
        else
            s->replace(QList<QPointF>{{ranges.left(), rl.value}, {ranges.right(), rl.value}});
        const QColor c = rl.color.isValid() ? rl.color : QColor(80, 80, 80);
        // dashed reference line, with an optional label drawn next to it
        s->style = Qt::DashLine;
        if (!rl.label.isEmpty()) {
            s->isReference = true;
            s->refLabel    = rl.label;
            s->refAnchor   = rl.anchor;
        }
        addColumnSeries(plot, s.get(), c, 1.5);
        col.vlines.push_back(std::move(s));
        col.reflineDefs.append(rl);
    }
    // reference lines are annotations anchored to the full data extent (and
    // clipped to the view); they do not change the data range, so leave the
    // displayed range -- and the range sliders that drive it -- untouched
    plot->update();
}

// Seed a fresh column's series styles from the chart preferences.  Colors are
// deliberately left invalid: refreshColumn() resolves those against the
// preferences on every redraw, so a color edited in the preferences dialog
// reaches charts that already exist.  Out-of-range values from a hand-edited
// settings file are clamped rather than honored, so that no setting can produce
// an invisible curve.
void applyColumnStyleDefaults(ChartColumn &col)
{
    auto mode = [](const QVariant &value) {
        const int m = value.toInt();
        if ((m < static_cast<int>(ChartDisplayMode::Lines)) ||
            (m > static_cast<int>(ChartDisplayMode::LinesAndPoints)))
            return ChartDisplayMode::Lines;
        return static_cast<ChartDisplayMode>(m);
    };

    QSettings settings;
    settings.beginGroup(Keys::GROUP_CHARTS);
    const int lines  = static_cast<int>(ChartDisplayMode::Lines);
    col.dispmode     = mode(settings.value(Keys::RAWMODE, lines));
    col.smoothmode   = mode(settings.value(Keys::SMOOTHMODE, lines));
    col.rawWidth     = qBound(Cfg::LINE_WIDTH_MIN,
                              settings.value(Keys::RAWWIDTH, Cfg::LINE_WIDTH_DEFAULT).toDouble(),
                              Cfg::LINE_WIDTH_MAX);
    col.smoothwidth  = qBound(Cfg::LINE_WIDTH_MIN,
                              settings.value(Keys::SMOOTHWIDTH, Cfg::LINE_WIDTH_DEFAULT).toDouble(),
                              Cfg::LINE_WIDTH_MAX);
    col.errWidth     = qBound(Cfg::LINE_WIDTH_MIN,
                              settings.value(Keys::ERRWIDTH, Cfg::ERR_WIDTH_DEFAULT).toDouble(),
                              Cfg::LINE_WIDTH_MAX);
    col.rawPointSize = qBound(
        Cfg::POINT_SIZE_MIN, settings.value(Keys::RAWPOINTSIZE, Cfg::POINT_SIZE_DEFAULT).toDouble(),
        Cfg::POINT_SIZE_MAX);
    col.smoothpointsize =
        qBound(Cfg::POINT_SIZE_MIN,
               settings.value(Keys::SMOOTHPOINTSIZE, Cfg::POINT_SIZE_DEFAULT).toDouble(),
               Cfg::POINT_SIZE_MAX);
    settings.endGroup();
}

// Apply smoothing flags/parameters to the column WITHOUT redrawing (so a
// non-active column can be updated without touching the shared plot).
void setColumnSmoothFlags(ChartColumn &col, bool doRaw, bool doSmooth, int window, int order)
{
    // hide raw plot (keep the series alive; data is still needed for smoothing)
    if (!doRaw) {
        if (col.series) col.series->setVisible(false);
        if (col.scatter) col.scatter->setVisible(false);
    }
    // hide processed plot (keep the series alive for quick re-enable)
    if (!doSmooth) {
        if (col.smooth) col.smooth->setVisible(false);
        if (col.smoothScatter) col.smoothScatter->setVisible(false);
        if (col.custom && col.fit) col.fit->setVisible(false);
    }
    col.doRaw    = doRaw;
    col.doSmooth = doSmooth;
    col.window   = window;
    col.order    = order;
}

// Replace a column's raw series with a full point list (and optional error
// bars) and recompute its cached bounds, WITHOUT redrawing (for loading
// non-active columns).
void setColumnData(ChartColumn &col, const QList<QPointF> &points, const QList<double> &yerr,
                   const QList<double> &yerrLo)
{
    col.series->replace(points); // drops any previous error bars
    if (yerr.size() == points.size()) {
        col.series->yerr = yerr;
        if (yerrLo.size() == points.size()) col.series->yerrLo = yerrLo;
    }
    col.lastX   = points.isEmpty() ? -1.0 : points.last().x();
    col.rawXmin = col.rawYmin = 1.0e100;
    col.rawXmax = col.rawYmax = -1.0e100;
    for (int i = 0; i < points.size(); ++i) {
        const QPointF &p = points[i];
        // the bars have to fit inside the plot, so the bounds cover the bar ends
        col.rawXmin = qMin(col.rawXmin, p.x());
        col.rawXmax = qMax(col.rawXmax, p.x());
        col.rawYmin = qMin(col.rawYmin, p.y() - col.series->errLow(i));
        col.rawYmax = qMax(col.rawYmax, p.y() + col.series->errHigh(i));
    }
}

} // namespace

/* -------------------------------------------------------------------- */

ChartViewer::ChartViewer(QWidget *parent) :
    QWidget(parent), plot(nullptr), updChart(Cfg::CHART_UPDATE_INTERVAL_DEFAULT), col(nullptr)
{
    plot = new PlotWidget(this);
    plot->setXTitle("Time step");
    plot->setXLabelFormat("%d");

    QSettings settings;
    // cache the live-update throttle interval once; re-reading it per appended
    // point would construct a QSettings object on the hot thermo path
    updChart = settings.value(Keys::UPDCHART, Cfg::CHART_UPDATE_INTERVAL_DEFAULT).toInt();
    settings.beginGroup(Keys::GROUP_CHARTS);
    plot->setGrid(settings.value(Keys::GRID, true).toBool(),
                  settings.value(Keys::MINORGRID, true).toBool());
    settings.endGroup();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(plot);
}

/* -------------------------------------------------------------------- */

ChartViewer::~ChartViewer() = default;

/* -------------------------------------------------------------------- */

void ChartViewer::setColumn(ChartColumn *c)
{
    plot->clearSeries();
    col = c;
    if (!col) {
        plot->update();
        return;
    }
    // re-register the column's persistent overlays/fit (they keep their styling);
    // the raw/smoothed series are (re)created and styled by refreshColumn, and the
    // reference lines are (re)applied by ChartWindow after binding.
    if (col->fit) plot->addSeries(col->fit.get());
    for (auto &s : col->overlaySeries)
        plot->addSeries(s.get());
    refreshColumn(plot, *col);
    plot->setYTitle(col->yTitle);
    resetColumnZoom(plot, *col);
}

/* -------------------------------------------------------------------- */

void ChartViewer::addPoint(double x, double y)
{
    if (appendColumnPoint(*col, x, y)) {
        // update the chart display only after at least updChart milliseconds have passed
        // a monotonic clock, so a run that crosses midnight keeps refreshing
        if (col->lastUpdate.elapsed() > updChart) {
            col->lastUpdate.restart();
            refreshColumn(plot, *col);
            resetColumnZoom(plot, *col);
        }
    }
}

/* -------------------------------------------------------------------- */

void ChartViewer::setXAxisRange(double min, double max)
{
    plot->setXRange(min, max);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setYAxisRange(double min, double max)
{
    plot->setYRange(min, max);
}

/* -------------------------------------------------------------------- */

QString ChartViewer::getName() const
{
    return col->series->name;
}

/* -------------------------------------------------------------------- */

QString ChartViewer::getXLabel() const
{
    return plot->xTitle();
}

/* -------------------------------------------------------------------- */

QString ChartViewer::getYLabel() const
{
    return plot->yTitle();
}

/* -------------------------------------------------------------------- */

QRectF ChartViewer::getMinMax() const
{
    return columnMinMax(*col);
}

/* -------------------------------------------------------------------- */

void ChartViewer::resetZoom()
{
    resetColumnZoom(plot, *col);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setTLabel(const QString &tlabel)
{
    plot->setTitle(tlabel);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setYLabel(const QString &ylabel)
{
    plot->setYTitle(ylabel);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setXLabel(const QString &xlabel)
{
    plot->setXTitle(xlabel);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setXLabelFormat(const QString &fmt)
{
    plot->setXLabelFormat(fmt);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setDisplayStyle(ChartDisplayMode mode, const QColor &color, qreal width,
                                  qreal pointSize)
{
    setColumnDisplayStyle(plot, *col, mode, color, width, pointSize);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setSmoothStyle(ChartDisplayMode mode, const QColor &color, qreal width,
                                 qreal pointSize)
{
    setColumnSmoothStyle(plot, *col, mode, color, width, pointSize);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setErrorStyle(const QColor &color, qreal width)
{
    setColumnErrorStyle(plot, *col, color, width);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setFitCurve(const QList<QPointF> &points, const QString &name)
{
    setColumnFitCurve(plot, *col, points, name);
}

/* -------------------------------------------------------------------- */

void ChartViewer::clearFitCurve()
{
    clearColumnFitCurve(plot, *col);
}

/* -------------------------------------------------------------------- */

void ChartViewer::addOverlaySeries(const QList<QPointF> &pts, const QString &name,
                                   const QColor &color, const QList<double> &yerr,
                                   const QList<double> &yerrLo)
{
    addColumnOverlay(plot, *col, pts, name, color, yerr, yerrLo);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setReferenceLines(const QList<RefLine> &lines)
{
    setColumnReferenceLines(plot, *col, lines);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setLegendPos(LegendPos pos)
{
    plot->setLegendPos(pos);
}

/* -------------------------------------------------------------------- */

void ChartViewer::setRefLabelStyle(double pointSize, double distance, bool boxed)
{
    plot->setRefLabelStyle(pointSize, distance, boxed);
}

/* -------------------------------------------------------------------- */

void ChartViewer::updateSmooth()
{
    refreshColumn(plot, *col);
}

// Local Variables:
// c-basic-offset: 4
// End:
