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

#include "colormaps.h"

#include <map>

// Single source of truth for the dump-image color maps.  Continuous maps list
// their stops with explicit positions (the first is the `min` value, the last
// `max`); discrete maps list colors only (position is ignored).  Explicit RGB
// stops are given as the same 0..1 floats LAMMPS renders, so the generated
// command and the dialog preview cannot drift apart.

namespace {

// a LAMMPS named-color stop
ColorMapStop nm(double pos, const char *name)
{
    return {pos, QString::fromLatin1(name), 0.0, 0.0, 0.0};
}

// an explicit RGB stop (components in [0,1])
ColorMapStop rc(double pos, double r, double g, double b)
{
    return {pos, QString(), r, g, b};
}

const std::map<QString, ColorMapDef> &table()
{
    // clang-format off
    static const std::map<QString, ColorMapDef> maps = {
        {"BWR", {true, {rc(0.0, 0.000, 0.227, 0.427), nm(0.5, "white"),
                        rc(1.0, 0.459, 0.055, 0.075)}}},
        {"RWB", {true, {rc(0.0, 0.459, 0.055, 0.075), rc(0.1, 0.459, 0.055, 0.075),
                        nm(0.5, "white"),
                        rc(0.9, 0.000, 0.227, 0.427), rc(1.0, 0.000, 0.227, 0.427)}}},
        {"PWT", {true, {rc(0.0, 0.286, 0.114, 0.553), rc(0.1, 0.286, 0.114, 0.553),
                        nm(0.5, "white"),
                        rc(0.9, 0.000, 0.255, 0.267), rc(1.0, 0.000, 0.255, 0.267)}}},
        {"BWG", {true, {nm(0.0, "blue"), nm(0.1, "blue"), nm(0.5, "white"),
                        nm(0.9, "green"), nm(1.0, "green")}}},
        {"BGR", {true, {nm(0.0, "blue"), nm(0.05, "blue"), nm(0.5, "green"),
                        nm(0.95, "red"), nm(1.0, "red")}}},
        {"Grayscale", {true, {nm(0.0, "black"), nm(1.0, "white")}}},
        {"Viridis", {true, {rc(0.0,   0.267, 0.005, 0.329), rc(0.333, 0.191, 0.407, 0.556),
                            rc(0.667, 0.208, 0.719, 0.473), rc(1.0,   0.993, 0.906, 0.144)}}},
        {"Plasma", {true, {rc(0.0,   0.050, 0.030, 0.528), rc(0.333, 0.611, 0.090, 0.620),
                           rc(0.667, 0.928, 0.473, 0.326), rc(1.0,   0.940, 0.975, 0.131)}}},
        {"Inferno", {true, {rc(0.0,  0.001, 0.000, 0.014), rc(0.25, 0.342, 0.062, 0.429),
                            rc(0.5,  0.736, 0.216, 0.330), rc(0.75, 0.978, 0.558, 0.035),
                            rc(1.0,  0.988, 0.998, 0.645)}}},
        {"Magma", {true, {rc(0.0,  0.001, 0.000, 0.014), rc(0.25, 0.317, 0.072, 0.485),
                          rc(0.5,  0.716, 0.215, 0.475), rc(0.75, 0.987, 0.536, 0.382),
                          rc(1.0,  0.987, 0.991, 0.750)}}},
        {"Cividis", {true, {rc(0.0,  0.000, 0.135, 0.305), rc(0.25, 0.264, 0.308, 0.423),
                            rc(0.5,  0.489, 0.485, 0.471), rc(0.75, 0.736, 0.681, 0.424),
                            rc(1.0,  0.996, 0.909, 0.218)}}},
        {"Turbo", {true, {rc(0.0, 0.190, 0.072, 0.232), rc(0.2, 0.244, 0.609, 0.997),
                          rc(0.4, 0.276, 0.971, 0.517), rc(0.6, 0.883, 0.866, 0.217),
                          rc(0.8, 0.941, 0.356, 0.070), rc(1.0, 0.480, 0.016, 0.011)}}},
        {"Teal", {true, {rc(0.0, 0.071, 0.153, 0.251), rc(0.25, 0.106, 0.282, 0.369),
                         rc(0.5, 0.337, 0.545, 0.529), rc(1.0,  0.710, 0.820, 0.682)}}},
        {"Rainbow", {true, {nm(0.0, "red"), nm(0.25, "yellow"), nm(0.45, "green"),
                            nm(0.65, "cyan"), nm(0.85, "blue"), nm(1.0, "purple")}}},
        {"Sequential", {false, {rc(0.0, 0.808, 0.808, 0.808), rc(0.0, 0.647, 0.349, 0.667),
                                rc(0.0, 0.349, 0.659, 0.612), rc(0.0, 0.941, 0.772, 0.443),
                                rc(0.0, 0.878, 0.169, 0.208), rc(0.0, 0.031, 0.165, 0.329)}}},
        {"Landscape", {false, {rc(0.0, 0.145, 0.400, 0.463), rc(0.0, 0.392, 0.867, 0.588),
                               rc(0.0, 0.572, 0.192, 0.141), rc(0.0, 0.392, 0.831, 0.992),
                               rc(0.0, 0.020, 0.431, 0.071), rc(0.0, 0.992, 0.349, 0.145),
                               rc(0.0, 0.275, 0.953, 0.243), rc(0.0, 0.729, 0.525, 0.361),
                               rc(0.0, 0.780, 0.867, 0.529), rc(0.0, 0.243, 0.298, 0.078)}}},
        {"Basic", {false, {nm(0.0, "red"), nm(0.0, "cyan"), nm(0.0, "green"), nm(0.0, "black"),
                           nm(0.0, "magenta"), nm(0.0, "blue"), nm(0.0, "yellow"),
                           nm(0.0, "purple"), nm(0.0, "white"), nm(0.0, "orange")}}},
    };
    // clang-format on
    return maps;
}

} // namespace

const ColorMapDef &colorMapDef(const QString &name)
{
    const auto &maps = table();
    const auto it    = maps.find(name);
    if (it != maps.end()) return it->second;
    return maps.at(QStringLiteral("BWR"));
}

const QStringList &colorMapNames()
{
    // RWB is intentionally not offered: it is exactly BWR with the scale
    // reversed, so the dialog's Reverse option reproduces it.  It is still
    // present in table() above so any stale RWB selection still renders
    // correctly.
    static const QStringList names = {
        "BWR",   "PWT",     "BWG",   "BGR",  "Grayscale", "Viridis",    "Plasma",    "Inferno",
        "Magma", "Cividis", "Turbo", "Teal", "Rainbow",   "Sequential", "Landscape", "Basic"};
    return names;
}

// Local Variables:
// c-basic-offset: 4
// End:
