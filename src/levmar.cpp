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

#include "levmar.h"

#include "leastsquares.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

// sum of squared residuals (the cost being minimized)
double sum_squares(const std::vector<double> &r)
{
    double s = 0.0;
    for (const double v : r)
        s += v * v;
    return s;
}

} // namespace

LevmarResult levmarFit(int numResiduals, int numParams, const std::vector<double> &initial,
                       const LevmarModel &model, int maxIterations, double tolerance)
{
    const int m = numResiduals;
    const int n = numParams;

    LevmarResult res;
    if (n <= 0 || m < n || static_cast<int>(initial.size()) != n) {
        res.message = "invalid problem dimensions";
        return res;
    }

    std::vector<double> params = initial;
    std::vector<double> r(m, 0.0);
    std::vector<std::vector<double>> jac(m, std::vector<double>(n, 0.0));

    if (!model(params, r, jac)) {
        res.message = "initial model evaluation failed";
        return res;
    }
    double cost = sum_squares(r);

    double lambda                = 1.0e-3;
    constexpr double LAMBDA_UP   = 10.0;
    constexpr double LAMBDA_DOWN = 0.1;
    constexpr double LAMBDA_MAX  = 1.0e12;
    // floor for the Marquardt diagonal scaling so a parameter with (near) zero
    // curvature still contributes a positive damping term
    constexpr double DIAG_FLOOR = 1.0e-12;

    int iter       = 0;
    bool converged = false;
    for (; iter < maxIterations; ++iter) {
        // normal-equation building blocks: JtJ (n x n) and Jtr (n)
        float_mat JtJ(n, n, 0.0);
        std::vector<double> Jtr(n, 0.0);
        for (int j = 0; j < n; ++j) {
            for (int k = 0; k < n; ++k) {
                double s = 0.0;
                for (int i = 0; i < m; ++i)
                    s += jac[i][j] * jac[i][k];
                JtJ[j][k] = s;
            }
            double s = 0.0;
            for (int i = 0; i < m; ++i)
                s += jac[i][j] * r[i];
            Jtr[j] = s;
        }

        bool accepted = false;
        for (int tries = 0; tries < 30 && lambda <= LAMBDA_MAX; ++tries) {
            // damped system (JtJ + lambda*diag(JtJ)) delta = -Jtr
            float_mat A(n, n, 0.0);
            float_mat b(n, 1, 0.0);
            for (int j = 0; j < n; ++j) {
                for (int k = 0; k < n; ++k)
                    A[j][k] = JtJ[j][k];
                A[j][j] += lambda * std::max(JtJ[j][j], DIAG_FLOOR);
                b[j][0] = -Jtr[j];
            }
            const float_mat delta = lin_solve(A, b);

            // a singular/ill-conditioned solve can yield non-finite steps; treat
            // those like a rejected step and increase the damping
            bool finite = true;
            std::vector<double> trial(n, 0.0);
            for (int j = 0; j < n; ++j) {
                trial[j] = params[j] + delta[j][0];
                if (!std::isfinite(delta[j][0])) finite = false;
            }

            if (finite) {
                std::vector<double> rtrial(m, 0.0);
                std::vector<std::vector<double>> jtrial(m, std::vector<double>(n, 0.0));
                if (model(trial, rtrial, jtrial)) {
                    const double trialCost = sum_squares(rtrial);
                    if (trialCost < cost) {
                        const double rel = (cost - trialCost) / (cost > 0.0 ? cost : 1.0);
                        params           = std::move(trial);
                        r                = std::move(rtrial);
                        jac              = std::move(jtrial);
                        cost             = trialCost;
                        lambda *= LAMBDA_DOWN;
                        accepted = true;
                        if (rel < tolerance) converged = true;
                        break;
                    }
                }
            }
            lambda *= LAMBDA_UP;
        }

        if (!accepted) {
            res.message = "converged (no further reduction possible)";
            break;
        }
        if (converged) {
            res.message = "converged";
            break;
        }
    }
    if (iter >= maxIterations) res.message = "reached maximum iterations";

    res.params     = std::move(params);
    res.rms        = std::sqrt(cost / static_cast<double>(m));
    res.iterations = iter;
    res.ok         = true;
    return res;
}

// Local Variables:
// c-basic-offset: 4
// End:
