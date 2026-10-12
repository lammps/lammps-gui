// Unit tests for the Qt-free axis-layout helpers (src/plotaxismath.cpp),
// exercised without a GUI.

#include "plotaxismath.h"

#include "gtest/gtest.h"

#include <cmath>
#include <string>
#include <vector>

using namespace PlotAxisMath;

namespace {

// ---- niceTickInterval ----------------------------------------------------

TEST(NiceTickInterval, NonPositiveOrNonFiniteRangeIsOne)
{
    EXPECT_DOUBLE_EQ(niceTickInterval(0.0), 1.0);
    EXPECT_DOUBLE_EQ(niceTickInterval(-5.0), 1.0);
    EXPECT_DOUBLE_EQ(niceTickInterval(std::nan("")), 1.0);
}

TEST(NiceTickInterval, SelectsOneTwoFiveTen)
{
    // targetIntervals = 1 so range maps directly onto the rough value
    EXPECT_DOUBLE_EQ(niceTickInterval(1.2, 1), 1.0);  // frac 1.2  -> 1
    EXPECT_DOUBLE_EQ(niceTickInterval(2.0, 1), 2.0);  // frac 2.0  -> 2
    EXPECT_DOUBLE_EQ(niceTickInterval(4.0, 1), 5.0);  // frac 4.0  -> 5
    EXPECT_DOUBLE_EQ(niceTickInterval(8.0, 1), 10.0); // frac 8.0 -> 10
}

TEST(NiceTickInterval, DefaultFourIntervals)
{
    // rough = range/4
    EXPECT_DOUBLE_EQ(niceTickInterval(10.0), 2.0);   // rough 2.5 -> 2
    EXPECT_DOUBLE_EQ(niceTickInterval(40.0), 10.0);  // rough 10  -> 1 * 10
    EXPECT_DOUBLE_EQ(niceTickInterval(100.0), 20.0); // rough 25  -> 2 * 10
    EXPECT_DOUBLE_EQ(niceTickInterval(1000.0), 200.0);
}

TEST(NiceTickInterval, ScalesAcrossPowersOfTen)
{
    EXPECT_DOUBLE_EQ(niceTickInterval(1.0), 0.2);    // rough 0.25 -> 2 * 0.1
    EXPECT_DOUBLE_EQ(niceTickInterval(0.01), 0.002); // rough 0.0025 -> 2 * 0.001
}

TEST(NiceTickInterval, NonPositiveTargetClampsToOne)
{
    EXPECT_DOUBLE_EQ(niceTickInterval(2.0, 0), niceTickInterval(2.0, 1));
    EXPECT_DOUBLE_EQ(niceTickInterval(2.0, -3), niceTickInterval(2.0, 1));
}

// ---- tickValues ----------------------------------------------------------

TEST(TickValues, EvenlySpacedFromZero)
{
    const std::vector<double> t = tickValues(0.0, 10.0, 2.0);
    ASSERT_EQ(t.size(), 6u);
    const double expect[] = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0};
    for (std::size_t i = 0; i < t.size(); ++i)
        EXPECT_DOUBLE_EQ(t[i], expect[i]);
}

TEST(TickValues, AlignedToAnchorZeroWithinRange)
{
    // ticks fall on multiples of 2 that lie within [1, 9]
    const std::vector<double> t = tickValues(1.0, 9.0, 2.0);
    ASSERT_EQ(t.size(), 4u);
    EXPECT_DOUBLE_EQ(t.front(), 2.0);
    EXPECT_DOUBLE_EQ(t.back(), 8.0);
}

TEST(TickValues, SpansNegativeAndSnapsZero)
{
    const std::vector<double> t = tickValues(-5.0, 5.0, 5.0);
    ASSERT_EQ(t.size(), 3u);
    EXPECT_DOUBLE_EQ(t[0], -5.0);
    EXPECT_DOUBLE_EQ(t[1], 0.0); // exactly zero, not -0 or tiny residue
    EXPECT_DOUBLE_EQ(t[2], 5.0);
}

TEST(TickValues, IncludesEndpointDespiteFloatingError)
{
    const std::vector<double> t = tickValues(0.0, 1.0, 0.1);
    ASSERT_EQ(t.size(), 11u);
    EXPECT_NEAR(t.front(), 0.0, 1.0e-12);
    EXPECT_NEAR(t.back(), 1.0, 1.0e-9);
}

TEST(TickValues, SwapsReversedRange)
{
    EXPECT_EQ(tickValues(10.0, 0.0, 2.0), tickValues(0.0, 10.0, 2.0));
}

TEST(TickValues, InvalidIntervalIsEmpty)
{
    EXPECT_TRUE(tickValues(0.0, 10.0, 0.0).empty());
    EXPECT_TRUE(tickValues(0.0, 10.0, -1.0).empty());
    EXPECT_TRUE(tickValues(0.0, 10.0, std::nan("")).empty());
}

TEST(TickValues, CustomAnchor)
{
    // grid aligned to 0.5 instead of 0
    const std::vector<double> t = tickValues(0.0, 2.0, 1.0, 0.5);
    ASSERT_EQ(t.size(), 2u);
    EXPECT_DOUBLE_EQ(t[0], 0.5);
    EXPECT_DOUBLE_EQ(t[1], 1.5);
}

// ---- tickDecimals --------------------------------------------------------

TEST(TickDecimals, IntegerOrLargerSpacingsAreZero)
{
    EXPECT_EQ(tickDecimals(1.0), 0);
    EXPECT_EQ(tickDecimals(2.0), 0);
    EXPECT_EQ(tickDecimals(5.0), 0);
    EXPECT_EQ(tickDecimals(10.0), 0);
    EXPECT_EQ(tickDecimals(2000.0), 0);
}

TEST(TickDecimals, FractionalSpacings)
{
    EXPECT_EQ(tickDecimals(0.5), 1);
    EXPECT_EQ(tickDecimals(0.2), 1);
    EXPECT_EQ(tickDecimals(0.1), 1);
    EXPECT_EQ(tickDecimals(0.05), 2);
    EXPECT_EQ(tickDecimals(0.02), 2);
    EXPECT_EQ(tickDecimals(0.01), 2);
    EXPECT_EQ(tickDecimals(0.001), 3);
}

TEST(TickDecimals, NonPositiveIsZero)
{
    EXPECT_EQ(tickDecimals(0.0), 0);
    EXPECT_EQ(tickDecimals(-1.0), 0);
    EXPECT_EQ(tickDecimals(std::nan("")), 0);
}

// ---- formatAxisLabel -----------------------------------------------------

TEST(FormatAxisLabel, IntegerSpecifier)
{
    EXPECT_EQ(formatAxisLabel(1000.0, "%d"), "1000");
    EXPECT_EQ(formatAxisLabel(5.0, "%03d"), "005");
    EXPECT_EQ(formatAxisLabel(-7.0, "%d"), "-7");
}

TEST(FormatAxisLabel, IntegerRounds)
{
    EXPECT_EQ(formatAxisLabel(2.9, "%d"), "3");
    EXPECT_EQ(formatAxisLabel(1999.9999999, "%d"), "2000");
}

TEST(FormatAxisLabel, NormalizesLengthModifier)
{
    // a stray length modifier must not cause undefined behavior
    EXPECT_EQ(formatAxisLabel(1000.0, "%ld"), "1000");
}

TEST(FormatAxisLabel, FloatingSpecifiers)
{
    EXPECT_EQ(formatAxisLabel(2.5, "%.1f"), "2.5");
    EXPECT_EQ(formatAxisLabel(123.456, "%.6g"), "123.456");
    EXPECT_EQ(formatAxisLabel(0.000123, "%.2e"), "1.23e-04");
}

TEST(FormatAxisLabel, EmptyDefaultsToG)
{
    EXPECT_EQ(formatAxisLabel(42.0, ""), "42");
    EXPECT_EQ(formatAxisLabel(1000000.0, ""), "1e+06");
}

TEST(FormatAxisLabel, LiteralPrefixSuffixAndPercent)
{
    EXPECT_EQ(formatAxisLabel(3.0, "x=%d units"), "x=3 units");
    EXPECT_EQ(formatAxisLabel(50.0, "%d%%"), "50%");
}

TEST(FormatAxisLabel, NoPlaceholderReturnedUnchanged)
{
    EXPECT_EQ(formatAxisLabel(3.0, "hello"), "hello");
    EXPECT_EQ(formatAxisLabel(3.0, "%y"), "%y"); // unknown specifier
}

} // namespace
