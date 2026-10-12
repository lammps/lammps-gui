// Unit tests for the customfunc module: evaluating user-supplied expressions
// (via the vendored LeptonMini parser) over an x range for custom-function
// plotting in the chart post-processing dialog.

#include "customfunc.h"

#include "gtest/gtest.h"

#include <cmath>
#include <vector>

// a polynomial sampled at the endpoints and midpoint
TEST(CustomFunc, Polynomial)
{
    const CustomCurve c = evalCustomCurve("2*x^2 + 1", 0.0, 2.0, 2);
    ASSERT_TRUE(c.ok);
    EXPECT_TRUE(c.error.isEmpty());
    ASSERT_EQ(c.points.size(), 3);
    EXPECT_DOUBLE_EQ(c.points[0].x(), 0.0);
    EXPECT_DOUBLE_EQ(c.points[0].y(), 1.0); // 2*0+1
    EXPECT_DOUBLE_EQ(c.points[1].x(), 1.0);
    EXPECT_DOUBLE_EQ(c.points[1].y(), 3.0); // 2*1+1
    EXPECT_DOUBLE_EQ(c.points[2].x(), 2.0);
    EXPECT_DOUBLE_EQ(c.points[2].y(), 9.0); // 2*4+1
}

// transcendental functions are recognized
TEST(CustomFunc, Trig)
{
    const CustomCurve c = evalCustomCurve("sin(x)", 0.0, M_PI, 2);
    ASSERT_TRUE(c.ok);
    ASSERT_EQ(c.points.size(), 3);
    EXPECT_NEAR(c.points[0].y(), 0.0, 1e-12);
    EXPECT_NEAR(c.points[1].y(), 1.0, 1e-12); // sin(pi/2)
    EXPECT_NEAR(c.points[2].y(), 0.0, 1e-12); // sin(pi)
}

// a constant expression yields a flat line
TEST(CustomFunc, Constant)
{
    const CustomCurve c = evalCustomCurve("42", -1.0, 1.0, 4);
    ASSERT_TRUE(c.ok);
    ASSERT_EQ(c.points.size(), 5);
    for (const auto &p : c.points)
        EXPECT_DOUBLE_EQ(p.y(), 42.0);
}

// a custom variable name can be used instead of x
TEST(CustomFunc, CustomVariable)
{
    const CustomCurve c = evalCustomCurve("v^2", 1.0, 3.0, 2, "v");
    ASSERT_TRUE(c.ok);
    ASSERT_EQ(c.points.size(), 3);
    EXPECT_DOUBLE_EQ(c.points[0].y(), 1.0);
    EXPECT_DOUBLE_EQ(c.points[1].y(), 4.0);
    EXPECT_DOUBLE_EQ(c.points[2].y(), 9.0);
}

// non-finite values are skipped: 1/x at x=0 is dropped, the rest are kept
TEST(CustomFunc, SkipsNonFinite)
{
    const CustomCurve c = evalCustomCurve("1/x", 0.0, 2.0, 2);
    ASSERT_TRUE(c.ok);
    // x = 0 gives inf and is dropped; x = 1 and x = 2 remain
    ASSERT_EQ(c.points.size(), 2);
    EXPECT_DOUBLE_EQ(c.points[0].x(), 1.0);
    EXPECT_DOUBLE_EQ(c.points[0].y(), 1.0);
    EXPECT_DOUBLE_EQ(c.points[1].x(), 2.0);
    EXPECT_DOUBLE_EQ(c.points[1].y(), 0.5);
}

// an empty expression is reported as an error, not a crash
TEST(CustomFunc, EmptyExpression)
{
    const CustomCurve c = evalCustomCurve("   ", 0.0, 1.0, 4);
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.error.isEmpty());
    EXPECT_TRUE(c.points.isEmpty());
}

// {name} spans become generated variables; everything else passes through
TEST(ColumnRefs, Substitute)
{
    const QStringList names   = {"Step", "Temp"};
    const ColumnRefExpr subst = substituteColumnRefs("{Temp}/2 + {Step}", names);
    ASSERT_TRUE(subst.ok);
    ASSERT_EQ(subst.refs.size(), 2);
    EXPECT_EQ(subst.refs[0].column, 1);
    EXPECT_TRUE(subst.refs[0].accessor.isEmpty());
    EXPECT_EQ(subst.refs[1].column, 0);
    EXPECT_EQ(subst.expr, subst.refs[0].variable + "/2 + " + subst.refs[1].variable);
}

// names with brackets, operators, parentheses, and spaces all resolve, since
// the braces end the span before Lepton ever sees the name
TEST(ColumnRefs, AwkwardNames)
{
    const QStringList names = {"c_rdf[2]", "g(r)", "r / sigma", "2theta"};
    const ColumnRefExpr subst =
        substituteColumnRefs("{c_rdf[2]}*{g(r)}-{r / sigma}+{2theta}", names);
    ASSERT_TRUE(subst.ok);
    ASSERT_EQ(subst.refs.size(), 4);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(subst.refs[i].column, i);
    // the rewritten expression must be valid for the Lepton parser
    const CompiledExpression program(subst.expr);
    EXPECT_TRUE(program.isValid());
}

// repeated references to the same column reuse one generated variable
TEST(ColumnRefs, DeduplicatesReferences)
{
    const ColumnRefExpr subst = substituteColumnRefs("{a}*{a}+{a:mean}", {"a"});
    ASSERT_TRUE(subst.ok);
    ASSERT_EQ(subst.refs.size(), 2); // {a} and {a:mean}
    EXPECT_TRUE(subst.refs[0].accessor.isEmpty());
    EXPECT_EQ(subst.refs[1].accessor, "mean");
}

// a colon inside braces always selects an accessor
TEST(ColumnRefs, Accessors)
{
    for (const char *acc : {"first", "last", "min", "max", "mean"}) {
        const ColumnRefExpr subst = substituteColumnRefs(QString("{T:%1}").arg(acc), {"T"});
        ASSERT_TRUE(subst.ok) << acc;
        ASSERT_EQ(subst.refs.size(), 1);
        EXPECT_EQ(subst.refs[0].column, 0);
        EXPECT_EQ(subst.refs[0].accessor, acc);
    }
    const ColumnRefExpr bad = substituteColumnRefs("{T:median}", {"T"});
    EXPECT_FALSE(bad.ok);
    EXPECT_TRUE(bad.error.contains("median"));
}

// lookup failures name the offender; a colon-in-name column is called out as
// unreferenceable rather than reported as missing
TEST(ColumnRefs, Errors)
{
    ColumnRefExpr r = substituteColumnRefs("{nope}+1", {"Step", "Temp"});
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.error.contains("nope"));
    EXPECT_TRUE(r.error.contains("{Step}"));

    r = substituteColumnRefs("{T:ps}", {"T:ps"});
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.error.contains("rename"));

    EXPECT_FALSE(substituteColumnRefs("{Step", {"Step"}).ok);  // unmatched {
    EXPECT_FALSE(substituteColumnRefs("Step}", {"Step"}).ok);  // unmatched }
    EXPECT_FALSE(substituteColumnRefs("{} + 1", {"Step"}).ok); // empty span
    EXPECT_TRUE(substituteColumnRefs("1 + 2", {"Step"}).ok);   // no refs at all
}

// renaming rewrites exactly the spans of the renamed column
TEST(ColumnRefs, Rename)
{
    EXPECT_EQ(renameColumnRefs("{old}*2+{old:mean}-{other}", "old", "new"),
              QString("{new}*2+{new:mean}-{other}"));
    // the new name may be one the old was a prefix of, and vice versa
    EXPECT_EQ(renameColumnRefs("{T}+{T2}", "T", "T2x"), QString("{T2x}+{T2}"));
    // text outside braces and incomplete spans are left alone
    EXPECT_EQ(renameColumnRefs("old + {old", "old", "new"), QString("old + {old"));
}

// per-column accessor constants
TEST(ColumnRefs, ColumnAccessor)
{
    const std::vector<double> col = {3.0, -1.0, 4.0, 2.0};
    EXPECT_DOUBLE_EQ(columnAccessor(col, "first"), 3.0);
    EXPECT_DOUBLE_EQ(columnAccessor(col, "last"), 2.0);
    EXPECT_DOUBLE_EQ(columnAccessor(col, "min"), -1.0);
    EXPECT_DOUBLE_EQ(columnAccessor(col, "max"), 4.0);
    EXPECT_DOUBLE_EQ(columnAccessor(col, "mean"), 2.0);
    EXPECT_TRUE(std::isnan(columnAccessor({}, "first")));
    EXPECT_TRUE(std::isnan(columnAccessor(col, "median")));
}

// a syntactically invalid expression is reported as an error
TEST(CustomFunc, InvalidSyntax)
{
    const CustomCurve c = evalCustomCurve("2*(x+", 0.0, 1.0, 4);
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.error.isEmpty());
    EXPECT_TRUE(c.points.isEmpty());
}

// referencing an undefined variable is an error (only the declared variable is set)
TEST(CustomFunc, UndefinedVariable)
{
    const CustomCurve c = evalCustomCurve("a*x + b", 0.0, 1.0, 4);
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.error.isEmpty());
    EXPECT_TRUE(c.points.isEmpty());
}

// nsamples below 1 is clamped to 1 (two endpoints)
TEST(CustomFunc, ClampsSampleCount)
{
    const CustomCurve c = evalCustomCurve("x", 0.0, 1.0, 0);
    ASSERT_TRUE(c.ok);
    ASSERT_EQ(c.points.size(), 2);
    EXPECT_DOUBLE_EQ(c.points[0].x(), 0.0);
    EXPECT_DOUBLE_EQ(c.points[1].x(), 1.0);
}

// ---- nonlinear fitting via fitCustomCurve --------------------------------

namespace {
// build (x, y) samples from a model functor over an index range
template <typename F>
void makeData(std::vector<double> &xs, std::vector<double> &ys, int npts, double x0, double dx,
              F model)
{
    xs.clear();
    ys.clear();
    for (int i = 0; i < npts; ++i) {
        const double x = x0 + dx * i;
        xs.push_back(x);
        ys.push_back(model(x));
    }
}
} // namespace

// fit an exponential decay A*exp(-k*x) and recover A and k
TEST(CustomFit, ExponentialDecay)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 20, 0.0, 0.2, [](double x) {
        return 5.0 * std::exp(-0.7 * x);
    });

    const QList<FitParam> init = {{"A", 1.0}, {"k", 0.1}};
    const CustomFit f          = fitCustomCurve("A*exp(-k*x)", init, xs, ys, 0.0, 3.8, 50);

    ASSERT_TRUE(f.ok) << f.error.toStdString();
    ASSERT_EQ(f.params.size(), 2);
    EXPECT_EQ(f.params[0].name, "A");
    EXPECT_EQ(f.params[1].name, "k");
    EXPECT_NEAR(f.params[0].value, 5.0, 1e-3);
    EXPECT_NEAR(f.params[1].value, 0.7, 1e-3);
    EXPECT_NEAR(f.rms, 0.0, 1e-4);
    EXPECT_EQ(f.curve.size(), 51);
}

// fit a quadratic with named coefficients
TEST(CustomFit, Quadratic)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 15, -2.0, 0.3, [](double x) {
        return 2.0 * x * x - 3.0 * x + 1.0;
    });

    const QList<FitParam> init = {{"a", 1.0}, {"b", 1.0}, {"c", 0.0}};
    const CustomFit f          = fitCustomCurve("a*x^2 + b*x + c", init, xs, ys, -2.0, 2.2, 10);

    ASSERT_TRUE(f.ok) << f.error.toStdString();
    ASSERT_EQ(f.params.size(), 3);
    EXPECT_NEAR(f.params[0].value, 2.0, 1e-4);
    EXPECT_NEAR(f.params[1].value, -3.0, 1e-4);
    EXPECT_NEAR(f.params[2].value, 1.0, 1e-4);
}

// a parameter that clashes with the independent variable is rejected
TEST(CustomFit, RejectsVariableClash)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 5, 0.0, 1.0, [](double x) {
        return x;
    });
    const QList<FitParam> init = {{"x", 1.0}};
    const CustomFit f          = fitCustomCurve("a*x", init, xs, ys, 0.0, 4.0, 4);
    EXPECT_FALSE(f.ok);
    EXPECT_FALSE(f.error.isEmpty());
}

// duplicate parameter names are rejected
TEST(CustomFit, RejectsDuplicateParam)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 5, 0.0, 1.0, [](double x) {
        return x;
    });
    const QList<FitParam> init = {{"a", 1.0}, {"a", 2.0}};
    const CustomFit f          = fitCustomCurve("a*x", init, xs, ys, 0.0, 4.0, 4);
    EXPECT_FALSE(f.ok);
    EXPECT_FALSE(f.error.isEmpty());
}

// an expression referencing an undeclared symbol fails with a message
TEST(CustomFit, RejectsUndeclaredSymbol)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 6, 0.0, 1.0, [](double x) {
        return x;
    });
    const QList<FitParam> init = {{"a", 1.0}};
    // 'q' is neither the variable nor a declared parameter
    const CustomFit f = fitCustomCurve("a*x + q", init, xs, ys, 0.0, 5.0, 5);
    EXPECT_FALSE(f.ok);
    EXPECT_FALSE(f.error.isEmpty());
}

// too few data points for the number of parameters is rejected
TEST(CustomFit, RejectsTooFewPoints)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 1, 0.0, 1.0, [](double x) {
        return x;
    });
    const QList<FitParam> init = {{"a", 1.0}, {"b", 1.0}};
    const CustomFit f          = fitCustomCurve("a*x + b", init, xs, ys, 0.0, 1.0, 4);
    EXPECT_FALSE(f.ok);
    EXPECT_FALSE(f.error.isEmpty());
}

// ---- weighted fitting ----------------------------------------------------

// A Maxwell-Boltzmann distribution sampled the way fix ave/histo writes one --
// binned, and cut off before its tail has died away -- is still recovered
// exactly, weighted or not: the shape inside the range fixes the parameters.
TEST(CustomFit, MaxwellBoltzmannIsRecovered)
{
    constexpr double kT = 0.5465; // kcal/mol, i.e. 275 K in real units
    std::vector<double> xs, ys;
    makeData(xs, ys, 100, 0.01, 0.02, [](double x) {
        return 0.3 * std::sqrt(x) * std::exp(-x / kT);
    });

    const QList<FitParam> init = {{"A", 1.0}, {"kT", 0.2}};
    std::vector<double> weights(ys.begin(), ys.end()); // by bin population

    const CustomFit plain = fitCustomCurve("A*sqrt(x)*exp(-x/kT)", init, xs, ys, 0.01, 1.99, 20);
    ASSERT_TRUE(plain.ok) << plain.error.toStdString();
    EXPECT_NEAR(plain.params[1].value, kT, 1e-6);

    const CustomFit weighted = fitCustomCurve("A*sqrt(x)*exp(-x/kT)", init, xs, ys, 0.01, 1.99, 20,
                                              QStringLiteral("x"), weights);
    ASSERT_TRUE(weighted.ok) << weighted.error.toStdString();
    EXPECT_NEAR(weighted.params[0].value, 0.3, 1e-6);
    EXPECT_NEAR(weighted.params[1].value, kT, 1e-6);
    // the reported residual stays unweighted, so the two remain comparable
    EXPECT_NEAR(weighted.rms, plain.rms, 1e-9);
}

// Weighting decides which part of the data a model that cannot describe all of
// it will follow.  Two half-ranges of different curvature, joined: weighting by
// the y values pulls the single fitted line towards the larger ones.
TEST(CustomFit, WeightsShiftTheSolution)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 21, 0.0, 0.5, [](double x) {
        return (x < 5.0) ? (10.0 - x) : (2.0 + 0.1 * x);
    });

    const QList<FitParam> init = {{"a", 1.0}, {"b", 1.0}};
    const CustomFit plain      = fitCustomCurve("a*x + b", init, xs, ys, 0.0, 10.0, 10);
    ASSERT_TRUE(plain.ok) << plain.error.toStdString();

    std::vector<double> weights(ys.begin(), ys.end());
    const CustomFit weighted =
        fitCustomCurve("a*x + b", init, xs, ys, 0.0, 10.0, 10, QStringLiteral("x"), weights);
    ASSERT_TRUE(weighted.ok) << weighted.error.toStdString();

    // the weighted fit sits closer to the high-value branch at x = 0 ...
    EXPECT_GT(weighted.params[1].value, plain.params[1].value);
    // ... and pays for it with a larger unweighted residual
    EXPECT_GT(weighted.rms, plain.rms);
}

// a wrongly sized weight vector is ignored rather than misapplied
TEST(CustomFit, WrongSizedWeightsAreIgnored)
{
    std::vector<double> xs, ys;
    makeData(xs, ys, 12, 0.0, 0.4, [](double x) {
        return 3.0 * x + 1.0;
    });
    const QList<FitParam> init  = {{"a", 0.0}, {"b", 0.0}};
    const std::vector<double> w = {1.0, 2.0, 3.0}; // shorter than the data
    const CustomFit f =
        fitCustomCurve("a*x + b", init, xs, ys, 0.0, 4.4, 10, QStringLiteral("x"), w);
    ASSERT_TRUE(f.ok) << f.error.toStdString();
    EXPECT_NEAR(f.params[0].value, 3.0, 1e-6);
    EXPECT_NEAR(f.params[1].value, 1.0, 1e-6);
}
