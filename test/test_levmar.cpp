// Unit tests for the Levenberg-Marquardt nonlinear least-squares solver. The
// models and their Jacobians are coded by hand here so the test exercises only
// the levmar core (and the shared leastsquares LU solver), independent of the
// LeptonMini expression layer that the GUI uses to build them.

#include "levmar.h"

#include "gtest/gtest.h"

#include <cmath>
#include <vector>

namespace {

// recover a straight line y = a + b*x from exact data
TEST(Levmar, Linear)
{
    std::vector<double> xs, ys;
    const double a = 2.0, b = 3.0;
    for (int i = 0; i < 10; ++i) {
        const double x = 0.5 * i;
        xs.push_back(x);
        ys.push_back(a + b * x);
    }
    const int m = static_cast<int>(xs.size());

    LevmarModel model = [&](const std::vector<double> &p, std::vector<double> &res,
                            std::vector<std::vector<double>> &jac) {
        for (int i = 0; i < m; ++i) {
            res[i]    = p[0] + p[1] * xs[i] - ys[i];
            jac[i][0] = 1.0;
            jac[i][1] = xs[i];
        }
        return true;
    };

    const LevmarResult r = levmarFit(m, 2, {0.0, 0.0}, model);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.params.size(), 2u);
    EXPECT_NEAR(r.params[0], a, 1e-6);
    EXPECT_NEAR(r.params[1], b, 1e-6);
    EXPECT_NEAR(r.rms, 0.0, 1e-6);
}

// recover an exponential decay y = A*exp(-k*x)
TEST(Levmar, ExponentialDecay)
{
    std::vector<double> xs, ys;
    const double A = 5.0, k = 0.7;
    for (int i = 0; i < 20; ++i) {
        const double x = 0.2 * i;
        xs.push_back(x);
        ys.push_back(A * std::exp(-k * x));
    }
    const int m = static_cast<int>(xs.size());

    LevmarModel model = [&](const std::vector<double> &p, std::vector<double> &res,
                            std::vector<std::vector<double>> &jac) {
        for (int i = 0; i < m; ++i) {
            const double e = std::exp(-p[1] * xs[i]);
            res[i]         = p[0] * e - ys[i];
            jac[i][0]      = e;                 // d/dA
            jac[i][1]      = -p[0] * xs[i] * e; // d/dk
        }
        return true;
    };

    const LevmarResult r = levmarFit(m, 2, {1.0, 0.1}, model);
    ASSERT_TRUE(r.ok);
    EXPECT_NEAR(r.params[0], A, 1e-4);
    EXPECT_NEAR(r.params[1], k, 1e-4);
    EXPECT_NEAR(r.rms, 0.0, 1e-5);
}

// recover a Gaussian y = A*exp(-(x-mu)^2/(2 sigma^2))
TEST(Levmar, Gaussian)
{
    std::vector<double> xs, ys;
    const double A = 2.0, mu = 1.0, sg = 0.8;
    for (int i = 0; i < 25; ++i) {
        const double x = -2.0 + 0.2 * i;
        xs.push_back(x);
        const double d = (x - mu);
        ys.push_back(A * std::exp(-d * d / (2.0 * sg * sg)));
    }
    const int m = static_cast<int>(xs.size());

    LevmarModel model = [&](const std::vector<double> &p, std::vector<double> &res,
                            std::vector<std::vector<double>> &jac) {
        for (int i = 0; i < m; ++i) {
            const double d  = xs[i] - p[1];
            const double s2 = p[2] * p[2];
            const double e  = std::exp(-d * d / (2.0 * s2));
            res[i]          = p[0] * e - ys[i];
            jac[i][0]       = e;                              // d/dA
            jac[i][1]       = p[0] * e * d / s2;              // d/dmu
            jac[i][2]       = p[0] * e * d * d / (s2 * p[2]); // d/dsigma
        }
        return true;
    };

    const LevmarResult r = levmarFit(m, 3, {1.0, 0.5, 1.0}, model);
    ASSERT_TRUE(r.ok);
    EXPECT_NEAR(r.params[0], A, 1e-3);
    EXPECT_NEAR(r.params[1], mu, 1e-3);
    EXPECT_NEAR(std::fabs(r.params[2]), sg, 1e-3); // sigma enters only squared
}

// noisy data: the fit should land close to the generating parameters
TEST(Levmar, ExponentialWithNoise)
{
    std::vector<double> xs, ys;
    const double A = 4.0, k = 0.5;
    // small deterministic perturbation so the test stays reproducible
    const double noise[8] = {0.02, -0.03, 0.01, -0.015, 0.025, -0.02, 0.018, -0.012};
    for (int i = 0; i < 8; ++i) {
        const double x = 0.4 * i;
        xs.push_back(x);
        ys.push_back(A * std::exp(-k * x) + noise[i]);
    }
    const int m = static_cast<int>(xs.size());

    LevmarModel model = [&](const std::vector<double> &p, std::vector<double> &res,
                            std::vector<std::vector<double>> &jac) {
        for (int i = 0; i < m; ++i) {
            const double e = std::exp(-p[1] * xs[i]);
            res[i]         = p[0] * e - ys[i];
            jac[i][0]      = e;
            jac[i][1]      = -p[0] * xs[i] * e;
        }
        return true;
    };

    const LevmarResult r = levmarFit(m, 2, {1.0, 0.1}, model);
    ASSERT_TRUE(r.ok);
    EXPECT_NEAR(r.params[0], A, 0.1);
    EXPECT_NEAR(r.params[1], k, 0.1);
}

// invalid dimensions (more parameters than data points) are rejected
TEST(Levmar, RejectsBadDimensions)
{
    LevmarModel model = [](const std::vector<double> &, std::vector<double> &,
                           std::vector<std::vector<double>> &) {
        return true;
    };
    const LevmarResult r = levmarFit(1, 3, {0.0, 0.0, 0.0}, model);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.message.empty());
}

// a failing initial evaluation is reported, not crashed on
TEST(Levmar, ReportsInitialFailure)
{
    LevmarModel model = [](const std::vector<double> &, std::vector<double> &,
                           std::vector<std::vector<double>> &) {
        return false;
    };
    const LevmarResult r = levmarFit(5, 2, {1.0, 1.0}, model);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.message.empty());
}

} // namespace
