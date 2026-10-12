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

#ifndef CUSTOMFUNC_H
#define CUSTOMFUNC_H

// Evaluate user-supplied mathematical expressions (via the vendored LeptonMini
// parser) for custom-function plotting in the chart post-processing dialog and
// for the derived-column expressions of the data-import dialog, including the
// {name} column-reference syntax those expressions use.
// The interface is QString in / QString out so call sites stay free of
// std::string conversions; the LeptonMini (std::string) boundary is confined
// to the implementation, mirroring how LammpsWrapper confines the LAMMPS C API.

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace LeptonMini {
class ExpressionProgram;
}

/**
 * @brief A parsed and compiled LeptonMini expression with QString error reporting
 *
 * Confines the LeptonMini (std::string) parsing boundary to one translation
 * unit, the way LammpsWrapper confines the LAMMPS C API. Construct from a
 * QString expression, check @ref isValid / @ref error, then call @ref evaluate
 * repeatedly with a name -> value variable map. @ref evaluate propagates the
 * LeptonMini exception thrown for an unbound variable, so callers that may
 * reference variables not present in the map should evaluate inside a try block.
 */
class CompiledExpression {
public:
    /** @brief Parse, optimize, and compile @p expression (a LeptonMini string) */
    explicit CompiledExpression(const QString &expression);
    ~CompiledExpression();
    CompiledExpression()                                      = delete;
    CompiledExpression(const CompiledExpression &)            = delete;
    CompiledExpression(CompiledExpression &&)                 = delete;
    CompiledExpression &operator=(const CompiledExpression &) = delete;
    CompiledExpression &operator=(CompiledExpression &&)      = delete;

    /** @brief True if the expression parsed and compiled successfully */
    bool isValid() const { return valid; }
    /** @brief Parse-error message (empty when @ref isValid is true) */
    const QString &error() const { return errorMsg; }
    /** @brief Evaluate with the given variable bindings (may throw on unbound vars) */
    double evaluate(const std::map<std::string, double> &variables) const;

private:
    std::unique_ptr<LeptonMini::ExpressionProgram> program; ///< compiled program (null if invalid)
    bool valid = false;                                     ///< parse/compile succeeded
    QString errorMsg;                                       ///< parse error (empty when valid)
};

/**
 * @brief One resolved <tt>{name}</tt> or <tt>{name:accessor}</tt> column reference
 *
 * Produced by substituteColumnRefs(). An empty @ref accessor means the
 * reference stands for the column's value in the current row and must be
 * rebound for every row; otherwise it is one of the per-column constants
 * ("first", "last", "min", "max", "mean") and can be bound once.
 */
struct ColumnRef {
    int column = -1;  ///< index of the referenced column in the name list
    QString accessor; ///< empty = current-row value, else the accessor name
    QString variable; ///< generated variable the braced span was replaced with
};

/**
 * @brief Result of rewriting the <tt>{name}</tt> column references of an expression
 */
struct ColumnRefExpr {
    bool ok = false;       ///< true if every braced span resolved to a column
    QString error;         ///< human-readable error message when @ref ok is false
    QString expr;          ///< rewritten expression ready for the Lepton parser
    QList<ColumnRef> refs; ///< the distinct references, one per generated variable
};

/**
 * @brief Replace <tt>{name}</tt> column references with generated Lepton variables
 *
 * Column values are referenced in braces, like Python format strings: @c {name}
 * is the column's value in the current row, and @c {name:first}, @c {name:last},
 * @c {name:min}, @c {name:max}, and @c {name:mean} are per-column constants.
 * A colon inside braces is always the accessor separator, so a column whose
 * name contains a colon (or a brace) cannot be referenced; the dialogs refuse
 * such names for user-entered columns, and a file-supplied one has to be
 * renamed before it can be used.  Text outside braces is passed through
 * untouched and is never a column lookup.
 *
 * Each distinct (column, accessor) pair is replaced by one generated variable
 * and reported in @ref ColumnRefExpr::refs so the caller can bind it.  An
 * unknown name, an unknown accessor, an empty span, or an unmatched brace
 * fails with a message naming the offender and the available columns.
 *
 * @param expr  Expression with braced column references
 * @param names Column names, referenced by exact (case-sensitive) match
 * @return Rewritten expression plus the references, or an error
 */
ColumnRefExpr substituteColumnRefs(const QString &expr, const QStringList &names);

/**
 * @brief Rewrite the <tt>{name}</tt> references of a renamed column
 *
 * Replaces @c {oldName} and @c {oldName:accessor} spans with the same
 * reference to @p newName, leaving everything else (including references to
 * other columns) untouched.  This is what keeps stored derived-column
 * expressions valid when a column is renamed.
 *
 * @param expr    Expression with braced column references
 * @param oldName Column name to rewrite
 * @param newName Replacement column name
 * @return The rewritten expression
 */
QString renameColumnRefs(const QString &expr, const QString &oldName, const QString &newName);

/**
 * @brief Evaluate a per-column accessor constant
 *
 * @param column   Column values
 * @param accessor One of "first", "last", "min", "max", "mean"
 * @return The accessor value, or NaN for an empty column or unknown accessor
 */
double columnAccessor(const std::vector<double> &column, const QString &accessor);

/**
 * @brief Result of sampling a custom expression over an x range
 */
struct CustomCurve {
    bool ok = false;       ///< true if the expression parsed and evaluated
    QString error;         ///< human-readable error message when @ref ok is false
    QList<QPointF> points; ///< sampled (x, y) points; non-finite y values are skipped
};

/**
 * @brief A named nonlinear-fit parameter
 *
 * Carries the initial guess on input to fitCustomCurve() and the fitted
 * value on output.
 */
struct FitParam {
    QString name;       ///< parameter name as it appears in the expression
    double value = 0.0; ///< initial guess (input) / fitted value (output)
};

/**
 * @brief Result of a nonlinear least-squares fit of a custom expression
 */
struct CustomFit {
    bool ok = false;        ///< true if the fit produced a usable solution
    QString error;          ///< human-readable error message when @ref ok is false
    QList<FitParam> params; ///< fitted parameters, in the input order
    QList<QPointF> curve;   ///< fitted model sampled over the x range
    double rms     = 0.0;   ///< root-mean-square residual at the solution
    int iterations = 0;     ///< Levenberg-Marquardt iterations performed
};

/**
 * @brief Parse and evaluate a single-variable expression over an x range
 *
 * Parses @p expression with the vendored LeptonMini parser, optimizes it, and
 * evaluates it at @p nsamples + 1 equally spaced points spanning
 * [@p xmin, @p xmax]. Points whose value is not finite (NaN/inf) are omitted.
 *
 * @param expression Math expression in the variable @p variable (LeptonMini syntax)
 * @param xmin       Lower bound of the sampling range
 * @param xmax       Upper bound of the sampling range
 * @param nsamples   Number of sub-intervals (clamped to >= 1); nsamples+1 points
 * @param variable   Name of the independent variable in @p expression (default "x")
 * @return Curve result; on a parse/evaluation error @ref CustomCurve::ok is
 *         false and @ref CustomCurve::error describes the problem
 */
CustomCurve evalCustomCurve(const QString &expression, double xmin, double xmax, int nsamples,
                            const QString &variable = QStringLiteral("x"));

/**
 * @brief Nonlinear least-squares fit of a custom expression to (x, y) data
 *
 * Fits @p expression -- a function of the independent variable @p variable and
 * the named parameters in @p initialParams -- to the data (@p xdata, @p ydata)
 * by Levenberg-Marquardt. The Jacobian is built from analytic derivatives of
 * the expression with respect to each parameter (via LeptonMini's symbolic
 * differentiation). On success the fitted model is sampled at @p nsamples + 1
 * points over [@p xmin, @p xmax] for overlaying on the chart.
 *
 * @param expression    Math expression in @p variable and the parameter names
 * @param initialParams Parameters with their initial guesses (at least one)
 * @param xdata         Independent-variable data
 * @param ydata         Dependent-variable data (same length as @p xdata)
 * @param xmin          Lower bound for sampling the fitted curve
 * @param xmax          Upper bound for sampling the fitted curve
 * @param nsamples      Number of sub-intervals (clamped to >= 1); nsamples+1 points
 * @param variable      Name of the independent variable (default "x")
 * @param weights       Optional per-point weights w_i for a weighted fit, which
 *                      minimizes sum w_i (model_i - y_i)^2.  Empty (the default)
 *                      or a wrongly sized vector means every point counts the
 *                      same; negative entries are treated as zero.  Weighting
 *                      decides which part of the data a model that cannot
 *                      describe all of it will follow
 * @return Fit result; on a parse/dimension/evaluation error @ref CustomFit::ok
 *         is false and @ref CustomFit::error describes the problem.
 *         @ref CustomFit::rms is the plain, unweighted residual either way, so
 *         that fits with different weightings stay comparable
 */
CustomFit fitCustomCurve(const QString &expression, const QList<FitParam> &initialParams,
                         const std::vector<double> &xdata, const std::vector<double> &ydata,
                         double xmin, double xmax, int nsamples,
                         const QString &variable            = QStringLiteral("x"),
                         const std::vector<double> &weights = {});

#endif

// Local Variables:
// c-basic-offset: 4
// End:
