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

#include "lammpssyntax.h"
#include "syntaxcheck.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

class SyntaxCheckTest : public ::testing::Test {
protected:
    void SetUp() override
    {
#ifdef COMMAND_SPECS_TABLE
        ASSERT_TRUE(syntax.loadCommandSpecs(QStringLiteral(COMMAND_SPECS_TABLE)));
#endif
        syntax.setCommands(
            {"fix",        "compute",      "dump",     "run",        "units",       "pair_style",
             "include",    "read_data",    "variable", "group",      "print",       "thermo_style",
             "thermo",     "timestep",     "minimize", "replicate",  "jump",        "label",
             "next",       "if",           "python",   "shell",      "geturl",      "read_restart",
             "plugin",     "clear",        "log",      "write_data", "dump_modify", "velocity",
             "molecule",   "rerun",        "lattice",  "region",     "create_box",  "mass",
             "pair_coeff", "kspace_style", "unfix",    "write_dump"});
        syntax.setStyles(StyleCat::Fix, {"nvt", "nve", "ave/time"});
        syntax.setStyles(StyleCat::Pair, {"lj/cut", "zero", "hybrid", "hybrid/overlay"});
        syntax.setStyles(StyleCat::Compute, {"temp"});
        syntax.setStyles(StyleCat::Dump, {"atom", "custom"});
        syntax.setStyles(StyleCat::Region, {"block", "sphere"});
    }

    QList<LintIssue> lint(const QString &buffer, const QStringList &presets = {},
                          const QString &cwd = QDir::tempPath())
    {
        const SyntaxChecker checker(&syntax);
        return checker.check(buffer, presets, cwd);
    }

    static bool hasMessage(const QList<LintIssue> &issues, const QString &fragment)
    {
        for (const auto &issue : issues)
            if (issue.message.contains(fragment)) return true;
        return false;
    }

    static int countWarnings(const QList<LintIssue> &issues)
    {
        int count = 0;
        for (const auto &issue : issues)
            if (issue.severity == LintSeverity::Warning) ++count;
        return count;
    }

    LammpsSyntax syntax;
};

TEST_F(SyntaxCheckTest, CleanScriptHasNoIssues)
{
    const auto issues = lint(QStringLiteral("# example\nunits lj\npair_style lj/cut 2.5\n"
                                            "pair_coeff 1 1 1.0 1.0\nfix 1 all nvt\nrun 100\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();
}

TEST_F(SyntaxCheckTest, UnknownCommand)
{
    const auto issues = lint(QStringLiteral("runx 100\n"));
    ASSERT_EQ(issues.size(), 1);
    EXPECT_EQ(issues[0].severity, LintSeverity::Error);
    EXPECT_EQ(issues[0].line, 1);
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unknown command")));
}

TEST_F(SyntaxCheckTest, UnknownCommandDowngradedWithJump)
{
    // jumped-over lines may never execute: downgrade to a warning
    const auto issues = lint(QStringLiteral("label top\nrunx 100\njump SELF top\n"));
    ASSERT_FALSE(issues.isEmpty());
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 0);
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unknown command")));
}

TEST_F(SyntaxCheckTest, UnknownCommandSkippedAfterPlugin)
{
    const auto issues = lint(QStringLiteral("plugin load foo.so\nrunx 100\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("unknown command")));
}

TEST_F(SyntaxCheckTest, UnknownStyle)
{
    auto issues = lint(QStringLiteral("fix 1 all nvtx\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unknown fix style \"nvtx\"")));
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 1);

    issues = lint(QStringLiteral("pair_style lj/cutt 2.5\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unknown pair style")));

    // hybrid styles validate only the leading word; sub-styles are not checked
    issues = lint(QStringLiteral("pair_style hybrid lj/cut 2.5 zero 3.0\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // $-substitution in a style position disables the check
    issues = lint(QStringLiteral("variable sty index nvt\nfix 1 all ${sty}\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("unknown fix style")));

    // "none" is accepted for force field styles
    issues = lint(QStringLiteral("kspace_style none\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();
}

TEST_F(SyntaxCheckTest, QuoteAndContinuationDiagnostics)
{
    auto issues = lint(QStringLiteral("print \"abc\nrun 100\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unbalanced quotes")));
    EXPECT_GE(SyntaxChecker::countErrors(issues), 1);

    issues = lint(QStringLiteral("print \"\"\"abc\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("triple-quoted")));

    issues = lint(QStringLiteral("run 100 &"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("line continuation")));
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 0); // only a warning
}

TEST_F(SyntaxCheckTest, UndefinedVariable)
{
    // used before defined
    auto issues = lint(QStringLiteral("print $x\nvariable x index 1\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("variable \"x\" is used before")));

    // defined first: clean
    issues = lint(QStringLiteral("variable x index 1\nprint $x\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("used before")));

    // preset variables (SetVariables dialog + gui_run) count as defined
    issues = lint(QStringLiteral("print ${steps}\n"), {QStringLiteral("steps")});
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("used before")));

    // quoted text is not substituted when the line is parsed
    issues = lint(QStringLiteral("print \"value = $x\"\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("used before")));

    // an include can define variables: rule disabled buffer-wide
    issues = lint(QStringLiteral("include setup.lmp\nprint $x\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("used before")));
}

TEST_F(SyntaxCheckTest, UndefinedGroup)
{
    // only a warning: some fixes (e.g. bond/react stabilization) create groups
    auto issues = lint(QStringLiteral("fix 1 mygrp nvt\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("group ID \"mygrp\"")));
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 0);
    EXPECT_EQ(countWarnings(issues), 1);

    // "all" always exists; defined groups are recognized
    issues = lint(QStringLiteral("group mygrp type 1\nfix 1 mygrp nvt\nfix 2 all nve\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // group used before its definition
    issues = lint(QStringLiteral("fix 1 mygrp nvt\ngroup mygrp type 1\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("group ID \"mygrp\"")));

    // a restart file restores groups: rule disabled after read_restart
    issues = lint(QStringLiteral("shell prepare\nread_restart foo.rst\nfix 1 mygrp nvt\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("group ID")));

    // group role on other commands (velocity)
    issues = lint(QStringLiteral("velocity vgrp create 300.0 4928\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("group ID \"vgrp\"")));
}

TEST_F(SyntaxCheckTest, MissingFiles)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    QFile data(tmp.filePath(QStringLiteral("data.lmp")));
    ASSERT_TRUE(data.open(QIODevice::WriteOnly));
    data.write("# dummy\n");
    data.close();

    // existing file: clean
    auto issues = lint(QStringLiteral("read_data data.lmp\n"), {}, tmp.path());
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // missing file: error
    issues = lint(QStringLiteral("read_data missing.lmp\n"), {}, tmp.path());
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("file \"missing.lmp\" not found")));

    // $-substitution and wildcards disable the check
    issues = lint(QStringLiteral("variable i index 1\nread_data data_${i}.lmp\n"), {}, tmp.path());
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("not found")));
    issues = lint(QStringLiteral("read_restart file.*\n"), {}, tmp.path());
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("not found")));

    // shell commands may create files: checks disabled downstream
    issues =
        lint(QStringLiteral("shell cp /tmp/a data2.lmp\nread_data data2.lmp\n"), {}, tmp.path());
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("not found")));

    // files written earlier in the script are exempt
    issues = lint(QStringLiteral("write_data out.data\nread_data out.data\n"), {}, tmp.path());
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("not found")));
}

TEST_F(SyntaxCheckTest, MinimumArgumentCount)
{
    auto issues = lint(QStringLiteral("fix 1 all\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("requires at least 3 arguments")));

    issues = lint(QStringLiteral("dump 1 all atom 100\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("requires at least 5 arguments")));

    // a substitution can expand to any number of words
    issues = lint(QStringLiteral("variable rest index \"all nvt\"\nfix 1 ${rest}\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("requires at least")));
}

TEST_F(SyntaxCheckTest, StrictlyNumericArguments)
{
    auto issues = lint(QStringLiteral("run abc\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("must be an integer number")));

    issues = lint(QStringLiteral("timestep fast\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("must be a number")));

    issues = lint(QStringLiteral("run 100\ntimestep 0.005\nminimize 1.0e-4 1.0e-6 100 1000\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // thermo accepts an equal-style variable reference
    issues = lint(QStringLiteral("variable n equal 100\nthermo v_n\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();
}

TEST_F(SyntaxCheckTest, WholeBufferReferenceWarnings)
{
    // reference to a compute defined nowhere: warning, not error
    auto issues = lint(QStringLiteral("thermo_style custom step c_mytemp\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("compute \"mytemp\"")));
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 0);
    EXPECT_EQ(countWarnings(issues), 1);

    // defined later in the buffer is fine (references resolve at run time)
    issues = lint(QStringLiteral("thermo_style custom step c_mytemp\ncompute mytemp all temp\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // undefined variable reference in a variable formula
    issues = lint(QStringLiteral("variable e equal v_und+1\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("variable \"und\"")));
    issues = lint(QStringLiteral("thermo_style custom f_2\nfix 2 all nve\n"));
    EXPECT_FALSE(hasMessage(issues, QStringLiteral("fix \"2\"")));

    // implicitly created computes of thermostat fixes are recognized
    issues = lint(QStringLiteral("fix 1 all nvt\nthermo_style custom step c_1_temp\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    // only reference contexts are checked: "f_max" here is a style keyword
    issues = lint(QStringLiteral("fix 1 all nvt temp 300 300 100 f_max 0.3\n"));
    EXPECT_EQ(countWarnings(issues), 0);

    // if/then clauses can define anything: warnings disabled
    issues = lint(QStringLiteral("if \"${x} > 0\" then \"print yes\"\n"
                                 "variable e equal v_und+1\n"),
                  {QStringLiteral("x")});
    EXPECT_EQ(countWarnings(issues), 0);
}

TEST_F(SyntaxCheckTest, ContinuationsAreJoinedBeforeChecking)
{
    // arguments on continuation lines count toward the argument minimum and
    // the style position is found across the join
    auto issues = lint(QStringLiteral("fix 1 all &\n  nvt\nrun 100\n"));
    EXPECT_TRUE(issues.isEmpty()) << SyntaxChecker::formatIssues(issues).toStdString();

    issues = lint(QStringLiteral("pair_style lj/&\ncutt 2.5\n"));
    EXPECT_TRUE(hasMessage(issues, QStringLiteral("unknown pair style \"lj/cutt\"")));
}

TEST_F(SyntaxCheckTest, FormatAndCount)
{
    const auto issues = lint(QStringLiteral("runx 1\nwalkx 2\nflyx 3\n"));
    ASSERT_EQ(issues.size(), 3);
    EXPECT_EQ(SyntaxChecker::countErrors(issues), 3);
    const QString text = SyntaxChecker::formatIssues(issues, 2);
    EXPECT_TRUE(text.contains(QStringLiteral("line 1:")));
    EXPECT_TRUE(text.contains(QStringLiteral("line 2:")));
    EXPECT_FALSE(text.contains(QStringLiteral("line 3:")));
    EXPECT_TRUE(text.contains(QStringLiteral("... and 1 more")));
    EXPECT_TRUE(SyntaxChecker::formatIssues(issues).contains(QStringLiteral("line 3:")));
}

// Local Variables:
// c-basic-offset: 4
// End:
