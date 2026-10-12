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

#ifndef ANALYSIS_H
#define ANALYSIS_H

// Small, self-contained (Qt-free) post-processing analyses on a data series.
// Pure functions on std::vector<double> so they can be unit-tested without a
// GUI and reused by the chart post-processing dialog.

#include <vector>

/// Pi, spelled out: M_PI needs feature-test macros on some of the platforms
/// the packaging cross-compiles for
constexpr double MY_PI_CONST = 3.14159265358979323846;

/**
 * @brief Normalized autocorrelation function (ACF) of a data series
 * @param y      Input samples (assumed equally spaced)
 * @param maxlag Largest lag to compute; values <= 0 or >= y.size() are
 *               clamped to y.size()-1
 * @return ACF values for lags 0..maxlag (length maxlag+1), normalized so that
 *         the lag-0 value is 1; an empty vector if the input has fewer than
 *         two samples or zero variance (a constant series)
 *
 * Uses the standard biased estimator
 * @f$ \mathrm{ACF}(k) = \frac{\sum_{i=0}^{N-1-k}(y_i-\bar y)(y_{i+k}-\bar y)}
 * {\sum_{i=0}^{N-1}(y_i-\bar y)^2} @f$.
 */
std::vector<double> autocorrelation(const std::vector<double> &y, int maxlag);

/**
 * @brief Kind of Fourier transform computed by fourierTransform()
 */
enum class FourierKind {
    Cosine, ///< one-sided cosine transform 2 * integral y(x) cos(kx) dx
    Sine,   ///< one-sided sine transform   2 * integral y(x) sin(kx) dx
    Power   ///< power spectrum |integral y(x) exp(-ikx) dx|^2
};

/**
 * @brief Window (taper) applied to the data before a Fourier transform
 */
enum class FourierWindow {
    None, ///< transform the data as it stands
    Hann  ///< Hann taper: 1 at the first sample decaying to 0 at the last
};

/**
 * @brief Fourier transform of sampled data by direct quadrature
 *
 * Computes the transform integral over the sampled x range with the
 * trapezoidal rule, evaluated at each requested k value.  Working on the
 * integral directly keeps arbitrary (also non-uniformly spaced) x grids and
 * arbitrary output grids possible; with the modest series lengths of chart
 * data the O(N*M) cost is irrelevant.  The cosine and sine transforms are the
 * one-sided conventions with their factor 2, so the cosine transform of an
 * autocorrelation function is the spectral density (Wiener-Khinchin); k is an
 * angular frequency (rad per x unit).  The Hann taper is 1 at the first
 * sample and 0 at the last, which suppresses the ringing of data truncated
 * before it has decayed to zero.
 *
 * @param x      Sample positions, ascending
 * @param y      Sample values (same length as @p x)
 * @param k      Angular frequencies / wavenumbers to evaluate at
 * @param kind   Which transform to compute
 * @param window Taper applied to @p y before transforming
 * @return One value per entry of @p k; empty if the input has fewer than two
 *         samples or the lengths of @p x and @p y differ
 */
std::vector<double> fourierTransform(const std::vector<double> &x, const std::vector<double> &y,
                                     const std::vector<double> &k, FourierKind kind,
                                     FourierWindow window = FourierWindow::None);

/**
 * @brief Static structure factor from a radial distribution function
 *
 * Computes @f$ S(q) = 1 + 4\pi\rho \int_0^R r^2\,(g(r)-1)\,
 * \frac{\sin(qr)}{qr}\,dr @f$ by the trapezoidal rule over the sampled r
 * range.  The @f$\sin(qr)/(qr)@f$ form is regular at @f$ q = 0 @f$ (where it
 * is 1), so the q grid may start at zero.  The -1 shift is applied here, so
 * @p g is the plain radial distribution function as imported.  The optional
 * Hann taper is applied to @f$ g(r)-1 @f$ to suppress the ringing from
 * truncating it at a finite R where it has not fully decayed.
 *
 * @param r      Radial sample positions, ascending
 * @param g      Radial distribution function values (same length as @p r)
 * @param rho    Number density N/V, in the units of the r axis cubed
 * @param q      Wavenumbers to evaluate at (angular, rad per r unit)
 * @param window Taper applied to g(r)-1 before transforming
 * @return S(q), one value per entry of @p q; empty if the input has fewer
 *         than two samples or the lengths of @p r and @p g differ
 */
std::vector<double> structureFactor(const std::vector<double> &r, const std::vector<double> &g,
                                    double rho, const std::vector<double> &q,
                                    FourierWindow window = FourierWindow::None);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
