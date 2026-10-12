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

#ifndef PLOTAXISMATH_H
#define PLOTAXISMATH_H

// Small, self-contained (Qt-free) helpers for laying out a linear value axis:
// "nice" tick spacing, the set of major tick values for a range, and printf-style
// tick-label formatting.  Pure functions so they can be unit-tested without a GUI
// and reused by the native QPainter chart renderer.

#include <string>
#include <vector>

namespace PlotAxisMath {

/**
 * @brief "Nice" tick spacing (1, 2, 5 times a power of ten) for a value range
 * @param range          Span of the axis (max - min); values <= 0 or non-finite
 *                       yield a spacing of 1
 * @param targetIntervals Desired number of gaps between ticks (clamped to >= 1);
 *                       the result targets roughly this many intervals
 * @return A tick spacing of the form {1,2,5} * 10^n closest to range/targetIntervals
 */
double niceTickInterval(double range, int targetIntervals = 4);

/**
 * @brief Major tick values across an axis range, aligned to an anchor
 * @param min      Lower end of the axis range
 * @param max      Upper end of the axis range (swapped with min if min > max)
 * @param interval Tick spacing; values <= 0 or non-finite yield an empty list
 * @param anchor   Value the tick grid is aligned to (default 0, i.e. ticks fall
 *                 on integer multiples of @p interval)
 * @return Ascending tick values t = anchor + k*interval that lie within
 *         [min, max] (inclusive within a small tolerance); values within the
 *         tolerance of zero are snapped to exactly 0
 */
std::vector<double> tickValues(double min, double max, double interval, double anchor = 0.0);

/**
 * @brief Format a single axis value with a printf-style format string
 * @param value  The numeric value to format
 * @param format printf-style format (e.g. "%d", "%.6g", "%.3f"); an empty
 *               string defaults to "%g"
 * @return The formatted label
 *
 * Follows the usual printf conventions: an integer conversion
 * specifier (d, i, o, u, x, X, c) formats the value as an integer (rounded);
 * a floating specifier (e, E, f, F, g, G, a, A) formats it as a double. Any
 * length modifier in the format is normalized so passing, e.g., "%d" is safe.
 * A format without a usable conversion specifier is returned unchanged.
 */
std::string formatAxisLabel(double value, const std::string &format);

/**
 * @brief Decimal places needed to distinguish ticks at a given spacing
 * @param interval Tick spacing (e.g. from niceTickInterval)
 * @return Number of decimals so that adjacent ticks differ in their last shown
 *         digit: 0 for integer-or-larger spacings, more as the spacing shrinks
 *         (e.g. 0.5 -> 1, 0.05 -> 2). Capped at 12; 0 for non-positive input.
 *
 * Lets a renderer pick a `"%.Nf"` format from the tick spacing so that, e.g.,
 * closely spaced values do not collapse to identical labels.
 */
int tickDecimals(double interval);

} // namespace PlotAxisMath

#endif

// Local Variables:
// c-basic-offset: 4
// End:
