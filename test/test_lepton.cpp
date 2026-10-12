// Unit tests for the vendored LeptonMini expression library: parsing,
// evaluation, optimization, symbolic differentiation, and custom functions.
//
// Adapted from the LAMMPS test at unittest/utils/test_lepton.cpp. The
// LAMMPS-specific tests (lepton_utils, ZBLFunction) and the JIT-based
// CompiledExpression / CompiledVectorExpression tests were removed -- this
// vendored subset keeps only interpreted evaluation and symbolic
// differentiation -- and the namespace was changed Lepton -> LeptonMini.

#include "lepton_mini.h"

#include "gtest/gtest.h"

#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <sstream>
#include <string>

namespace {

// a custom function equal to f(x,y) = 2*x*y
class ExampleFunction : public LeptonMini::CustomFunction {
    [[nodiscard]] int getNumArguments() const override { return 2; }
    double evaluate(const double *arguments) const override
    {
        return 2.0 * arguments[0] * arguments[1];
    }
    double evaluateDerivative(const double *arguments, const int *derivOrder) const override
    {
        if (derivOrder[0] == 1) {
            if (derivOrder[1] == 0)
                return 2.0 * arguments[1];
            else if (derivOrder[1] == 1)
                return 2.0;
        }
        if (derivOrder[1] == 1 && derivOrder[0] == 0) return 2.0 * arguments[0];
        return 0.0;
    }
    [[nodiscard]] LeptonMini::CustomFunction *clone() const override
    {
        return new ExampleFunction();
    }
};

// verify a constant expression evaluates correctly via the parsed expression,
// its optimized form, and the interpreted ExpressionProgram
void verifyEvaluation(const std::string &expression, double expectedValue)
{
    std::map<std::string, LeptonMini::CustomFunction *> customFunctions;
    LeptonMini::ParsedExpression parsed = LeptonMini::Parser::parse(expression, customFunctions);
    ASSERT_NEAR(expectedValue, parsed.evaluate(), 1e-10);
    ASSERT_NEAR(expectedValue, parsed.optimize().evaluate(), 1e-10);

    LeptonMini::ExpressionProgram program = parsed.createProgram();
    ASSERT_NEAR(expectedValue, program.evaluate(), 1e-10);
}

// verify an expression with variables x and y evaluates correctly
void verifyEvaluation(const std::string &expression, double x, double y, double expectedValue)
{
    std::map<std::string, double> variables;
    variables["x"]                      = x;
    variables["y"]                      = y;
    LeptonMini::ParsedExpression parsed = LeptonMini::Parser::parse(expression);
    ASSERT_NEAR(expectedValue, parsed.evaluate(variables), 1e-10);
    ASSERT_NEAR(expectedValue, parsed.optimize().evaluate(variables), 1e-10);
    ASSERT_NEAR(expectedValue, parsed.optimize(variables).evaluate(), 1e-10);

    LeptonMini::ExpressionProgram program = parsed.createProgram();
    ASSERT_NEAR(expectedValue, program.evaluate(variables), 1e-10);

    // make sure variable renaming works
    variables.clear();
    variables["w"] = x;
    variables["y"] = y;
    std::map<std::string, std::string> replacements;
    replacements["x"] = "w";
    ASSERT_NEAR(expectedValue, parsed.renameVariables(replacements).evaluate(variables), 1e-10);
}

// confirm that an invalid expression throws on parse
void verifyInvalidExpression(const std::string &expression)
{
    try {
        LeptonMini::Parser::parse(expression);
    } catch (const std::exception &) {
        return;
    }
    throw std::exception();
}

void assertNumbersEqual(double val1, double val2, double tol = 1e-10)
{
    const double inf = std::numeric_limits<double>::infinity();
    if (val1 == val1 || val2 == val2)           // if both are NaN, that's fine
        if (val1 != inf || val2 != inf)         // both infinity is also fine
            if (val1 != -inf || val2 != -inf) { // same for -infinity
                ASSERT_NEAR(val1, val2, tol);
            }
}

// verify two expressions evaluate to the same value at (x, y)
void verifySameValue(const LeptonMini::ParsedExpression &exp1,
                     const LeptonMini::ParsedExpression &exp2, double x, double y)
{
    std::map<std::string, double> variables;
    variables["x"] = x;
    variables["y"] = y;
    assertNumbersEqual(exp1.evaluate(variables), exp2.evaluate(variables));
}

// verify the analytic derivative of an expression matches the expected one
void verifyDerivative(const std::string &expression, const std::string &expectedDeriv)
{
    LeptonMini::ParsedExpression computed =
        LeptonMini::Parser::parse(expression).differentiate("x").optimize();
    LeptonMini::ParsedExpression expected = LeptonMini::Parser::parse(expectedDeriv);
    verifySameValue(computed, expected, 1.0, 2.0);
    verifySameValue(computed, expected, 2.0, 3.0);
    verifySameValue(computed, expected, -2.0, 3.0);
    verifySameValue(computed, expected, 2.0, -3.0);
    verifySameValue(computed, expected, 0.0, -3.0);
    verifySameValue(computed, expected, 2.0, 0.0);
}

// verify a custom function (and its derivatives) matches an equivalent expression
void testCustomFunction(const std::string &expression, const std::string &equivalent)
{
    std::map<std::string, LeptonMini::CustomFunction *> functions;
    ExampleFunction exp;
    functions["custom"]               = &exp;
    LeptonMini::ParsedExpression exp1 = LeptonMini::Parser::parse(expression, functions);
    LeptonMini::ParsedExpression exp2 = LeptonMini::Parser::parse(equivalent);
    verifySameValue(exp1, exp2, 1.0, 2.0);
    verifySameValue(exp1, exp2, 2.0, 3.0);
    verifySameValue(exp1, exp2, -2.0, 3.0);
    verifySameValue(exp1, exp2, 2.0, -3.0);
    LeptonMini::ParsedExpression deriv1 = exp1.differentiate("x").optimize();
    LeptonMini::ParsedExpression deriv2 = exp2.differentiate("x").optimize();
    verifySameValue(deriv1, deriv2, 1.0, 2.0);
    verifySameValue(deriv1, deriv2, 2.0, 3.0);
    verifySameValue(deriv1, deriv2, -2.0, 3.0);
    verifySameValue(deriv1, deriv2, 2.0, -3.0);
    LeptonMini::ParsedExpression deriv3 = deriv1.differentiate("y").optimize();
    LeptonMini::ParsedExpression deriv4 = deriv2.differentiate("y").optimize();
    verifySameValue(deriv3, deriv4, 1.0, 2.0);
    verifySameValue(deriv3, deriv4, 2.0, 3.0);
    verifySameValue(deriv3, deriv4, -2.0, 3.0);
    verifySameValue(deriv3, deriv4, 2.0, -3.0);
}

} // namespace

TEST(Lepton, Evaluation)
{
    verifyEvaluation("5", 5.0);
    verifyEvaluation("5*2", 10.0);
    verifyEvaluation("2*3+4*5", 26.0);
    verifyEvaluation("2^-3", 0.125);
    verifyEvaluation("1e+2", 100.0);
    verifyEvaluation("-x", 2.0, 3.0, -2.0);
    verifyEvaluation("y^-x", 3.0, 2.0, 0.125);
    verifyEvaluation("1/-x", 3.0, 2.0, -1.0 / 3.0);
    verifyEvaluation("2.1e-4*x*(y+1)", 3.0, 1.0, 1.26e-3);
    verifyEvaluation("sin(2.5)", std::sin(2.5));
    verifyEvaluation("cot(x)", 3.0, 1.0, 1.0 / std::tan(3.0));
    verifyEvaluation("log(x)", 3.0, 1.0, std::log(3.0));
    verifyEvaluation("x^2+y^3+x^-1+y^(1/2)", 1.0, 1.0, 4.0);
    verifyEvaluation("(2*x)*3", 4.0, 4.0, 24.0);
    verifyEvaluation("(x*2)*3", 4.0, 4.0, 24.0);
    verifyEvaluation("2*(x*3)", 4.0, 4.0, 24.0);
    verifyEvaluation("2*(3*x)", 4.0, 4.0, 24.0);
    verifyEvaluation("2*x/3", 1.0, 4.0, 2.0 / 3.0);
    verifyEvaluation("x*2/3", 1.0, 4.0, 2.0 / 3.0);
    verifyEvaluation("5*(-x)*(-y)", 1.0, 4.0, 20.0);
    verifyEvaluation("5*(-x)*(y)", 1.0, 4.0, -20.0);
    verifyEvaluation("5*(x)*(-y)", 1.0, 4.0, -20.0);
    verifyEvaluation("5*(-x)/(-y)", 1.0, 4.0, 1.25);
    verifyEvaluation("5*(-x)/(y)", 1.0, 4.0, -1.25);
    verifyEvaluation("5*(x)/(-y)", 1.0, 4.0, -1.25);
    verifyEvaluation("x+(-y)", 1.0, 4.0, -3.0);
    verifyEvaluation("(-x)+y", 1.0, 4.0, 3.0);
    verifyEvaluation("x/(1/y)", 1.0, 4.0, 4.0);
    verifyEvaluation("x*w; w = 5", 3.0, 1.0, 15.0);
    verifyEvaluation("a+b^2;a=x-b;b=3*y", 2.0, 3.0, 74.0);
    verifyEvaluation("erf(x)+erfc(x)", 2.0, 3.0, 1.0);
    verifyEvaluation("min(3, x)", 2.0, 3.0, 2.0);
    verifyEvaluation("min(y, 5)", 2.0, 3.0, 3.0);
    verifyEvaluation("max(x, y)", 2.0, 3.0, 3.0);
    verifyEvaluation("max(x, -1)", 2.0, 3.0, 2.0);
    verifyEvaluation("abs(x-y)", 2.0, 3.0, 1.0);
    verifyEvaluation("delta(x)+3*delta(y-1.5)", 2.0, 1.5, 3.0);
    verifyEvaluation("step(x-3)+y*step(x)", 2.0, 3.0, 3.0);
    verifyEvaluation("floor(x)", -2.1, 3.0, -3.0);
    verifyEvaluation("ceil(x)", -2.1, 3.0, -2.0);
    verifyEvaluation("select(x, 1.0, y)", 0.3, 2.0, 1.0);
    verifyEvaluation("select(x, 1.0, y)", 0.0, 2.0, 2.0);
    verifyEvaluation("atan2(x, y)", 3.0, 1.5, std::atan(2.0));
    verifyEvaluation("sqrt(x^2)", -2.2, 0.0, 2.2);
    verifyEvaluation("sqrt(x)^2", 2.2, 0.0, 2.2);
    verifyEvaluation("x^2+x^4", 2.0, 0.0, 20.0);
    verifyEvaluation("x^-2+x^-3", 2.0, 0.0, 0.375);
    verifyEvaluation("x^1.8", 2.2, 0.0, std::pow(2.2, 1.8));
}

TEST(Lepton, InvalidEvaluation)
{
    ASSERT_NO_THROW(verifyInvalidExpression("1..2"));
    ASSERT_NO_THROW(verifyInvalidExpression("1*(2+3"));
    ASSERT_NO_THROW(verifyInvalidExpression("5++4"));
    ASSERT_NO_THROW(verifyInvalidExpression("1+2)"));
    ASSERT_NO_THROW(verifyInvalidExpression("cos(2,3)"));
}

TEST(Lepton, VerifyDerivative)
{
    verifyDerivative("x", "1");
    verifyDerivative("x^2+x", "2*x+1");
    verifyDerivative("y^x-x", "log(y)*(y^x)-1");
    verifyDerivative("sin(x)", "cos(x)");
    verifyDerivative("cos(x)", "-sin(x)");
    verifyDerivative("tan(x)", "square(sec(x))");
    verifyDerivative("cot(x)", "-square(csc(x))");
    verifyDerivative("sec(x)", "sec(x)*tan(x)");
    verifyDerivative("csc(x)", "-csc(x)*cot(x)");
    verifyDerivative("exp(2*x)", "2*exp(2*x)");
    verifyDerivative("log(x)", "1/x");
    verifyDerivative("sqrt(x)", "0.5/sqrt(x)");
    verifyDerivative("asin(x)", "1/sqrt(1-x^2)");
    verifyDerivative("acos(x)", "-1/sqrt(1-x^2)");
    verifyDerivative("atan(x)", "1/(1+x^2)");
    verifyDerivative("atan2(2*x,y)", "2*y/(4*x^2+y^2)");
    verifyDerivative("sinh(x)", "cosh(x)");
    verifyDerivative("cosh(x)", "sinh(x)");
    verifyDerivative("tanh(x)", "1/(cosh(x)^2)");
    verifyDerivative("erf(x)", "1.12837916709551*exp(-x^2)");
    verifyDerivative("erfc(x)", "-1.12837916709551*exp(-x^2)");
    verifyDerivative("step(x)*x+step(1-x)*2*x", "step(x)+step(1-x)*2");
    verifyDerivative("recip(x)", "-1/x^2");
    verifyDerivative("square(x)", "2*x");
    verifyDerivative("cube(x)", "3*x^2");
    verifyDerivative("min(x, 2*x)", "step(x-2*x)*2+(1-step(x-2*x))*1");
    verifyDerivative("max(5, x^2)", "(1-step(5-x^2))*2*x");
    verifyDerivative("abs(3*x)", "step(3*x)*3+(1-step(3*x))*-3");
    verifyDerivative("floor(x)+0.5*x*ceil(x)", "0.5*ceil(x)");
    verifyDerivative("select(x, x^2, 3*x)", "select(x, 2*x, 3)");
}

TEST(Lepton, CustomFunction)
{
    testCustomFunction("custom(x, y)/2", "x*y");
    testCustomFunction("custom(x^2, 1)+custom(2, y-1)", "2*x^2+4*(y-1)");
}

TEST(Lepton, Optimize)
{
    std::stringstream out;
    out << LeptonMini::Parser::parse("x*x").optimize();
    EXPECT_EQ(out.str(), "square(x)");
    out.str("");

    out << LeptonMini::Parser::parse("x*x*x").optimize();
    EXPECT_EQ(out.str(), "cube(x)");
    out.str("");

    out << LeptonMini::Parser::parse("x*(x*x)").optimize();
    EXPECT_EQ(out.str(), "cube(x)");
    out.str("");

    out << LeptonMini::Parser::parse("(x*x)*x").optimize();
    EXPECT_EQ(out.str(), "cube(x)");
    out.str("");

    out << LeptonMini::Parser::parse("2*3*x").optimize();
    EXPECT_EQ(out.str(), "6*(x)");
    out.str("");

    out << LeptonMini::Parser::parse("1/(1+x)").optimize();
    EXPECT_EQ(out.str(), "recip(1+(x))");
    out.str("");

    out << LeptonMini::Parser::parse("x^(1/2)").optimize();
    EXPECT_EQ(out.str(), "sqrt(x)");
    out.str("");

    out << LeptonMini::Parser::parse("log(3*cos(x))^(sqrt(4)-2)").optimize();
    EXPECT_EQ(out.str(), "1");
    out.str("");
}
