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

#include "chartstyle.h"

#include "chartviewer.h"
#include "constants.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QList>
#include <QSettings>

namespace {

struct PaletteEntry {
    const char *name; ///< what the Preferences dialog shows
    QColor color;     ///< what a chart draws with
};

const QList<PaletteEntry> &palette()
{
    // a configured index refers to this order, so entries are only appended
    static const QList<PaletteEntry> entries = {
        {"Black", QColor(0, 0, 0)},        // 0
        {"Blue", QColor(100, 150, 255)},   // 1
        {"Red", QColor(255, 125, 125)},    // 2
        {"Green", QColor(100, 200, 100)},  // 3
        {"Gray", QColor(120, 120, 120)},   // 4
        {"Orange", QColor(255, 160, 40)},  // 5
        {"Purple", QColor(160, 80, 220)},  // 6
        {"Teal", QColor(0, 160, 180)},     // 7
        {"Magenta", QColor(220, 80, 180)}, // 8
        {"Brown", QColor(150, 100, 50)},   // 9
    };
    return entries;
}

} // namespace

QColor chartPaletteColor(int index)
{
    const auto &entries = palette();
    if ((index < 0) || (index >= entries.size())) index = 0;
    return entries[index].color;
}

QColor configuredChartColor(const QString &key, int fallback)
{
    QSettings settings;
    settings.beginGroup(Keys::GROUP_CHARTS);
    const int index = settings.value(key, fallback).toInt();
    settings.endGroup();
    return chartPaletteColor(index);
}

QComboBox *makeChartColorCombo(int current)
{
    auto *combo = new QComboBox;
    for (const auto &entry : palette())
        combo->addItem(entry.name);
    combo->setCurrentIndex(current);
    return combo;
}

QComboBox *makeChartModeCombo(int current)
{
    auto *combo = new QComboBox;
    combo->addItem("Lines", static_cast<int>(ChartDisplayMode::Lines));
    combo->addItem("Points", static_cast<int>(ChartDisplayMode::Points));
    combo->addItem("Lines + Points", static_cast<int>(ChartDisplayMode::LinesAndPoints));
    combo->setCurrentIndex(current);
    return combo;
}

QComboBox *makePlotChoiceCombo(int current)
{
    auto *combo = new QComboBox;
    combo->addItem("Raw");
    combo->addItem("Smooth");
    combo->addItem("Both");
    combo->setCurrentIndex(current);
    return combo;
}

QDoubleSpinBox *makeLineWidthSpin(double value)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(Cfg::LINE_WIDTH_MIN, Cfg::LINE_WIDTH_MAX);
    spin->setSingleStep(0.5);
    spin->setValue(value);
    return spin;
}

QDoubleSpinBox *makePointSizeSpin(double value)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(Cfg::POINT_SIZE_MIN, Cfg::POINT_SIZE_MAX);
    spin->setSingleStep(1.0);
    spin->setValue(value);
    return spin;
}

// Local Variables:
// c-basic-offset: 4
// End:
