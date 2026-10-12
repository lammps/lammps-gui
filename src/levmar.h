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

#ifndef LEVMAR_H
#define LEVMAR_H

// Compact, self-contained (Qt-free) Levenberg-Marquardt solver for nonlinear
// least-squares fits. The model is supplied as a callback returning residuals
// and the (analytic) Jacobian, so the core is independent of how the model is
// expressed; the chart post-processing dialog drives it with LeptonMini
// expressions and their symbolic derivatives. The damped normal equations are
// solved with the shared leastsquares LU solver.

#include <functional>
#include <string>
#include <vector>

/**
 * @brief Result of a Levenberg-Marquardt nonlinear least-squares fit
 */
struct LevmarResult {
    std::vector<double> params; ///< fitted parameter values (empty on failure)
    double rms     = 0.0;       ///< root-mean-square residual at the solution
    int iterations = 0;         ///< number of outer iterations performed
    bool ok        = false;     ///< true if the fit ran to a usable solution
    std::string message;        ///< human-readable status/diagnostic
};

/**
 * @brief Callback evaluating residuals and the Jacobian at a parameter vector
 *
 * Given the current @p params (length n), it must fill @p residuals (length m)
 * with model(x_i) - y_i and @p jacobian (m rows of n columns) with
 * d model(x_i) / d p_j. Returning false signals an evaluation failure (e.g. a
 * domain error) for that parameter vector; the solver treats the trial step as
 * rejected rather than aborting.
 */
using LevmarModel =
    std::function<bool(const std::vector<double> &params, std::vector<double> &residuals,
                       std::vector<std::vector<double>> &jacobian)>;

/**
 * @brief Levenberg-Marquardt nonlinear least-squares minimization
 * @param numResiduals  Number of data points m (must be >= numParams)
 * @param numParams     Number of free parameters n (> 0)
 * @param initial       Initial parameter guess (length numParams)
 * @param model         Residual/Jacobian callback
 * @param maxIterations Maximum number of outer iterations
 * @param tolerance     Relative cost-change convergence threshold
 * @return Fit result; ok is false (with a message) on bad dimensions or a
 *         failed initial evaluation
 */
LevmarResult levmarFit(int numResiduals, int numParams, const std::vector<double> &initial,
                       const LevmarModel &model, int maxIterations = 200,
                       double tolerance = 1.0e-12);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
