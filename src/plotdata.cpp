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

#include "plotdata.h"
#include "plotdata_internal.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringView>
#include <utility>

void PlotData::setColumnNames(const QStringList &columnNames)
{
    names = columnNames;
    cols.assign(columnNames.size(), {});
}

void PlotData::renameColumns(const QStringList &newNames)
{
    for (int i = 0; i < newNames.size() && i < static_cast<int>(names.size()); ++i)
        names[i] = newNames[i];
}

bool PlotData::appendRow(const std::vector<double> &row)
{
    if (static_cast<int>(row.size()) != columnCount()) return false;
    for (int c = 0; c < columnCount(); ++c)
        cols[c].push_back(row[c]);
    return true;
}

void PlotData::addColumn(const QString &name, std::vector<double> data)
{
    names << name;
    cols.push_back(std::move(data));
}

/* -------------------------------------------------------------------- */

QString unquote(QString t)
{
    t = t.trimmed();
    if (t.size() >= 2) {
        const QChar f = t.front();
        const QChar l = t.back();
        if ((f == l) && ((f == '\'') || (f == '"'))) t = t.mid(1, t.size() - 2);
    }
    return t.trimmed();
}

QString bracketContents(const QString &s)
{
    const int a = s.indexOf('[');
    const int b = s.lastIndexOf(']');
    if ((a < 0) || (b < 0) || (b <= a)) return {};
    return s.mid(a + 1, b - a - 1);
}

QStringList genericColumnNames(int ncol)
{
    QStringList names;
    names.reserve(ncol);
    for (int i = 0; i < ncol; ++i)
        names << QStringLiteral("column%1").arg(i + 1);
    return names;
}

const QRegularExpression &whitespaceRe()
{
    static const QRegularExpression re("\\s+");
    return re;
}

bool numericFields(QStringView line, std::vector<double> &row)
{
    row.clear();
    const qsizetype n = line.size();
    qsizetype i       = 0;
    while (i < n) {
        while ((i < n) && line[i].isSpace())
            ++i;
        if (i >= n) break;
        qsizetype j = i;
        while ((j < n) && !line[j].isSpace())
            ++j;
        bool good      = false;
        const double d = line.mid(i, j - i).toDouble(&good);
        if (!good) return false;
        row.push_back(d);
        i = j;
    }
    return !row.empty();
}

bool anyLineStartsWith(const QString &text, QLatin1String prefix)
{
    QStringView rest(text);
    while (!rest.isEmpty()) {
        const qsizetype nl     = rest.indexOf(u'\n');
        const QStringView line = (nl < 0 ? rest : rest.left(nl)).trimmed();
        if (line.startsWith(prefix)) return true;
        if (nl < 0) break;
        rest = rest.mid(nl + 1);
    }
    return false;
}

namespace {

// Heuristic: does the text look like YAML tabular data?  We accept either the
// LAMMPS thermo format (has a "keywords:" line) or a generic sequence of maps
// (has a "- {key: value, ...}" line with at least one colon inside the braces).
bool looksLikeYaml(const QString &text)
{
    QStringView rest(text);
    while (!rest.isEmpty()) {
        const qsizetype nl  = rest.indexOf(u'\n');
        const QStringView t = (nl < 0 ? rest : rest.left(nl)).trimmed();
        if (t.startsWith(QLatin1String("keywords:"))) return true;
        if (t.startsWith(QLatin1String("- {")) && t.endsWith(u'}') && t.contains(u':')) return true;
        if (nl < 0) break;
        rest = rest.mid(nl + 1);
    }
    return false;
}

// Heuristic: does the text start like a JSON document?
bool looksLikeJson(const QString &text)
{
    const QString t = text.trimmed();
    return t.startsWith('[') || t.startsWith('{');
}

// Convert a list of string tokens to a numeric row. Returns false (and leaves
// @p row in an unspecified state) on the first token that is not a number. When
// @p skipEmpty is true, empty tokens are ignored rather than treated as errors
// (tolerates the trailing comma LAMMPS writes in YAML rows).
bool tokensToRow(const QStringList &toks, std::vector<double> &row, bool skipEmpty = false)
{
    row.clear();
    row.reserve(toks.size());
    for (const QString &t : toks) {
        const QString tok = skipEmpty ? t.trimmed() : t;
        if (skipEmpty && tok.isEmpty()) continue;
        bool good      = false;
        const double v = tok.toDouble(&good);
        if (!good) return false;
        row.push_back(v);
    }
    return true;
}

} // namespace

/* -------------------------------------------------------------------- */

PlotData parsePlotCsv(const QString &text, QString *error)
{
    PlotData out;
    const QStringList lines = text.split('\n');
    QStringList names;
    int ncol        = -1;
    bool headerDone = false;
    std::vector<std::vector<double>> rows;

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        QStringList toks = line.split(',');
        for (QString &t : toks)
            t = t.trimmed();

        if (!headerDone) {
            headerDone      = true;
            bool allNumeric = !toks.isEmpty();
            for (const QString &t : toks) {
                bool ok = false;
                t.toDouble(&ok);
                if (!ok) {
                    allNumeric = false;
                    break;
                }
            }
            if (!allNumeric) {
                names = toks;
                ncol  = toks.size();
                continue; // header consumed
            }
            // a fully numeric first line means there is no header
            ncol  = toks.size();
            names = genericColumnNames(ncol);
            // fall through to parse this line as data
        }

        if (toks.size() != ncol) continue; // skip ragged lines
        std::vector<double> row;
        if (tokensToRow(toks, row)) rows.push_back(std::move(row));
    }

    if ((ncol <= 0) || rows.empty()) {
        if (error) *error = QStringLiteral("no CSV data found");
        return out;
    }
    out.setColumnNames(names);
    for (auto &r : rows)
        out.appendRow(r);
    return out;
}

/* -------------------------------------------------------------------- */

PlotData parsePlotWhitespace(const QString &text, QString *error)
{
    PlotData out;
    QString lastComment;
    QStringList names;
    int ncol = -1;
    std::vector<std::vector<double>> rows;

    // The text is walked line by line rather than split into a list of lines
    // first, and the fields of a line are read without a regular expression:
    // for a large file, those allocations were most of the cost of reading it.
    std::vector<double> row;
    QStringView rest(text);
    while (!rest.isEmpty()) {
        const qsizetype nl     = rest.indexOf(u'\n');
        const QStringView line = (nl < 0 ? rest : rest.left(nl)).trimmed();
        rest                   = (nl < 0) ? QStringView() : rest.mid(nl + 1);
        if (line.isEmpty()) continue;
        if (line.startsWith(u'#')) {
            lastComment = line.mid(1).trimmed().toString();
            continue;
        }

        if (!numericFields(line, row)) continue; // skip non-numeric lines (e.g. text headers)

        if (ncol < 0) {
            ncol                    = static_cast<int>(row.size());
            const QStringList ctoks = lastComment.split(whitespaceRe(), Qt::SkipEmptyParts);
            if (ctoks.size() == ncol)
                names = ctoks;
            else
                names = genericColumnNames(ncol);
        }
        if (static_cast<int>(row.size()) != ncol) continue; // skip ragged
        rows.push_back(row);
    }

    if ((ncol <= 0) || rows.empty()) {
        if (error) *error = QStringLiteral("no whitespace-separated data found");
        return out;
    }
    out.setColumnNames(names);
    for (auto &r : rows)
        out.appendRow(r);
    return out;
}

/* -------------------------------------------------------------------- */

PlotData parsePlotYaml(const QString &text, QString *error)
{
    PlotData out;
    const QStringList lines = text.split('\n');
    QStringList names;
    int ncol = -1;
    std::vector<std::vector<double>> rows;

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith("keywords:")) {
            const QStringList toks = bracketContents(line).split(',');
            names.clear();
            for (const QString &t : toks) {
                const QString name = unquote(t);
                // tolerate the trailing comma LAMMPS writes (e.g. "..., 'Press', ]")
                if (!name.isEmpty()) names << name;
            }
            ncol = names.size();
        } else if (line.startsWith('-')) {
            // Format 1: - [val, val, ...]  (LAMMPS thermo YAML, keywords already parsed)
            const QString inside = bracketContents(line);
            if (!inside.isEmpty()) {
                const QStringList toks = inside.split(',');
                std::vector<double> row;
                // tolerate the trailing comma LAMMPS writes (e.g. "..., -837.0, ]")
                if (tokensToRow(toks, row, /*skipEmpty=*/true) && !row.empty())
                    rows.push_back(std::move(row));
                continue;
            }
            // Format 2: - {key: val, key: val, ...}  (generic YAML sequence of maps)
            QString rest = line.mid(1).trimmed();
            if (rest.startsWith('{') && rest.endsWith('}')) {
                rest                    = rest.mid(1, rest.size() - 2).trimmed();
                const QStringList pairs = rest.split(',');
                if (names.isEmpty()) {
                    // first entry: derive column names from the keys
                    for (const QString &pair : pairs) {
                        const int colon = pair.indexOf(':');
                        if (colon < 0) {
                            names.clear();
                            break;
                        }
                        const QString key = pair.left(colon).trimmed();
                        if (!key.isEmpty()) names << key;
                    }
                    ncol = names.size();
                }
                if (ncol <= 0) continue;
                std::vector<double> row;
                bool ok = true;
                for (const QString &pair : pairs) {
                    const int colon = pair.indexOf(':');
                    if (colon < 0) {
                        ok = false;
                        break;
                    }
                    const QString val = pair.mid(colon + 1).trimmed();
                    bool good         = false;
                    const double v    = val.toDouble(&good);
                    if (!good) {
                        ok = false;
                        break;
                    }
                    row.push_back(v);
                }
                if (ok && !row.empty()) rows.push_back(std::move(row));
            }
        }
    }

    if (rows.empty()) {
        if (error) *error = QStringLiteral("no YAML data found");
        return out;
    }
    // fall back to generic names if keywords were missing or mismatched
    if (ncol != static_cast<int>(rows.front().size())) {
        ncol  = static_cast<int>(rows.front().size());
        names = genericColumnNames(ncol);
    }
    out.setColumnNames(names);
    for (auto &r : rows)
        if (static_cast<int>(r.size()) == ncol) out.appendRow(r);
    return out;
}

/* -------------------------------------------------------------------- */

PlotData parsePlotJson(const QByteArray &bytes, QString *error)
{
    PlotData out;
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (doc.isNull()) {
        if (error) *error = perr.errorString();
        return out;
    }

    // shape 1: array of equally long numeric rows
    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        if (arr.isEmpty() || !arr.first().isArray()) {
            if (error) *error = QStringLiteral("unsupported JSON array shape");
            return out;
        }
        int ncol = -1;
        std::vector<std::vector<double>> rows;
        for (const QJsonValue &v : arr) {
            if (!v.isArray()) continue;
            const QJsonArray r = v.toArray();
            std::vector<double> row;
            row.reserve(r.size());
            for (const QJsonValue &e : r)
                row.push_back(e.toDouble());
            if (ncol < 0) ncol = static_cast<int>(row.size());
            if (static_cast<int>(row.size()) == ncol) rows.push_back(std::move(row));
        }
        if ((ncol <= 0) || rows.empty()) {
            if (error) *error = QStringLiteral("no JSON rows found");
            return out;
        }
        out.setColumnNames(genericColumnNames(ncol));
        for (auto &r : rows)
            out.appendRow(r);
        return out;
    }

    // shape 2: object mapping names to equally long numeric arrays
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        int nrow              = -1;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (!it.value().isArray()) {
                if (error) *error = QStringLiteral("JSON object values must be arrays");
                return out;
            }
            const int len = it.value().toArray().size();
            if (nrow < 0)
                nrow = len;
            else if (len != nrow) {
                if (error) *error = QStringLiteral("JSON columns have unequal length");
                return out;
            }
        }
        if (nrow <= 0) {
            if (error) *error = QStringLiteral("no JSON data found");
            return out;
        }
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QJsonArray a = it.value().toArray();
            std::vector<double> col;
            col.reserve(a.size());
            for (const QJsonValue &e : a)
                col.push_back(e.toDouble());
            out.addColumn(it.key(), std::move(col));
        }
        return out;
    }

    if (error) *error = QStringLiteral("unsupported JSON document");
    return out;
}

/* -------------------------------------------------------------------- */

PlotData loadPlotData(const QString &filename, QString *error)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open file: %1").arg(filename);
        return {};
    }
    const QByteArray bytes = f.readAll();
    f.close();

    const QString text = QString::fromUtf8(bytes);

    // an explicit, known extension wins
    const QString suffix = QFileInfo(filename).suffix().toLower();
    if (suffix == "csv") return parsePlotCsv(text, error);
    if (suffix == "json") return parsePlotJson(bytes, error);
    if ((suffix == "yaml") || (suffix == "yml")) return parsePlotYaml(text, error);

    // otherwise detect the format from the content: a LAMMPS .log/.dat may
    // embed a YAML thermo block (interleaved with other log output), or the
    // file may actually be JSON
    if (looksLikeYaml(text)) return parsePlotYaml(text, error);
    if (looksLikeJson(text)) return parsePlotJson(bytes, error);
    return parsePlotWhitespace(text, error);
}

/* -------------------------------------------------------------------- */

namespace {
// 8 significant digits, matching the historical export precision
QString fmt(double v)
{
    return QString::number(v, 'g', 8);
}
} // namespace

QString writePlotCsv(const PlotData &data)
{
    QString out;
    for (int c = 0; c < data.columnCount(); ++c) {
        if (c) out += ',';
        out += data.columnName(c);
    }
    out += '\n';
    for (int r = 0; r < data.rowCount(); ++r) {
        for (int c = 0; c < data.columnCount(); ++c) {
            if (c) out += ',';
            out += fmt(data.column(c)[r]);
        }
        out += '\n';
    }
    return out;
}

QString writePlotDat(const PlotData &data, const QString &source)
{
    QString out;
    if (!source.isEmpty()) out += "# data from " + source + '\n';
    out += '#';
    for (int c = 0; c < data.columnCount(); ++c)
        out += ' ' + data.columnName(c);
    out += '\n';
    for (int r = 0; r < data.rowCount(); ++r) {
        for (int c = 0; c < data.columnCount(); ++c) {
            if (c) out += ' ';
            out += fmt(data.column(c)[r]);
        }
        out += '\n';
    }
    return out;
}

QString writePlotYaml(const PlotData &data)
{
    QString out = "---\n";
    out += "keywords: [";
    for (int c = 0; c < data.columnCount(); ++c) {
        if (c) out += ", ";
        out += '\'' + data.columnName(c) + '\'';
    }
    out += "]\n";
    out += "data:\n";
    for (int r = 0; r < data.rowCount(); ++r) {
        out += "  - [";
        for (int c = 0; c < data.columnCount(); ++c) {
            if (c) out += ", ";
            out += fmt(data.column(c)[r]);
        }
        out += "]\n";
    }
    out += "...\n";
    return out;
}

// Local Variables:
// c-basic-offset: 4
// End:
