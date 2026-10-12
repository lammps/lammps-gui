// Unit tests for the linear-least-squares curve fits (src/fitting.cpp),
// exercised without a GUI.

#include "fitting.h"

#include "gtest/gtest.h"

#include <cmath>
#include <vector>

namespace {

TEST(PolynomialFit, RecoversQuadratic)
{
    // y = 2 + 3x + x^2
    std::vector<double> x, y;
    for (int i = 0; i <= 5; ++i) {
        const double xi = i;
        x.push_back(xi);
        y.push_back(2.0 + 3.0 * xi + xi * xi);
    }
    const PolynomialFit fit = polynomialFit(x, y, 2);
    ASSERT_TRUE(fit.ok);
    ASSERT_EQ(fit.coeffs.size(), 3u);
    EXPECT_NEAR(fit.coeffs[0], 2.0, 1.0e-6);
    EXPECT_NEAR(fit.coeffs[1], 3.0, 1.0e-6);
    EXPECT_NEAR(fit.coeffs[2], 1.0, 1.0e-6);
    EXPECT_NEAR(fit.rms, 0.0, 1.0e-6);
}

TEST(PolynomialFit, RecoversLine)
{
    std::vector<double> x, y;
    for (int i = 0; i <= 4; ++i) {
        const double xi = i;
        x.push_back(xi);
        y.push_back(5.0 - 2.0 * xi);
    }
    const PolynomialFit fit = polynomialFit(x, y, 1);
    ASSERT_TRUE(fit.ok);
    ASSERT_EQ(fit.coeffs.size(), 2u);
    EXPECT_NEAR(fit.coeffs[0], 5.0, 1.0e-9);
    EXPECT_NEAR(fit.coeffs[1], -2.0, 1.0e-9);
}

TEST(PolynomialFit, TooFewPointsFails)
{
    const PolynomialFit fit = polynomialFit({0.0, 1.0}, {1.0, 2.0}, 3);
    EXPECT_FALSE(fit.ok);
}

TEST(PolynomialFit, Eval)
{
    EXPECT_DOUBLE_EQ(evalPolynomial({2.0, 3.0, 1.0}, 2.0), 12.0); // 2 + 6 + 4
    EXPECT_DOUBLE_EQ(evalPolynomial({}, 5.0), 0.0);
}

TEST(BirchMurnaghan, RecoversKnownModel)
{
    // E(V) = a + b V^-2/3 + c V^-4/3 + d V^-2 with a=10, b=-6, c=1.5, d=1.
    // Constructed so the minimum is at V0 = 1 (u = V^-2/3 = 1): the equation
    // 3 d u^2 + 2 c u + b = 0 has roots u = 1 and u = -2.
    const double a = 10.0, b = -6.0, c = 1.5, d = 1.0;
    std::vector<double> v, e;
    for (int i = 6; i <= 18; ++i) {
        const double vol = 0.1 * i; // 0.6 .. 1.8
        const double u   = std::pow(vol, -2.0 / 3.0);
        v.push_back(vol);
        e.push_back(a + b * u + c * u * u + d * u * u * u);
    }

    const EosFit fit = birchMurnaghanFit(v, e);
    ASSERT_TRUE(fit.ok);
    EXPECT_NEAR(fit.a, a, 1.0e-5);
    EXPECT_NEAR(fit.b, b, 1.0e-5);
    EXPECT_NEAR(fit.c, c, 1.0e-5);
    EXPECT_NEAR(fit.d, d, 1.0e-5);

    EXPECT_NEAR(fit.v0, 1.0, 1.0e-4);
    EXPECT_NEAR(fit.e0, 6.5, 1.0e-4); // 10 - 6 + 1.5 + 1
    EXPECT_NEAR(fit.b0, 4.0, 1.0e-4);
    EXPECT_NEAR(fit.b0prime, 40.0 / 9.0, 1.0e-4); // ~4.4444
    EXPECT_NEAR(fit.rms, 0.0, 1.0e-6);

    // evaluating at V0 reproduces E0
    EXPECT_NEAR(evalBirchMurnaghan(fit, fit.v0), fit.e0, 1.0e-9);
}

TEST(BirchMurnaghan, TooFewPointsFails)
{
    const EosFit fit = birchMurnaghanFit({1.0, 1.1, 1.2}, {0.0, -0.1, 0.0});
    EXPECT_FALSE(fit.ok);
}

TEST(BirchMurnaghan, NonPositiveVolumeFails)
{
    const EosFit fit = birchMurnaghanFit({1.0, 0.0, 1.2, 1.3}, {0.0, -0.1, 0.0, 0.1});
    EXPECT_FALSE(fit.ok);
}

} // namespace
