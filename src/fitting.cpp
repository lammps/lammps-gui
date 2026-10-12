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

#include "fitting.h"

#include "leastsquares.h"

#include <cmath>

namespace {

// Solve the normal equations (A^T A) c = A^T y for the coefficient vector c
// using the dense LU solver from the leastsquares toolkit.
std::vector<double> solveNormalEquations(const float_mat &A, const std::vector<double> &y)
{
    float_mat Y(y.size(), 1);
    for (std::size_t i = 0; i < y.size(); ++i)
        Y[i][0] = y[i];

    const float_mat At = transpose(A);
    const float_mat c  = lin_solve(At * A, At * Y);

    std::vector<double> coeffs(c.nr_rows());
    for (std::size_t i = 0; i < c.nr_rows(); ++i)
        coeffs[i] = c[i][0];
    return coeffs;
}

} // namespace

double evalPolynomial(const std::vector<double> &coeffs, double x)
{
    double value = 0.0;
    double xpow  = 1.0;
    for (double cf : coeffs) {
        value += cf * xpow;
        xpow *= x;
    }
    return value;
}

PolynomialFit polynomialFit(const std::vector<double> &x, const std::vector<double> &y, int degree)
{
    PolynomialFit result;
    const int n = static_cast<int>(x.size());
    if ((static_cast<int>(y.size()) != n) || (degree < 0) || (n < degree + 1)) return result;

    const int p = degree + 1;
    float_mat A(n, p);
    for (int i = 0; i < n; ++i) {
        double xpow = 1.0;
        for (int j = 0; j < p; ++j) {
            A[i][j] = xpow;
            xpow *= x[i];
        }
    }

    result.coeffs = solveNormalEquations(A, y);

    double sumsq = 0.0;
    for (int i = 0; i < n; ++i) {
        const double resid = y[i] - evalPolynomial(result.coeffs, x[i]);
        sumsq += resid * resid;
    }
    result.rms = std::sqrt(sumsq / static_cast<double>(n));
    result.ok  = true;
    return result;
}

double evalBirchMurnaghan(const EosFit &fit, double v)
{
    const double u = std::pow(v, -2.0 / 3.0);
    return fit.a + fit.b * u + fit.c * u * u + fit.d * u * u * u;
}

EosFit birchMurnaghanFit(const std::vector<double> &v, const std::vector<double> &e)
{
    EosFit result;
    const int n = static_cast<int>(v.size());
    if ((static_cast<int>(e.size()) != n) || (n < 4)) return result;
    for (double vv : v)
        if (vv <= 0.0) return result;

    // design matrix columns: 1, V^(-2/3), V^(-4/3), V^(-2)
    float_mat A(n, 4);
    for (int i = 0; i < n; ++i) {
        const double u = std::pow(v[i], -2.0 / 3.0);
        A[i][0]        = 1.0;
        A[i][1]        = u;
        A[i][2]        = u * u;
        A[i][3]        = u * u * u;
    }

    const std::vector<double> co = solveNormalEquations(A, e);
    result.a                     = co[0];
    result.b                     = co[1];
    result.c                     = co[2];
    result.d                     = co[3];

    // equilibrium volume: dE/dV = 0 reduces to 3 d u^2 + 2 c u + b = 0 with
    // u = V^(-2/3); choose the positive root that is a minimum (E''(V0) > 0).
    std::vector<double> roots;
    if (std::fabs(result.d) < 1.0e-14) {
        if (std::fabs(result.c) > 0.0) roots.push_back(-result.b / (2.0 * result.c));
    } else {
        const double disc = 4.0 * result.c * result.c - 12.0 * result.d * result.b;
        if (disc >= 0.0) {
            const double sq = std::sqrt(disc);
            roots.push_back((-2.0 * result.c + sq) / (6.0 * result.d));
            roots.push_back((-2.0 * result.c - sq) / (6.0 * result.d));
        }
    }

    bool found = false;
    for (double u : roots) {
        if (u <= 0.0) continue;
        const double V0 = std::pow(u, -1.5); // u = V^(-2/3) -> V = u^(-3/2)
        // analytic E''(V) and E'''(V) of E(V) = a + b V^-2/3 + c V^-4/3 + d V^-2
        const double epp = (10.0 / 9.0) * result.b * std::pow(V0, -8.0 / 3.0) +
                           (28.0 / 9.0) * result.c * std::pow(V0, -10.0 / 3.0) +
                           6.0 * result.d * std::pow(V0, -4.0);
        if (epp <= 0.0) continue; // not a minimum
        const double eppp = -(80.0 / 27.0) * result.b * std::pow(V0, -11.0 / 3.0) -
                            (280.0 / 27.0) * result.c * std::pow(V0, -13.0 / 3.0) -
                            24.0 * result.d * std::pow(V0, -5.0);
        result.v0      = V0;
        result.e0      = result.a + result.b * u + result.c * u * u + result.d * u * u * u;
        result.b0      = V0 * epp;
        result.b0prime = -1.0 - V0 * eppp / epp;
        found          = true;
        break;
    }
    if (!found) return result; // ok stays false

    double sumsq = 0.0;
    for (int i = 0; i < n; ++i) {
        const double resid = e[i] - evalBirchMurnaghan(result, v[i]);
        sumsq += resid * resid;
    }
    result.rms = std::sqrt(sumsq / static_cast<double>(n));
    result.ok  = true;
    return result;
}

// Local Variables:
// c-basic-offset: 4
// End:
