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

#ifndef FITTING_H
#define FITTING_H

// Linear-in-parameters least-squares curve fits, built on the (Qt-free)
// leastsquares LU solver. Pure functions on std::vector<double> so they can
// be unit-tested without a GUI and reused by the chart post-processing dialog.

#include <vector>

/**
 * @brief Result of a least-squares polynomial fit
 */
struct PolynomialFit {
    std::vector<double> coeffs; ///< coefficients c0..cn, i.e. y = sum_k c_k x^k
    double rms = 0.0;           ///< root-mean-square residual
    bool ok    = false;         ///< true if the fit succeeded
};

/**
 * @brief Least-squares polynomial fit of y(x)
 * @param x      Abscissa values
 * @param y      Ordinate values (same length as @p x)
 * @param degree Polynomial degree (>= 0)
 * @return Fit result; ok is false if the sizes mismatch or there are fewer
 *         than degree+1 points
 */
PolynomialFit polynomialFit(const std::vector<double> &x, const std::vector<double> &y, int degree);

/**
 * @brief Evaluate a polynomial at a point
 * @param coeffs Coefficients c0..cn (y = sum_k c_k x^k)
 * @param x      Evaluation point
 * @return Polynomial value
 */
double evalPolynomial(const std::vector<double> &coeffs, double x);

/**
 * @brief Result of a 4-parameter Birch-Murnaghan equation-of-state fit
 *
 * The fitted model is the linear-in-coefficients form
 * @f$ E(V) = a + b\,V^{-2/3} + c\,V^{-4/3} + d\,V^{-2} @f$, from which the
 * physical quantities are derived.
 */
struct EosFit {
    double a       = 0.0;   ///< constant coefficient
    double b       = 0.0;   ///< coefficient of V^(-2/3)
    double c       = 0.0;   ///< coefficient of V^(-4/3)
    double d       = 0.0;   ///< coefficient of V^(-2)
    double v0      = 0.0;   ///< equilibrium volume
    double e0      = 0.0;   ///< equilibrium energy
    double b0      = 0.0;   ///< bulk modulus (in energy/volume units)
    double b0prime = 0.0;   ///< pressure derivative of the bulk modulus
    double rms     = 0.0;   ///< root-mean-square residual
    bool ok        = false; ///< true if the fit succeeded and a minimum was found
};

/**
 * @brief 4-parameter Birch-Murnaghan EOS fit of energy versus volume
 * @param v Volumes (must be positive)
 * @param e Energies (same length as @p v)
 * @return Fit result; ok is false if the sizes mismatch, there are fewer than
 *         four points, a volume is non-positive, or no physical minimum exists
 */
EosFit birchMurnaghanFit(const std::vector<double> &v, const std::vector<double> &e);

/**
 * @brief Evaluate the fitted Birch-Murnaghan energy at a volume
 * @param fit Fit result (uses its a/b/c/d coefficients)
 * @param v   Volume
 * @return Energy E(V)
 */
double evalBirchMurnaghan(const EosFit &fit, double v);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
