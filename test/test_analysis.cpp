// Unit tests for the post-processing analyses (src/analysis.cpp),
// exercised without a GUI.

#include "analysis.h"

#include "gtest/gtest.h"

#include <cmath>
#include <vector>

namespace {

TEST(Autocorrelation, ExactSmallCase)
{
    // y = [1,2,3], mean 2, deviations [-1,0,1], denom = 2
    //   lag0 = 2/2 = 1 ; lag1 = 0/2 = 0 ; lag2 = -1/2 = -0.5
    const std::vector<double> acf = autocorrelation({1.0, 2.0, 3.0}, 2);
    ASSERT_EQ(acf.size(), 3u);
    EXPECT_NEAR(acf[0], 1.0, 1.0e-12);
    EXPECT_NEAR(acf[1], 0.0, 1.0e-12);
    EXPECT_NEAR(acf[2], -0.5, 1.0e-12);
}

TEST(Autocorrelation, LagZeroIsOne)
{
    const std::vector<double> acf = autocorrelation({3.0, 1.0, 4.0, 1.0, 5.0, 9.0}, 3);
    ASSERT_FALSE(acf.empty());
    EXPECT_NEAR(acf[0], 1.0, 1.0e-12);
    for (double v : acf)
        EXPECT_LE(std::fabs(v), 1.0 + 1.0e-12);
}

TEST(Autocorrelation, ConstantSeriesIsEmpty)
{
    EXPECT_TRUE(autocorrelation({5.0, 5.0, 5.0, 5.0}, 2).empty());
}

TEST(Autocorrelation, TooShortIsEmpty)
{
    EXPECT_TRUE(autocorrelation({}, 5).empty());
    EXPECT_TRUE(autocorrelation({1.0}, 5).empty());
}

TEST(Autocorrelation, MaxlagClampedToLength)
{
    const std::vector<double> y = {1.0, 2.0, 3.0, 4.0, 5.0};
    // non-positive maxlag clamps to n-1
    EXPECT_EQ(autocorrelation(y, 0).size(), 5u);
    // oversized maxlag clamps to n-1
    EXPECT_EQ(autocorrelation(y, 100).size(), 5u);
    // in-range maxlag is honored
    EXPECT_EQ(autocorrelation(y, 2).size(), 3u);
}

TEST(Autocorrelation, AlternatingIsAnticorrelated)
{
    std::vector<double> y(100);
    for (std::size_t i = 0; i < y.size(); ++i)
        y[i] = (i % 2 == 0) ? 1.0 : -1.0;

    const std::vector<double> acf = autocorrelation(y, 4);
    ASSERT_EQ(acf.size(), 5u);
    EXPECT_NEAR(acf[0], 1.0, 1.0e-12);
    EXPECT_LT(acf[1], 0.0); // odd lags: anticorrelated
    EXPECT_GT(acf[2], 0.0); // even lags: correlated
    EXPECT_LT(acf[3], 0.0);
}

// exp(-a x) sampled densely enough that the finite range and step barely
// matter, so the transforms can be checked against their analytic forms
std::vector<double> expGridX(double dx, double xmax)
{
    std::vector<double> x;
    for (double v = 0.0; v <= xmax; v += dx)
        x.push_back(v);
    return x;
}

std::vector<double> expGridY(const std::vector<double> &x, double a)
{
    std::vector<double> y;
    y.reserve(x.size());
    for (double v : x)
        y.push_back(std::exp(-a * v));
    return y;
}

// 2 * integral exp(-ax) cos(kx) = 2a/(a^2+k^2), the Lorentzian spectral
// density of an exponentially decaying correlation function
TEST(FourierTransform, CosineOfExponentialIsLorentzian)
{
    const double a                = 1.0;
    const std::vector<double> x   = expGridX(0.005, 30.0);
    const std::vector<double> y   = expGridY(x, a);
    const std::vector<double> k   = {0.0, 0.5, 1.0, 2.0, 5.0};
    const std::vector<double> res = fourierTransform(x, y, k, FourierKind::Cosine);
    ASSERT_EQ(res.size(), k.size());
    for (std::size_t i = 0; i < k.size(); ++i)
        EXPECT_NEAR(res[i], 2.0 * a / (a * a + k[i] * k[i]), 1.0e-4);
}

// 2 * integral exp(-ax) sin(kx) = 2k/(a^2+k^2)
TEST(FourierTransform, SineOfExponential)
{
    const double a                = 2.0;
    const std::vector<double> x   = expGridX(0.005, 20.0);
    const std::vector<double> y   = expGridY(x, a);
    const std::vector<double> k   = {0.0, 1.0, 3.0};
    const std::vector<double> res = fourierTransform(x, y, k, FourierKind::Sine);
    ASSERT_EQ(res.size(), k.size());
    for (std::size_t i = 0; i < k.size(); ++i)
        EXPECT_NEAR(res[i], 2.0 * k[i] / (a * a + k[i] * k[i]), 1.0e-4);
}

// |integral exp(-ax) exp(-ikx)|^2 = 1/(a^2+k^2)
TEST(FourierTransform, PowerOfExponential)
{
    const double a                = 1.0;
    const std::vector<double> x   = expGridX(0.005, 30.0);
    const std::vector<double> y   = expGridY(x, a);
    const std::vector<double> k   = {0.0, 1.0, 2.0};
    const std::vector<double> res = fourierTransform(x, y, k, FourierKind::Power);
    ASSERT_EQ(res.size(), k.size());
    for (std::size_t i = 0; i < k.size(); ++i)
        EXPECT_NEAR(res[i], 1.0 / (a * a + k[i] * k[i]), 1.0e-4);
}

// the quadrature works on a non-uniform grid: same Lorentzian, sampled with a
// step that grows quadratically
TEST(FourierTransform, NonUniformGrid)
{
    const double a = 1.0;
    std::vector<double> x;
    for (int i = 0; i <= 4000; ++i) {
        const double t = 30.0 * (i / 4000.0) * (i / 4000.0);
        x.push_back(t);
    }
    const std::vector<double> y   = expGridY(x, a);
    const std::vector<double> k   = {0.5, 1.0};
    const std::vector<double> res = fourierTransform(x, y, k, FourierKind::Cosine);
    ASSERT_EQ(res.size(), k.size());
    for (std::size_t i = 0; i < k.size(); ++i)
        EXPECT_NEAR(res[i], 2.0 * a / (a * a + k[i] * k[i]), 1.0e-3);
}

// the Hann taper is 1 at the first sample, 0 at the last, 1/2 in the middle:
// the k = 0 cosine transform of a constant 1 is then 2 * T/2 = T
TEST(FourierTransform, HannTaper)
{
    const std::vector<double> x = expGridX(0.01, 10.0);
    const std::vector<double> y(x.size(), 1.0);
    const std::vector<double> res =
        fourierTransform(x, y, {0.0}, FourierKind::Cosine, FourierWindow::Hann);
    ASSERT_EQ(res.size(), 1u);
    EXPECT_NEAR(res[0], 10.0, 1.0e-4);
}

TEST(FourierTransform, DegenerateInputIsEmpty)
{
    EXPECT_TRUE(fourierTransform({}, {}, {1.0}, FourierKind::Cosine).empty());
    EXPECT_TRUE(fourierTransform({1.0}, {1.0}, {1.0}, FourierKind::Cosine).empty());
    EXPECT_TRUE(fourierTransform({1.0, 2.0}, {1.0}, {1.0}, FourierKind::Cosine).empty());
}

// an ideal gas has g(r) = 1 everywhere, so S(q) = 1 for every q
TEST(StructureFactor, IdealGasIsOne)
{
    const std::vector<double> r = expGridX(0.01, 15.0);
    const std::vector<double> g(r.size(), 1.0);
    const std::vector<double> q   = {0.0, 1.0, 5.0};
    const std::vector<double> res = structureFactor(r, g, 0.8, q);
    ASSERT_EQ(res.size(), q.size());
    for (double v : res)
        EXPECT_NEAR(v, 1.0, 1.0e-12);
}

// h(r) = exp(-ar) has the exact transform
// integral r^2 h sinc(qr) dr = 2a/(a^2+q^2)^2, so
// S(q) = 1 + 8 pi rho a/(a^2+q^2)^2, which is also regular at q = 0
TEST(StructureFactor, ExponentialShellExact)
{
    const double a              = 2.0;
    const double rho            = 0.5;
    const double pi             = 3.14159265358979323846;
    const std::vector<double> r = expGridX(0.002, 20.0);
    std::vector<double> g;
    g.reserve(r.size());
    for (double rv : r)
        g.push_back(1.0 + std::exp(-a * rv));
    const std::vector<double> q   = {0.0, 1.0, 2.0, 4.0};
    const std::vector<double> res = structureFactor(r, g, rho, q);
    ASSERT_EQ(res.size(), q.size());
    for (std::size_t i = 0; i < q.size(); ++i) {
        const double d = a * a + q[i] * q[i];
        EXPECT_NEAR(res[i], 1.0 + 8.0 * pi * rho * a / (d * d), 1.0e-4);
    }
}

TEST(StructureFactor, DegenerateInputIsEmpty)
{
    EXPECT_TRUE(structureFactor({}, {}, 1.0, {1.0}).empty());
    EXPECT_TRUE(structureFactor({1.0, 2.0}, {1.0}, 1.0, {1.0}).empty());
}

} // namespace
