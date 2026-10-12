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

#include "plotaxismath.h"

#include <cmath>
#include <cstdio>

namespace PlotAxisMath {

double niceTickInterval(double range, int targetIntervals)
{
    // guard against zero, negative, and non-finite ranges
    if (!(range > 0.0) || !std::isfinite(range)) return 1.0;
    if (targetIntervals < 1) targetIntervals = 1;

    const double rough = range / static_cast<double>(targetIntervals);
    const double p     = std::pow(10.0, std::floor(std::log10(rough)));
    const double frac  = rough / p;
    double nice;
    if (frac < 1.5)
        nice = 1.0;
    else if (frac < 3.0)
        nice = 2.0;
    else if (frac < 7.0)
        nice = 5.0;
    else
        nice = 10.0;
    return nice * p;
}

std::vector<double> tickValues(double min, double max, double interval, double anchor)
{
    std::vector<double> ticks;
    if (!(interval > 0.0) || !std::isfinite(interval)) return ticks;
    if (!std::isfinite(min) || !std::isfinite(max) || !std::isfinite(anchor)) return ticks;
    if (max < min) std::swap(min, max);

    const double eps = interval * 1.0e-9;
    // index of the first tick at or above min, aligned to anchor + k*interval
    const double kfirst = std::ceil((min - anchor) / interval - 1.0e-9);

    // cap the number of ticks to stay safe if interval is tiny relative to range
    constexpr int MAXTICKS = 100000;
    for (int count = 0; count < MAXTICKS; ++count) {
        const double t = anchor + (kfirst + count) * interval;
        if (t > max + eps) break;
        ticks.push_back(std::fabs(t) < eps ? 0.0 : t);
    }
    return ticks;
}

namespace {

// length modifiers that may appear in a printf conversion specification
bool isLengthModifier(char c)
{
    return c == 'l' || c == 'h' || c == 'L' || c == 'j' || c == 'z' || c == 't' || c == 'q';
}

} // namespace

std::string formatAxisLabel(double value, const std::string &format)
{
    const std::string fmt = format.empty() ? std::string("%g") : format;
    const std::size_t n   = fmt.size();

    // locate the first real conversion specifier (skip escaped "%%")
    std::size_t pct = std::string::npos;
    for (std::size_t i = 0; i < n;) {
        if (fmt[i] == '%') {
            if (i + 1 < n && fmt[i + 1] == '%') {
                i += 2;
                continue;
            }
            pct = i;
            break;
        }
        ++i;
    }
    // no numeric placeholder: return the format unchanged
    if (pct == std::string::npos) return fmt;

    // copy flags, width, and precision; then strip any length modifier
    std::size_t j          = pct + 1;
    const std::string head = fmt.substr(0, pct); // literal prefix (may contain "%%")
    std::string body       = "%";
    while (j < n && (fmt[j] == '-' || fmt[j] == '+' || fmt[j] == ' ' || fmt[j] == '#' ||
                     fmt[j] == '0' || (fmt[j] >= '0' && fmt[j] <= '9') || fmt[j] == '.')) {
        body.push_back(fmt[j]);
        ++j;
    }
    while (j < n && isLengthModifier(fmt[j]))
        ++j;
    if (j >= n) return fmt; // malformed: no conversion character

    const char conv        = fmt[j];
    const std::string tail = fmt.substr(j + 1); // literal suffix (may contain "%%")

    auto compose = [&](const char *lengthmod) {
        return head + body + lengthmod + std::string(1, conv) + tail;
    };

    char buf[256];
    switch (conv) {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X': {
            const std::string f = compose("ll");
            std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<long long>(std::llround(value)));
            return buf;
        }
        case 'c': {
            const std::string f = compose("");
            std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<int>(std::llround(value)));
            return buf;
        }
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            const std::string f = compose("");
            std::snprintf(buf, sizeof(buf), f.c_str(), value);
            return buf;
        }
        default:
            // unknown specifier: return the format unchanged
            return fmt;
    }
}

int tickDecimals(double interval)
{
    if (!(interval > 0.0) || !std::isfinite(interval)) return 0;
    // number of decimals so a step of `interval` changes the last shown digit;
    // the small epsilon keeps exact powers of ten (e.g. 0.1 -> 1) from rounding up
    int d = static_cast<int>(std::ceil(-std::log10(interval) - 1.0e-9));
    if (d < 0) d = 0;
    if (d > 12) d = 12;
    return d;
}

} // namespace PlotAxisMath

// Local Variables:
// c-basic-offset: 4
// End:
