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

#ifndef COLORMAPS_H
#define COLORMAPS_H

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief A single color stop of a dump-image color map
 *
 * A stop is either a LAMMPS named color (@ref name non-empty, used verbatim in
 * the generated command and resolved with QColor for the preview) or an explicit
 * RGB color (@ref name empty, @ref r / @ref g / @ref b used).  Storing the RGB as
 * floats keeps the generated command identical to the values LAMMPS renders and
 * lets the preview use the very same numbers via QColor::fromRgbF().  @ref pos is
 * the stop position in [0,1] and is only meaningful for continuous maps (the
 * first stop maps to the `min` value, the last to `max`).
 */
struct ColorMapStop {
    double pos;   ///< position in [0,1] (continuous maps); 0 = min, 1 = max
    QString name; ///< LAMMPS named color, or empty when an explicit RGB is given
    double r;     ///< explicit red in [0,1], used when @ref name is empty
    double g;     ///< explicit green in [0,1], used when @ref name is empty
    double b;     ///< explicit blue in [0,1], used when @ref name is empty
};

/**
 * @brief Definition of a named dump-image color map
 *
 * This is the single source of truth shared by the command builder
 * (`appendColorMapArgs()` in dumpimage.cpp, which emits `dump_modify amap/bmap`)
 * and the settings-dialog preview swatches (`addColorMapItems()` in
 * imageviewersettings.cpp), so the swatch a user picks matches what LAMMPS
 * renders.
 */
struct ColorMapDef {
    bool continuous;           ///< true: interpolated (`cf`); false: discrete sequence (`sa`)
    QList<ColorMapStop> stops; ///< ordered color stops
};

/**
 * @brief Look up a color-map definition by name
 * @param name color-map name (e.g. "Viridis"); unknown names fall back to "BWR"
 * @return reference to the (statically stored) definition
 */
const ColorMapDef &colorMapDef(const QString &name);

/**
 * @brief The selectable color-map names, in display order
 * @return reference to the (statically stored) ordered name list
 */
const QStringList &colorMapNames();

#endif

// Local Variables:
// c-basic-offset: 4
// End:
