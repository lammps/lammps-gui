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

#ifndef CHARTSTYLE_H
#define CHARTSTYLE_H

// The series style vocabulary shared by the per-chart "Chart Style" dialog and
// the Charts tab of the Preferences dialog: the palette that a configured color
// index refers to, and the widgets offering a display mode, a palette color, a
// plot data choice, a line width, or a marker size.  Both dialogs build from
// these, so their lists cannot drift apart.

#include <QColor>
#include <QString>

class QComboBox;
class QDoubleSpinBox;

/**
 * @brief The palette color for a configured series color index
 * @param index Index into the palette; the first entry when out of range
 * @return The color
 */
QColor chartPaletteColor(int index);

/**
 * @brief The configured default color of a series kind
 * @param key      Settings key of the color index in the charts group (e.g. `Keys::RAWBRUSH`)
 * @param fallback Index to use when the key is not set
 * @return The palette color for the configured index
 */
QColor configuredChartColor(const QString &key, int fallback);

/**
 * @brief Combo box offering the palette colors by name
 * @param current Index of the preselected color
 * @return The new widget, without a parent
 */
QComboBox *makeChartColorCombo(int current);

/**
 * @brief Combo box offering the display modes in ChartDisplayMode order
 *
 * Each item carries its mode as integer item data, so a choice can be read
 * back through `currentData()` as well as through `currentIndex()`.
 *
 * @param current Index of the preselected mode
 * @return The new widget, without a parent
 */
QComboBox *makeChartModeCombo(int current);

/**
 * @brief Combo box offering the plot data choices Raw, Smooth, and Both
 * @param current Index of the preselected choice
 * @return The new widget, without a parent
 */
QComboBox *makePlotChoiceCombo(int current);

/**
 * @brief Spin box for a series line width within the configured limits
 * @param value Preset width
 * @return The new widget, without a parent
 */
QDoubleSpinBox *makeLineWidthSpin(double value);

/**
 * @brief Spin box for a marker diameter within the configured limits
 * @param value Preset diameter
 * @return The new widget, without a parent
 */
QDoubleSpinBox *makePointSizeSpin(double value);

#endif
// Local Variables:
// c-basic-offset: 4
// End:
