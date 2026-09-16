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

#include "flagwarnings.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QTextDocument>

// Fixture for tests that need QApplication (for widgets)
class FlagWarningsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // QApplication needed for widget-based tests
        if (!QApplication::instance()) {
            // Use offscreen platform to avoid display requirement
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_flagwarnings"};
            app                 = new QApplication(argc, argv);
        }
    }

    static QApplication *app;
};

QApplication *FlagWarningsTest::app = nullptr;

TEST_F(FlagWarningsTest, ConstructorInitializesZeroWarnings)
{
    QTextDocument doc;
    FlagWarnings fw(nullptr, &doc);
    EXPECT_EQ(fw.getNWarnings(), 0);
}

TEST_F(FlagWarningsTest, DetectsWarningLines)
{
    QTextDocument doc;
    QLabel label;

    // Setting text BEFORE attaching the highlighter ensures content is there
    doc.setPlainText("WARNING: some warning message\nNormal line\nERROR: some error");

    FlagWarnings fw(&label, &doc);
    // Force re-highlight after construction
    fw.rehighlight();

    // The highlighter should have found 2 warnings (WARNING + ERROR)
    EXPECT_EQ(fw.getNWarnings(), 2);
}

TEST_F(FlagWarningsTest, IgnoresNormalLines)
{
    QTextDocument doc;
    doc.setPlainText("This is a normal line\nAnother normal line\nNo warnings here");

    FlagWarnings fw(nullptr, &doc);
    fw.rehighlight();

    EXPECT_EQ(fw.getNWarnings(), 0);
}

TEST_F(FlagWarningsTest, UpdatesSummaryLabel)
{
    QTextDocument doc;
    QLabel label;
    doc.setPlainText("WARNING: test warning\nLine 2");

    FlagWarnings fw(&label, &doc);
    fw.rehighlight();
    // the label is refreshed from the event loop, once per batch of lines
    QApplication::processEvents();

    // Label should contain the warning count and line count
    QString text = label.text();
    EXPECT_TRUE(text.contains("1 Warnings")) << text.toStdString();
    EXPECT_TRUE(text.contains("Lines")) << text.toStdString();
}

TEST_F(FlagWarningsTest, EmptyDocument)
{
    QTextDocument doc;
    doc.setPlainText("");

    FlagWarnings fw(nullptr, &doc);
    fw.rehighlight();

    EXPECT_EQ(fw.getNWarnings(), 0);
}

TEST_F(FlagWarningsTest, WarningAtStartOfLine)
{
    QTextDocument doc1;
    // WARNING must be at start of line for the regex ^(ERROR|WARNING).*$
    doc1.setPlainText("  WARNING: indented warning");
    FlagWarnings fw1(nullptr, &doc1);
    fw1.rehighlight();
    EXPECT_EQ(fw1.getNWarnings(), 0); // Should NOT match (not at start)

    QTextDocument doc2;
    doc2.setPlainText("WARNING: proper warning");
    FlagWarnings fw2(nullptr, &doc2);
    fw2.rehighlight();
    EXPECT_EQ(fw2.getNWarnings(), 1); // Should match
}

TEST_F(FlagWarningsTest, MultipleWarnings)
{
    QTextDocument doc;
    doc.setPlainText("WARNING: first warning\n"
                     "WARNING: second warning\n"
                     "ERROR: an error\n"
                     "Normal line\n"
                     "ERROR: another error");

    FlagWarnings fw(nullptr, &doc);
    fw.rehighlight();

    EXPECT_EQ(fw.getNWarnings(), 4);
}

TEST_F(FlagWarningsTest, ResetClearsWarningCount)
{
    QTextDocument doc;
    QLabel label;

    doc.setPlainText("WARNING: first warning\nERROR: an error");
    FlagWarnings fw(&label, &doc);
    fw.rehighlight();
    EXPECT_EQ(fw.getNWarnings(), 2);

    fw.reset();
    EXPECT_EQ(fw.getNWarnings(), 0);
    EXPECT_EQ(label.text(), QString("0 Warnings / Errors - 0 Lines"));
}

TEST_F(FlagWarningsTest, ReusedDocumentDoesNotAccumulateWarnings)
{
    // the log window is reused across runs: clearing the document and resetting
    // the counters must not carry the previous run's warnings into the new one
    QTextDocument doc;
    QLabel label;

    doc.setPlainText("WARNING: first run warning\nERROR: first run error");
    FlagWarnings fw(&label, &doc);
    fw.rehighlight();
    EXPECT_EQ(fw.getNWarnings(), 2);

    doc.clear();
    fw.reset();
    doc.setPlainText("WARNING: second run warning");
    fw.rehighlight();

    EXPECT_EQ(fw.getNWarnings(), 1);
}
