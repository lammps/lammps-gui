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

#include "syntaxcheck.h"

#include "lammpssyntax.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <climits>

namespace {

// style category names for lint messages
const char *styleCatName(StyleCat cat)
{
    switch (cat) {
        case StyleCat::Command:
            return "command";
        case StyleCat::Fix:
            return "fix";
        case StyleCat::Compute:
            return "compute";
        case StyleCat::Dump:
            return "dump";
        case StyleCat::Atom:
            return "atom";
        case StyleCat::Pair:
            return "pair";
        case StyleCat::Bond:
            return "bond";
        case StyleCat::Angle:
            return "angle";
        case StyleCat::Dihedral:
            return "dihedral";
        case StyleCat::Improper:
            return "improper";
        case StyleCat::Kspace:
            return "kspace";
        case StyleCat::Region:
            return "region";
        case StyleCat::Integrate:
            return "run";
        case StyleCat::Minimize:
            return "minimize";
        case StyleCat::Variable:
            return "variable";
        case StyleCat::Units:
            return "units";
        default:
            return "";
    }
}

// identifier following a reference prefix like c_ / f_ / v_
QString refName(const QString &word, int prefixLen)
{
    int end = prefixLen;
    while ((end < word.size()) &&
           (word.at(end).isLetterOrNumber() || (word.at(end) == QLatin1Char('_'))))
        ++end;
    return word.mid(prefixLen, end - prefixLen);
}

// names of variables referenced through immediate $x / ${name} substitution;
// $(...) immediate expressions are not descended into
QStringList substNames(const QString &word)
{
    QStringList names;
    const auto refs = findVarRefs(word);
    for (const auto &ref : refs) {
        if (ref.length < 2) continue;
        const QChar kind = word.at(ref.start + 1);
        if (kind == QLatin1Char('(')) continue;
        if (kind == QLatin1Char('{')) {
            if (ref.length > 3) names << word.mid(ref.start + 2, ref.length - 3);
        } else {
            names << word.mid(ref.start + 1, 1);
        }
    }
    return names;
}

} // namespace

QList<LintIssue> SyntaxChecker::check(const QString &buffer, const QStringList &presetVariables,
                                      const QString &cwd) const
{
    QList<LintIssue> issues;
    if (!syntax) return issues;

    InputScanner scanner;
    scanner.scan(buffer);

    // ---- pass 1: context flags and definition maps ------------------------

    bool hasInclude = false;   // include may define anything: definition rules off
    bool hasJump    = false;   // jump/label flow: order-dependent rules off
    bool hasIfThen  = false;   // quoted then-clauses may define IDs
    bool hasPython  = false;   // python callbacks may define variables
    int fileGate    = INT_MAX; // first shell/geturl/include line: file checks off after
    int restartGate = INT_MAX; // first read_restart line: ID checks off after
    int pluginGate  = INT_MAX; // first plugin line: unknown-name errors off after

    QHash<QString, int> varDefLine, groupDefLine; // name -> first definition line
    QSet<QString> varNames, computeIds, fixIds;
    QSet<QString> writeTargets; // files written earlier are exempt from existence checks

    for (const auto &name : presetVariables) {
        varDefLine.insert(name, 0);
        varNames.insert(name);
    }
    // computes that LAMMPS defines automatically at startup
    computeIds.insert(QStringLiteral("thermo_temp"));
    computeIds.insert(QStringLiteral("thermo_press"));
    computeIds.insert(QStringLiteral("thermo_pe"));

    // file names written by these commands (1-based argument index)
    static const struct {
        const char *cmd;
        int idx;
    } writecmds[] = {{"log", 1},     {"write_data", 1}, {"write_coeff", 1}, {"write_restart", 1},
                     {"restart", 2}, {"restart", 3},    {"dump", 5},        {"write_dump", 3}};

    for (const auto &cmd : scanner.commands()) {
        if (cmd.words.isEmpty() || cmd.words[0].text.isEmpty()) continue;
        const QString &name = cmd.words[0].text;
        const int line      = cmd.firstLine;
        const auto word     = [&cmd](int idx) -> QString {
            return (idx < cmd.words.size()) ? cmd.words[idx].text : QString();
        };
        // definition names are only trustworthy without $ substitution
        const auto defname = [&word](int idx) -> QString {
            const QString w = word(idx);
            return w.contains(QLatin1Char('$')) ? QString() : w;
        };

        if (name == QStringLiteral("include")) {
            hasInclude = true;
            fileGate   = qMin(fileGate, line);
        } else if ((name == QStringLiteral("jump")) || (name == QStringLiteral("label")) ||
                   (name == QStringLiteral("next"))) {
            hasJump = true;
        } else if (name == QStringLiteral("if")) {
            hasIfThen = true;
        } else if (name == QStringLiteral("python")) {
            hasPython = true;
        } else if ((name == QStringLiteral("shell")) || (name == QStringLiteral("geturl"))) {
            fileGate = qMin(fileGate, line);
        } else if (name == QStringLiteral("read_restart")) {
            restartGate = qMin(restartGate, line);
        } else if (name == QStringLiteral("plugin")) {
            pluginGate = qMin(pluginGate, line);
        } else if (name == QStringLiteral("variable")) {
            const QString vname = defname(1);
            if (!vname.isEmpty()) {
                if (!varDefLine.contains(vname)) varDefLine.insert(vname, line);
                varNames.insert(vname);
            }
        } else if ((name == QStringLiteral("kim")) && (word(1) == QStringLiteral("query"))) {
            // "kim query <name> ..." defines the named variable
            const QString vname = defname(2);
            if (!vname.isEmpty()) {
                if (!varDefLine.contains(vname)) varDefLine.insert(vname, line);
                varNames.insert(vname);
            }
        } else if (name == QStringLiteral("group")) {
            const QString gname = defname(1);
            if (!gname.isEmpty() && !groupDefLine.contains(gname)) groupDefLine.insert(gname, line);
        } else if (name == QStringLiteral("fix")) {
            if (!defname(1).isEmpty()) fixIds.insert(word(1));
        } else if (name == QStringLiteral("compute")) {
            if (!defname(1).isEmpty()) computeIds.insert(word(1));
        }

        for (const auto &wc : writecmds) {
            if (name == QLatin1String(wc.cmd)) {
                const QString target = defname(wc.idx);
                if (!target.isEmpty()) writeTargets.insert(target);
            }
        }
    }

    // ---- pass 2: apply the lint rules --------------------------------------

    // definition-tracking rules are only reliable when the buffer is complete
    // and executes top to bottom
    const bool idChecks = !hasInclude && !hasJump && !hasIfThen && !hasPython;
    const bool named    = syntax->isPopulated();

    for (const auto &cmd : scanner.commands()) {
        if (cmd.words.isEmpty()) continue;
        const auto &cmdword = cmd.words[0];
        if (cmdword.text.isEmpty() || cmdword.hasSubst || cmdword.quoted) continue;
        const QString &name = cmdword.text;
        const int line      = cmd.firstLine;

        bool lineHasSubst = false;
        for (const auto &word : cmd.words)
            lineHasSubst = lineHasSubst || word.hasSubst;

        // unknown command name; commands may be registered at runtime by the
        // plugin command, and jump targets may skip over unexecuted lines
        if (named && (line < pluginGate) && !syntax->knownCommand(name)) {
            issues.append({line, hasJump ? LintSeverity::Warning : LintSeverity::Error,
                           QStringLiteral("unknown command \"%1\"").arg(name)});
            continue; // no argument checks for an unknown command
        }

        const int specIdx       = syntax->commandIndex(name);
        const CommandSpec *spec = syntax->spec(specIdx);

        // minimum argument count; any substitution can expand to several words
        if (spec && !lineHasSubst && ((cmd.words.size() - 1) < spec->minArgs)) {
            issues.append({line, LintSeverity::Error,
                           QStringLiteral("\"%1\" requires at least %2 arguments")
                               .arg(name)
                               .arg(spec->minArgs)});
        }

        // per-argument role checks
        for (int i = 1; spec && (i < cmd.words.size()); ++i) {
            const auto &word = cmd.words[i];
            if (word.text.isEmpty() || word.hasSubst || word.quoted) continue;
            const ArgSpec as = syntax->argSpec(specIdx, i);

            // unknown style name in a style position
            if ((as.role == ArgRole::Style) && named && (line < pluginGate) &&
                !syntax->knownStyle(as.cat, word.text)) {
                issues.append({word.line, LintSeverity::Error,
                               QStringLiteral("unknown %1 style \"%2\"")
                                   .arg(QLatin1String(styleCatName(as.cat)), word.text)});
            }

            // reference to a group that is not defined at this point; only a
            // warning since some fixes create groups implicitly (for example
            // the bond/react stabilization groups)
            if ((as.role == ArgRole::GroupId) && idChecks && (line < restartGate) &&
                (word.text != QStringLiteral("all")) &&
                (!groupDefLine.contains(word.text) || (groupDefLine.value(word.text) >= line))) {
                issues.append({word.line, LintSeverity::Warning,
                               QStringLiteral("group ID \"%1\" is not defined before this command")
                                   .arg(word.text)});
            }
        }

        // immediate $-substitution of a variable that is not defined yet;
        // text inside quotes is not substituted when the line is parsed
        if (idChecks) {
            for (const auto &word : cmd.words) {
                if (word.quoted || !word.hasSubst) continue;
                const auto names = substNames(word.text);
                for (const auto &vname : names) {
                    if (vname.isEmpty()) continue;
                    if (!varDefLine.contains(vname) || (varDefLine.value(vname) >= line)) {
                        issues.append(
                            {word.line, LintSeverity::Error,
                             QStringLiteral("variable \"%1\" is used before it is defined")
                                 .arg(vname)});
                    }
                }
            }
        }

        // v_/c_/f_ references to names that are defined nowhere in the buffer;
        // only a warning, and only in contexts where such words are always
        // references (variable formulas, thermo_style arguments) -- in style
        // keyword positions words like "f_max" can be keyword names, and
        // styles can create fixes and computes implicitly
        const bool refContext = (name == QStringLiteral("variable")) ||
                                (name == QStringLiteral("thermo_style"));
        if (idChecks && refContext && (line < restartGate)) {
            const int firstRefArg = (name == QStringLiteral("variable")) ? 3 : 2;
            for (int i = firstRefArg; i < cmd.words.size(); ++i) {
                const auto &word = cmd.words[i];
                if (word.text.isEmpty() || word.quoted || word.hasSubst) continue;
                const QString &txt = word.text;
                QString missing, what;
                if (txt.startsWith(QStringLiteral("v_"))) {
                    const QString ref = refName(txt, 2);
                    if (!ref.isEmpty() && !varNames.contains(ref)) {
                        missing = ref;
                        what    = QStringLiteral("variable");
                    }
                } else if (txt.startsWith(QStringLiteral("c_")) ||
                           txt.startsWith(QStringLiteral("C_"))) {
                    const QString ref = refName(txt, 2);
                    if (!ref.isEmpty() && !computeIds.contains(ref)) {
                        // thermostat and barostat fixes create computes named
                        // <fix-ID>_temp, <fix-ID>_press, <fix-ID>_pe
                        bool implicit = false;
                        for (const auto *suffix : {"_temp", "_press", "_pe"}) {
                            const QLatin1String sfx(suffix);
                            if (ref.endsWith(sfx) && fixIds.contains(ref.chopped(sfx.size()))) {
                                implicit = true;
                                break;
                            }
                        }
                        if (!implicit) {
                            missing = ref;
                            what    = QStringLiteral("compute");
                        }
                    }
                } else if (txt.startsWith(QStringLiteral("f_")) ||
                           txt.startsWith(QStringLiteral("F_"))) {
                    const QString ref = refName(txt, 2);
                    if (!ref.isEmpty() && !fixIds.contains(ref)) {
                        missing = ref;
                        what    = QStringLiteral("fix");
                    }
                }
                if (!missing.isEmpty()) {
                    issues.append({word.line, LintSeverity::Warning,
                                   QStringLiteral("reference to %1 \"%2\" which is defined "
                                                  "nowhere in this file")
                                       .arg(what, missing)});
                }
            }
        }

        // missing input files (relative to cwd); disabled after shell/geturl/
        // include lines and for wildcard names and earlier write targets
        static const struct {
            const char *cmd;
            int idx;
        } filecmds[] = {{"include", 1},   {"read_data", 1}, {"read_restart", 1},
                        {"read_dump", 1}, {"rerun", 1},     {"molecule", 2}};
        if (line <= fileGate) {
            for (const auto &fc : filecmds) {
                if (name != QLatin1String(fc.cmd)) continue;
                if (fc.idx >= cmd.words.size()) continue;
                const auto &word = cmd.words[fc.idx];
                if (word.text.isEmpty() || word.hasSubst) continue;
                if (word.text.contains(QLatin1Char('*')) || word.text.contains(QLatin1Char('%')))
                    continue;
                if (writeTargets.contains(word.text)) continue;
                const QFileInfo info(QDir(cwd), word.text);
                if (!info.exists()) {
                    issues.append({word.line, LintSeverity::Error,
                                   QStringLiteral("file \"%1\" not found").arg(word.text)});
                }
            }
        }

        // strictly numeric argument positions of a few structural commands
        static const struct {
            const char *cmd;
            int idx;
            bool integer;
            bool allowVar;
        } numchecks[] = {{"run", 1, true, false},       {"timestep", 1, false, false},
                         {"minimize", 1, false, false}, {"minimize", 2, false, false},
                         {"minimize", 3, true, false},  {"minimize", 4, true, false},
                         {"replicate", 1, true, false}, {"replicate", 2, true, false},
                         {"replicate", 3, true, false}, {"thermo", 1, true, true}};
        for (const auto &nc : numchecks) {
            if (name != QLatin1String(nc.cmd)) continue;
            if (nc.idx >= cmd.words.size()) continue;
            const auto &word = cmd.words[nc.idx];
            if (word.text.isEmpty() || word.hasSubst || word.quoted) continue;
            if (nc.allowVar && word.text.startsWith(QStringLiteral("v_"))) continue;
            bool numeric = false;
            if (nc.integer)
                word.text.toLongLong(&numeric);
            else
                word.text.toDouble(&numeric);
            if (!numeric) {
                issues.append(
                    {word.line, LintSeverity::Error,
                     QStringLiteral("argument %1 of \"%2\" must be a%3 number (\"%4\")")
                         .arg(nc.idx)
                         .arg(name, nc.integer ? QStringLiteral("n integer") : QStringLiteral(""),
                              word.text)});
            }
        }
    }

    // tokenizer-level diagnostics
    for (const auto &diag : scanner.diagnostics()) {
        switch (diag.what) {
            case InputScanner::Diag::UnbalancedQuote:
                issues.append({diag.line, LintSeverity::Error,
                               QStringLiteral("unbalanced quotes on this line")});
                break;
            case InputScanner::Diag::UnclosedTriple:
                issues.append({diag.line, LintSeverity::Error,
                               QStringLiteral("triple-quoted string is never closed")});
                break;
            case InputScanner::Diag::DanglingContinuation:
                issues.append({diag.line, LintSeverity::Warning,
                               QStringLiteral("the last line ends with a line continuation '&'")});
                break;
        }
    }

    std::stable_sort(issues.begin(), issues.end(), [](const LintIssue &a, const LintIssue &b) {
        return a.line < b.line;
    });
    return issues;
}

int SyntaxChecker::countErrors(const QList<LintIssue> &issues)
{
    int nerrors = 0;
    for (const auto &issue : issues)
        if (issue.severity == LintSeverity::Error) ++nerrors;
    return nerrors;
}

QString SyntaxChecker::formatIssues(const QList<LintIssue> &issues, int maxShown)
{
    QString text;
    int shown = 0;
    for (const auto &issue : issues) {
        if ((maxShown >= 0) && (shown >= maxShown)) break;
        const char *severity = (issue.severity == LintSeverity::Error) ? "ERROR" : "warning";
        text += QStringLiteral("line %1: [%2] %3\n")
                    .arg(issue.line)
                    .arg(QLatin1String(severity), issue.message);
        ++shown;
    }
    if (shown < issues.size())
        text += QStringLiteral("... and %1 more\n").arg(issues.size() - shown);
    return text;
}

// Local Variables:
// c-basic-offset: 4
// End:
