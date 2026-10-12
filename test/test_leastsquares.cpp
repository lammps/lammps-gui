// Unit tests for the Qt-free least-squares / linear-algebra toolkit
// (src/leastsquares.cpp), exercised directly without a GUI.

#include "leastsquares.h"

#include "gtest/gtest.h"

#include <cmath>

namespace {

TEST(LeastSquares, Transpose)
{
    float_mat a(2, 3);
    double v = 1.0;
    for (std::size_t i = 0; i < 2; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            a[i][j] = v++; // [[1,2,3],[4,5,6]]

    const float_mat t = transpose(a);
    ASSERT_EQ(t.nr_rows(), 3u);
    ASSERT_EQ(t.nr_cols(), 2u);
    EXPECT_DOUBLE_EQ(t[0][0], 1.0);
    EXPECT_DOUBLE_EQ(t[0][1], 4.0);
    EXPECT_DOUBLE_EQ(t[1][0], 2.0);
    EXPECT_DOUBLE_EQ(t[2][1], 6.0);
}

TEST(LeastSquares, MatrixMultiply)
{
    float_mat a(2, 2);
    a[0][0] = 1.0;
    a[0][1] = 2.0;
    a[1][0] = 3.0;
    a[1][1] = 4.0;
    float_mat b(2, 2);
    b[0][0] = 5.0;
    b[0][1] = 6.0;
    b[1][0] = 7.0;
    b[1][1] = 8.0;

    const float_mat c = a * b; // [[19,22],[43,50]]
    EXPECT_DOUBLE_EQ(c[0][0], 19.0);
    EXPECT_DOUBLE_EQ(c[0][1], 22.0);
    EXPECT_DOUBLE_EQ(c[1][0], 43.0);
    EXPECT_DOUBLE_EQ(c[1][1], 50.0);
}

TEST(LeastSquares, LinSolve)
{
    // 2x + y = 1 ; x + 3y = 2  ->  x = 0.2, y = 0.6
    float_mat A(2, 2);
    A[0][0] = 2.0;
    A[0][1] = 1.0;
    A[1][0] = 1.0;
    A[1][1] = 3.0;
    float_mat rhs(2, 1);
    rhs[0][0] = 1.0;
    rhs[1][0] = 2.0;

    const float_mat x = lin_solve(A, rhs);
    ASSERT_EQ(x.nr_rows(), 2u);
    ASSERT_EQ(x.nr_cols(), 1u);
    EXPECT_NEAR(x[0][0], 0.2, 1.0e-12);
    EXPECT_NEAR(x[1][0], 0.6, 1.0e-12);
}

TEST(LeastSquares, LinSolveMultiColumnRhs)
{
    // Solve A X = B with a non-square (2-column) right-hand side. This is the
    // general case (the back/forward substitution must iterate the RHS columns,
    // not the coefficient-matrix columns).
    //   col 0: 2x+y=1, x+3y=2  -> (0.2, 0.6)
    //   col 1: 2x+y=0, x+3y=5  -> (-1.0, 2.0)
    float_mat A(2, 2);
    A[0][0] = 2.0;
    A[0][1] = 1.0;
    A[1][0] = 1.0;
    A[1][1] = 3.0;
    float_mat rhs(2, 2);
    rhs[0][0] = 1.0;
    rhs[0][1] = 0.0;
    rhs[1][0] = 2.0;
    rhs[1][1] = 5.0;

    const float_mat x = lin_solve(A, rhs);
    ASSERT_EQ(x.nr_rows(), 2u);
    ASSERT_EQ(x.nr_cols(), 2u);
    EXPECT_NEAR(x[0][0], 0.2, 1.0e-12);
    EXPECT_NEAR(x[1][0], 0.6, 1.0e-12);
    EXPECT_NEAR(x[0][1], -1.0, 1.0e-12);
    EXPECT_NEAR(x[1][1], 2.0, 1.0e-12);
}

TEST(LeastSquares, Invert)
{
    // inverse of [[2,1],[1,3]] = (1/5)[[3,-1],[-1,2]]
    float_mat A(2, 2);
    A[0][0] = 2.0;
    A[0][1] = 1.0;
    A[1][0] = 1.0;
    A[1][1] = 3.0;

    const float_mat inv = invert(A);
    EXPECT_NEAR(inv[0][0], 0.6, 1.0e-12);
    EXPECT_NEAR(inv[0][1], -0.2, 1.0e-12);
    EXPECT_NEAR(inv[1][0], -0.2, 1.0e-12);
    EXPECT_NEAR(inv[1][1], 0.4, 1.0e-12);

    // A * inv(A) should be the identity
    const float_mat id = A * inv;
    EXPECT_NEAR(id[0][0], 1.0, 1.0e-12);
    EXPECT_NEAR(id[0][1], 0.0, 1.0e-12);
    EXPECT_NEAR(id[1][0], 0.0, 1.0e-12);
    EXPECT_NEAR(id[1][1], 1.0, 1.0e-12);
}

TEST(SavitzkyGolay, ConstantUnchangedMovingAverage)
{
    const float_vect v(11, 5.0);
    const float_vect out = sg_smooth(v, 2, 0); // moving average
    ASSERT_EQ(out.size(), v.size());
    for (double y : out)
        EXPECT_NEAR(y, 5.0, 1.0e-9);
}

TEST(SavitzkyGolay, LinearPreservedDegree1)
{
    float_vect v(11);
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = 2.0 * static_cast<double>(i) + 1.0; // a line

    const float_vect out = sg_smooth(v, 2, 1);
    ASSERT_EQ(out.size(), v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        EXPECT_NEAR(out[i], v[i], 1.0e-6) << "index " << i;
}

TEST(SavitzkyGolay, QuadraticPreservedDegree2)
{
    float_vect v(11);
    for (std::size_t i = 0; i < v.size(); ++i) {
        const double x = static_cast<double>(i);
        v[i]           = x * x; // a parabola
    }

    const float_vect out = sg_smooth(v, 3, 2);
    ASSERT_EQ(out.size(), v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        EXPECT_NEAR(out[i], v[i], 1.0e-6) << "index " << i;
}

TEST(SavitzkyGolay, ReducesNoiseAroundLine)
{
    // a line with alternating +/-1 noise; smoothing should pull values toward
    // the underlying line at interior points
    float_vect v(21);
    for (std::size_t i = 0; i < v.size(); ++i) {
        const double line = 3.0 * static_cast<double>(i);
        v[i]              = line + ((i % 2 == 0) ? 1.0 : -1.0);
    }

    const float_vect out = sg_smooth(v, 4, 1);
    ASSERT_EQ(out.size(), v.size());
    const std::size_t mid = v.size() / 2;
    EXPECT_LT(std::fabs(out[mid] - 3.0 * static_cast<double>(mid)),
              std::fabs(v[mid] - 3.0 * static_cast<double>(mid)));
}

} // namespace
