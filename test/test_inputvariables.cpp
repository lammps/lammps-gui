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

#include "inputvariables.h"

#include <QList>
#include <QString>

#include "gtest/gtest.h"

TEST(InputVariablesTest, MatchIndexVariable)
{
    auto match = matchIndexVariable("variable xxx index 1 2 3 4 5");
    EXPECT_TRUE(match.valid);
    EXPECT_EQ(match.name, QString("xxx"));
    EXPECT_EQ(match.value, QString("1 2 3 4 5"));
    EXPECT_EQ(match.valueStart, 19);
    EXPECT_EQ(match.valueLength, 9);
}

TEST(InputVariablesTest, MatchIndexVariableWhitespace)
{
    // leading whitespace, tabs, and extra spacing shift the value offset
    auto match = matchIndexVariable("  variable\txxx   index\t 5 4 3 2 1  ");
    EXPECT_TRUE(match.valid);
    EXPECT_EQ(match.name, QString("xxx"));
    EXPECT_EQ(match.value, QString("5 4 3 2 1"));
    EXPECT_EQ(match.valueStart, 24);
    EXPECT_EQ(match.valueLength, 9);
    // check that the offsets point at the value in the original line
    const QString line("  variable\txxx   index\t 5 4 3 2 1  ");
    EXPECT_EQ(line.mid(match.valueStart, match.valueLength), QString("5 4 3 2 1"));
}

TEST(InputVariablesTest, MatchIndexVariableNoMatch)
{
    EXPECT_FALSE(matchIndexVariable("variable xxx loop 10").valid);
    EXPECT_FALSE(matchIndexVariable("variable xxx equal 1.0").valid);
    EXPECT_FALSE(matchIndexVariable("# variable xxx index 1 2 3").valid);
    EXPECT_FALSE(matchIndexVariable("print \"variable xxx index 1\"").valid);
    EXPECT_FALSE(matchIndexVariable("").valid);
}

TEST(InputVariablesTest, MatchIndexVariableEmptyValue)
{
    // an index variable command without values still matches (invalid input,
    // but the parser must register the name)
    auto match = matchIndexVariable("variable xxx index   ");
    EXPECT_TRUE(match.valid);
    EXPECT_EQ(match.name, QString("xxx"));
    EXPECT_TRUE(match.value.isEmpty());
}

TEST(InputVariablesTest, ParseDefinitions)
{
    const QString text("variable one index 1 2 3\n"
                       "variable two index a b c\n"
                       "run 0\n");
    const auto entries = parseInputVariables(text);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].name, QString("one"));
    EXPECT_EQ(entries[0].value, QString("1 2 3"));
    EXPECT_EQ(entries[0].scriptValue, QString("1 2 3"));
    EXPECT_EQ(entries[1].name, QString("two"));
    EXPECT_EQ(entries[1].value, QString("a b c"));
}

TEST(InputVariablesTest, ParseFirstDefinitionWins)
{
    // as in LAMMPS: redefinition of an existing index variable is ignored
    const QString text("variable xxx index 1 2 3\n"
                       "variable xxx index 5 4 3\n");
    const auto entries = parseInputVariables(text);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].scriptValue, QString("1 2 3"));
}

TEST(InputVariablesTest, ParseOtherStylesNotListed)
{
    // non-index definitions register the name but get no entry, and uses of
    // defined variables must not create used-but-undefined entries
    const QString text("variable aaa loop 10\n"
                       "variable bbb equal ${aaa}\n"
                       "print \"${bbb}\"\n");
    EXPECT_TRUE(parseInputVariables(text).isEmpty());
}

TEST(InputVariablesTest, ParseUsedButUndefined)
{
    const QString text("units ${sty}\n"
                       "run $n\n");
    const auto entries = parseInputVariables(text);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].name, QString("sty"));
    EXPECT_TRUE(entries[0].value.isEmpty());
    EXPECT_TRUE(entries[0].scriptValue.isEmpty());
    EXPECT_EQ(entries[1].name, QString("n"));
}

TEST(InputVariablesTest, MergeScriptEditWins)
{
    // the definition in the input was edited: the new script value replaces
    // the current value even if it was overridden in the dialog
    const QList<VariableEntry> previous = {{"xxx", "9 9 9", "1 2 3"}};
    const auto merged =
        mergeInputVariables(parseInputVariables("variable xxx index 5 4 3\n"), previous);
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].value, QString("5 4 3"));
    EXPECT_EQ(merged[0].scriptValue, QString("5 4 3"));
}

TEST(InputVariablesTest, MergeOverridePersists)
{
    // the definition in the input is unchanged: a dialog override survives
    const QList<VariableEntry> previous = {{"xxx", "9 9 9", "1 2 3"}};
    const auto merged =
        mergeInputVariables(parseInputVariables("variable xxx index 1 2 3\n"), previous);
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].value, QString("9 9 9"));
    EXPECT_EQ(merged[0].scriptValue, QString("1 2 3"));
}

TEST(InputVariablesTest, MergeUndefinedKeepsDialogValue)
{
    // a used-but-undefined variable keeps the value entered in the dialog
    const QList<VariableEntry> previous = {{"sty", "metal", ""}};
    const auto merged = mergeInputVariables(parseInputVariables("units ${sty}\n"), previous);
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].value, QString("metal"));
    EXPECT_TRUE(merged[0].scriptValue.isEmpty());
}

TEST(InputVariablesTest, MergeKeepsLeftoversWithValue)
{
    // dialog-defined variables that the parser cannot see (e.g. used only in
    // included files) survive as long as they have a value
    const QList<VariableEntry> previous = {{"inc", "somefile", ""}, {"unused", "", ""}};
    const auto merged = mergeInputVariables(parseInputVariables("run 0\n"), previous);
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].name, QString("inc"));
    EXPECT_EQ(merged[0].value, QString("somefile"));
}

TEST(InputVariablesTest, MergeDeletedDefinitionKeepsValue)
{
    // definition line removed but variable still used: keep the last value
    // as a dialog-owned entry so no data is lost
    const QList<VariableEntry> previous = {{"xxx", "9 9 9", "1 2 3"}};
    const auto merged = mergeInputVariables(parseInputVariables("run ${xxx}\n"), previous);
    ASSERT_EQ(merged.size(), 1);
    EXPECT_EQ(merged[0].value, QString("9 9 9"));
    EXPECT_TRUE(merged[0].scriptValue.isEmpty());
}

TEST(InputVariablesTest, MergeNewScriptVariable)
{
    const QList<VariableEntry> previous = {{"xxx", "1 2 3", "1 2 3"}};
    const auto merged                   = mergeInputVariables(
        parseInputVariables("variable xxx index 1 2 3\nvariable yyy index a b\n"), previous);
    ASSERT_EQ(merged.size(), 2);
    EXPECT_EQ(merged[1].name, QString("yyy"));
    EXPECT_EQ(merged[1].value, QString("a b"));
}

TEST(InputVariablesTest, IsOverridden)
{
    EXPECT_TRUE(isOverridden({"xxx", "9 9 9", "1 2 3"}));
    EXPECT_TRUE(isOverridden({"xxx", " 9 9 9 ", "1 2 3"})) << "value comparison must trim";
    EXPECT_FALSE(isOverridden({"xxx", "1 2 3", "1 2 3"}));
    // an empty value does not override: the script definition takes effect
    EXPECT_FALSE(isOverridden({"xxx", "", "1 2 3"}));
    // no script definition means nothing to override
    EXPECT_FALSE(isOverridden({"xxx", "9 9 9", ""}));
}

// Local Variables:
// c-basic-offset: 4
// End:
