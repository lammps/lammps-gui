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

#include "customfunc.h"

#include "lepton_mini.h"
#include "levmar.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <string>
#include <vector>

CompiledExpression::CompiledExpression(const QString &expression)
{
    try {
        // parse once, optimize, and compile to an interpreted program so the
        // per-point evaluation in the caller's loop stays cheap
        LeptonMini::ParsedExpression parsed =
            LeptonMini::Parser::parse(expression.toStdString()).optimize();
        program = std::make_unique<LeptonMini::ExpressionProgram>(parsed.createProgram());
        valid   = true;
    } catch (const std::exception &e) {
        errorMsg = QString::fromStdString(e.what());
        valid    = false;
    }
}

CompiledExpression::~CompiledExpression() = default;

double CompiledExpression::evaluate(const std::map<std::string, double> &variables) const
{
    return program->evaluate(variables);
}

// The accessors a colon inside a braced column reference may select.
static const QStringList known_accessors = {QStringLiteral("first"), QStringLiteral("last"),
                                            QStringLiteral("min"), QStringLiteral("max"),
                                            QStringLiteral("mean")};

// The available-columns list appended to a failed lookup.  Names that cannot
// be referenced anyway (colon or brace in the name) are left out.
static QString availableColumns(const QStringList &names)
{
    QStringList usable;
    for (const auto &n : names)
        if (!n.contains(':') && !n.contains('{') && !n.contains('}')) usable << ('{' + n + '}');
    if (usable.isEmpty()) return QStringLiteral("No column can currently be referenced.");
    return QStringLiteral("Available: %1").arg(usable.join(QStringLiteral(", ")));
}

ColumnRefExpr substituteColumnRefs(const QString &expr, const QStringList &names)
{
    ColumnRefExpr result;
    result.expr.reserve(expr.size());

    int pos = 0;
    while (pos < expr.size()) {
        const QChar c = expr.at(pos);
        if (c == '}') {
            result.error = QStringLiteral("Unmatched '}' in the expression.");
            return result;
        }
        if (c != '{') {
            result.expr += c;
            ++pos;
            continue;
        }

        const int close = expr.indexOf('}', pos + 1);
        if (close < 0) {
            result.error = QStringLiteral("Unmatched '{' in the expression.");
            return result;
        }
        const QString span = expr.mid(pos + 1, close - pos - 1);
        if (span.trimmed().isEmpty()) {
            result.error = QStringLiteral("Empty column reference {}.");
            return result;
        }

        // a colon is always the accessor separator; the name part is the span
        // up to the last colon so the error for a colon-in-name column is exact
        const int colon        = span.lastIndexOf(':');
        const QString name     = (colon < 0) ? span : span.left(colon);
        const QString accessor = (colon < 0) ? QString() : span.mid(colon + 1);

        const int column = names.indexOf(name);
        if (column < 0) {
            if (names.contains(span))
                result.error = QStringLiteral("Column '%1' cannot be referenced because its "
                                              "name contains ':'; rename the column first.")
                                   .arg(span);
            else
                result.error =
                    QStringLiteral("No column named '%1'.\n%2").arg(name, availableColumns(names));
            return result;
        }
        if ((colon >= 0) && !known_accessors.contains(accessor)) {
            result.error = QStringLiteral("Unknown accessor ':%1' in '{%2}'; one of "
                                          ":first, :last, :min, :max, :mean.")
                               .arg(accessor, span);
            return result;
        }

        // one generated variable per distinct (column, accessor) pair
        int ref = 0;
        for (; ref < result.refs.size(); ++ref)
            if ((result.refs[ref].column == column) && (result.refs[ref].accessor == accessor))
                break;
        if (ref == result.refs.size()) {
            const QString variable = QStringLiteral("_col%1_").arg(ref);
            result.refs.append(ColumnRef{column, accessor, variable});
        }
        result.expr += result.refs[ref].variable;
        pos = close + 1;
    }

    result.ok = true;
    return result;
}

QString renameColumnRefs(const QString &expr, const QString &oldName, const QString &newName)
{
    QString result;
    result.reserve(expr.size());

    int pos = 0;
    while (pos < expr.size()) {
        const QChar c   = expr.at(pos);
        const int close = (c == '{') ? expr.indexOf('}', pos + 1) : -1;
        if (close < 0) { // not a complete braced span; leave for substitution to flag
            result += c;
            ++pos;
            continue;
        }
        QString span      = expr.mid(pos + 1, close - pos - 1);
        const int colon   = span.lastIndexOf(':');
        const QString ref = (colon < 0) ? span : span.left(colon);
        if (ref == oldName) span.replace(0, ref.size(), newName);
        result += '{' + span + '}';
        pos = close + 1;
    }
    return result;
}

double columnAccessor(const std::vector<double> &column, const QString &accessor)
{
    if (column.empty()) return std::numeric_limits<double>::quiet_NaN();
    if (accessor == QStringLiteral("first")) return column.front();
    if (accessor == QStringLiteral("last")) return column.back();
    if (accessor == QStringLiteral("min")) return *std::min_element(column.begin(), column.end());
    if (accessor == QStringLiteral("max")) return *std::max_element(column.begin(), column.end());
    if (accessor == QStringLiteral("mean")) {
        double sum = 0.0;
        for (double v : column)
            sum += v;
        return sum / static_cast<double>(column.size());
    }
    return std::numeric_limits<double>::quiet_NaN();
}

CustomCurve evalCustomCurve(const QString &expression, double xmin, double xmax, int nsamples,
                            const QString &variable)
{
    CustomCurve result;

    const QString expr = expression.trimmed();
    if (expr.isEmpty()) {
        result.error = QStringLiteral("The expression is empty.");
        return result;
    }
    if (nsamples < 1) nsamples = 1;

    const std::string var = variable.toStdString();

    CompiledExpression program(expr);
    if (!program.isValid()) {
        result.error = program.error();
        return result;
    }

    try {
        std::map<std::string, double> vars;
        for (int k = 0; k <= nsamples; ++k) {
            const double x = xmin + (xmax - xmin) * static_cast<double>(k) / nsamples;
            vars[var]      = x;
            const double y = program.evaluate(vars);
            if (std::isfinite(y)) result.points.append(QPointF(x, y));
        }
    } catch (const std::exception &e) {
        result.error = QString::fromStdString(e.what());
        result.points.clear();
        return result;
    }

    result.ok = true;
    return result;
}

CustomFit fitCustomCurve(const QString &expression, const QList<FitParam> &initialParams,
                         const std::vector<double> &xdata, const std::vector<double> &ydata,
                         double xmin, double xmax, int nsamples, const QString &variable,
                         const std::vector<double> &weights)
{
    CustomFit result;

    const QString expr = expression.trimmed();
    if (expr.isEmpty()) {
        result.error = QStringLiteral("The expression is empty.");
        return result;
    }
    if (initialParams.isEmpty()) {
        result.error = QStringLiteral("No fit parameters were given.");
        return result;
    }
    if (xdata.size() != ydata.size()) {
        result.error = QStringLiteral("The x and y data have different lengths.");
        return result;
    }

    const int m = static_cast<int>(xdata.size());
    const int n = initialParams.size();
    if (m < n) {
        result.error = QStringLiteral("Too few data points (%1) for %2 parameters.").arg(m).arg(n);
        return result;
    }
    if (nsamples < 1) nsamples = 1;

    const std::string var = variable.toStdString();

    // collect parameter names; reject empties, duplicates, and clashes with the
    // independent variable
    std::vector<std::string> pnames;
    pnames.reserve(n);
    for (const FitParam &p : initialParams) {
        const QString nm = p.name.trimmed();
        if (nm.isEmpty()) {
            result.error = QStringLiteral("A fit parameter has an empty name.");
            return result;
        }
        if (nm == variable) {
            result.error =
                QStringLiteral("Parameter name '%1' clashes with the variable name.").arg(nm);
            return result;
        }
        const std::string s = nm.toStdString();
        for (const auto &e : pnames) {
            if (e == s) {
                result.error = QStringLiteral("Duplicate parameter name '%1'.").arg(nm);
                return result;
            }
        }
        pnames.push_back(s);
    }

    try {
        // parse once; build the optimized model and the analytic derivative with
        // respect to each parameter (the Jacobian columns)
        const LeptonMini::ParsedExpression base  = LeptonMini::Parser::parse(expr.toStdString());
        const LeptonMini::ParsedExpression model = base.optimize();
        std::vector<LeptonMini::ParsedExpression> derivs;
        derivs.reserve(n);
        for (const auto &s : pnames)
            derivs.push_back(base.differentiate(s).optimize());

        // pre-flight evaluation at the initial guess so an expression referencing
        // an undeclared symbol fails with a descriptive LeptonMini message
        std::map<std::string, double> probe;
        probe[var] = xdata.empty() ? 0.0 : xdata.front();
        for (int j = 0; j < n; ++j)
            probe[pnames[j]] = initialParams[j].value;
        (void)model.evaluate(probe);

        // A weighted least-squares problem is the unweighted one on residuals
        // and Jacobian rows scaled by sqrt(w), so the solver itself needs to
        // know nothing about weights.
        const bool weighted = (weights.size() == static_cast<std::size_t>(m));
        std::vector<double> sqrtw;
        if (weighted) {
            sqrtw.reserve(weights.size());
            for (double w : weights)
                sqrtw.push_back(std::sqrt(qMax(0.0, w)));
        }

        // residual/Jacobian callback for the Levenberg-Marquardt solver
        const LevmarModel fn = [&](const std::vector<double> &p, std::vector<double> &res,
                                   std::vector<std::vector<double>> &jac) -> bool {
            std::map<std::string, double> vars;
            for (int j = 0; j < n; ++j)
                vars[pnames[j]] = p[j];
            try {
                for (int i = 0; i < m; ++i) {
                    vars[var]            = xdata[i];
                    const double modeled = model.evaluate(vars);
                    if (!std::isfinite(modeled)) return false;
                    const double sw = weighted ? sqrtw[i] : 1.0;
                    res[i]          = (modeled - ydata[i]) * sw;
                    for (int j = 0; j < n; ++j) {
                        const double d = derivs[j].evaluate(vars);
                        if (!std::isfinite(d)) return false;
                        jac[i][j] = d * sw;
                    }
                }
            } catch (const std::exception &) {
                return false;
            }
            return true;
        };

        std::vector<double> initial(n, 0.0);
        for (int j = 0; j < n; ++j)
            initial[j] = initialParams[j].value;

        const LevmarResult lm = levmarFit(m, n, initial, fn);
        if (!lm.ok) {
            result.error = QString::fromStdString(lm.message);
            return result;
        }

        // report fitted parameters in the input order
        for (int j = 0; j < n; ++j)
            result.params.append(FitParam{initialParams[j].name.trimmed(), lm.params[j]});

        // sample the fitted model over the requested range
        std::map<std::string, double> fitted;
        for (int j = 0; j < n; ++j)
            fitted[pnames[j]] = lm.params[j];
        for (int k = 0; k <= nsamples; ++k) {
            const double x = xmin + (xmax - xmin) * static_cast<double>(k) / nsamples;
            fitted[var]    = x;
            const double y = model.evaluate(fitted);
            if (std::isfinite(y)) result.curve.append(QPointF(x, y));
        }

        // the solver's residual is the weighted one, whose scale depends on the
        // weights; report the plain one so fits stay comparable across them
        if (weighted) {
            double sum = 0.0;
            for (int i = 0; i < m; ++i) {
                fitted[var]    = xdata[i];
                const double d = model.evaluate(fitted) - ydata[i];
                sum += d * d;
            }
            result.rms = std::sqrt(sum / static_cast<double>(m));
        } else {
            result.rms = lm.rms;
        }
        result.iterations = lm.iterations;
        result.ok         = true;
    } catch (const std::exception &e) {
        result.error = QString::fromStdString(e.what());
        result.params.clear();
        result.curve.clear();
        result.ok = false;
        return result;
    }

    return result;
}

// Local Variables:
// c-basic-offset: 4
// End:
