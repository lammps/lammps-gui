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

#include "analysis.h"

#include <cmath>
#include <cstddef>

std::vector<double> autocorrelation(const std::vector<double> &y, int maxlag)
{
    const int n = static_cast<int>(y.size());
    if (n < 2) return {};

    if ((maxlag <= 0) || (maxlag >= n)) maxlag = n - 1;

    // mean
    double mean = 0.0;
    for (double v : y)
        mean += v;
    mean /= static_cast<double>(n);

    // total variance (denominator); zero for a constant series
    double denom = 0.0;
    for (double v : y) {
        const double d = v - mean;
        denom += d * d;
    }
    if (denom <= 0.0) return {};

    std::vector<double> acf(maxlag + 1, 0.0);
    for (int k = 0; k <= maxlag; ++k) {
        double num = 0.0;
        for (int i = 0; i + k < n; ++i)
            num += (y[i] - mean) * (y[i + k] - mean);
        acf[k] = num / denom;
    }
    return acf;
}

// The taper factor of sample i: 1 at the first sample, 0 at the last for the
// Hann case.  A one-sided decaying series keeps its full weight at the origin.
static double taper(FourierWindow window, const std::vector<double> &x, std::size_t i)
{
    if (window != FourierWindow::Hann) return 1.0;
    const double span = x.back() - x.front();
    if (span <= 0.0) return 1.0;
    return 0.5 * (1.0 + std::cos(MY_PI_CONST * (x[i] - x.front()) / span));
}

std::vector<double> fourierTransform(const std::vector<double> &x, const std::vector<double> &y,
                                     const std::vector<double> &k, FourierKind kind,
                                     FourierWindow window)
{
    const std::size_t n = x.size();
    if ((n < 2) || (y.size() != n)) return {};

    std::vector<double> result;
    result.reserve(k.size());
    for (double kval : k) {
        // trapezoidal rule for the cosine and sine integrals; the power
        // spectrum needs both either way
        double csum = 0.0, ssum = 0.0;
        double cprev = taper(window, x, 0) * y[0] * std::cos(kval * x[0]);
        double sprev = taper(window, x, 0) * y[0] * std::sin(kval * x[0]);
        for (std::size_t i = 1; i < n; ++i) {
            const double w    = taper(window, x, i) * y[i];
            const double ccur = w * std::cos(kval * x[i]);
            const double scur = w * std::sin(kval * x[i]);
            const double dx   = x[i] - x[i - 1];
            csum += 0.5 * (cprev + ccur) * dx;
            ssum += 0.5 * (sprev + scur) * dx;
            cprev = ccur;
            sprev = scur;
        }
        switch (kind) {
            case FourierKind::Cosine:
                result.push_back(2.0 * csum);
                break;
            case FourierKind::Sine:
                result.push_back(2.0 * ssum);
                break;
            case FourierKind::Power:
                result.push_back(csum * csum + ssum * ssum);
                break;
        }
    }
    return result;
}

std::vector<double> structureFactor(const std::vector<double> &r, const std::vector<double> &g,
                                    double rho, const std::vector<double> &q, FourierWindow window)
{
    const std::size_t n = r.size();
    if ((n < 2) || (g.size() != n)) return {};

    std::vector<double> result;
    result.reserve(q.size());
    for (double qval : q) {
        // sin(qr)/(qr) written to be regular at qr = 0, where it is 1
        auto sinc = [qval](double rv) {
            const double a = qval * rv;
            return (std::abs(a) < 1.0e-8) ? 1.0 : std::sin(a) / a;
        };
        double sum  = 0.0;
        double prev = taper(window, r, 0) * r[0] * r[0] * (g[0] - 1.0) * sinc(r[0]);
        for (std::size_t i = 1; i < n; ++i) {
            const double cur = taper(window, r, i) * r[i] * r[i] * (g[i] - 1.0) * sinc(r[i]);
            sum += 0.5 * (prev + cur) * (r[i] - r[i - 1]);
            prev = cur;
        }
        result.push_back(1.0 + 4.0 * MY_PI_CONST * rho * sum);
    }
    return result;
}

// Local Variables:
// c-basic-offset: 4
// End:
