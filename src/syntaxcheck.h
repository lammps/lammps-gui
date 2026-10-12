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

#ifndef SYNTAXCHECK_H
#define SYNTAXCHECK_H

#include <QList>
#include <QString>
#include <QStringList>

class LammpsSyntax;

/// severity of a single lint finding
enum class LintSeverity : quint8 {
    Warning, ///< suspicious, but may be intentional
    Error    ///< LAMMPS will reject this input
};

/// one finding of the pre-run input check
struct LintIssue {
    int line              = 0;                     ///< 1-based physical line number
    LintSeverity severity = LintSeverity::Warning; ///< how serious the finding is
    QString message;                               ///< human readable description
};

/**
 * @brief Static lint checks for LAMMPS input scripts
 *
 * Scans an input buffer with the syntax engine's InputScanner and applies a
 * set of deterministic checks against the introspected name registry:
 * unknown commands and style names, quoting and line continuation problems,
 * references to undefined variables and groups, references to computes,
 * fixes, or variables defined nowhere in the buffer, missing input files,
 * missing required arguments, and non-numeric arguments in a curated list
 * of strictly numeric positions.
 *
 * The design priority is the absence of false positives: any word containing
 * a '$' substitution is exempt from every check, a substitution anywhere on a
 * line disables its argument-count check, and script features that make
 * static analysis unreliable disable whole rule groups.  Any of include
 * files, jump/label loops, if/then commands, or python scripting disables
 * all definition-tracking rules (variable, group, and reference checks);
 * jump/label additionally downgrades unknown commands to warnings; shell,
 * geturl, and include disable file-existence checks downstream; read_restart
 * disables group and ID checks downstream; plugin disables unknown-name
 * errors downstream.  Only ERROR severity findings should gate a run.
 */
class SyntaxChecker {
public:
    /**
     * @brief Constructor
     * @param syntax Syntax registry with name sets and command specs (not owned, may not be null)
     */
    explicit SyntaxChecker(const LammpsSyntax *syntax) : syntax(syntax) {}
    ~SyntaxChecker() = default;

    SyntaxChecker()                                 = delete;
    SyntaxChecker(const SyntaxChecker &)            = delete;
    SyntaxChecker(SyntaxChecker &&)                 = delete;
    SyntaxChecker &operator=(const SyntaxChecker &) = delete;
    SyntaxChecker &operator=(SyntaxChecker &&)      = delete;

    /**
     * @brief Check an input buffer
     *
     * @param buffer complete input script text
     * @param presetVariables variable names that are defined outside the
     *        script before the run (the SetVariables dialog entries and the
     *        always-present gui_run variable)
     * @param cwd directory that relative file names are resolved against
     * @return findings sorted by line number
     */
    QList<LintIssue> check(const QString &buffer, const QStringList &presetVariables,
                           const QString &cwd) const;

    /// number of ERROR severity findings in a result list
    static int countErrors(const QList<LintIssue> &issues);

    /**
     * @brief Format findings as a plain text list, one finding per line
     * @param issues findings to format
     * @param maxShown maximum number of findings listed (-1 = all); when
     *        truncated, a final "... and N more" line is appended
     * @return the formatted list
     */
    static QString formatIssues(const QList<LintIssue> &issues, int maxShown = -1);

private:
    const LammpsSyntax *syntax; ///< syntax registry (not owned)
};

#endif // SYNTAXCHECK_H

// Local Variables:
// c-basic-offset: 4
// End:
