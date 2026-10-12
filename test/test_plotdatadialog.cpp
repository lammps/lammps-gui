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

#include "plotdatadialog.h"

#include "plotblockdata.h"
#include "plotdata.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

// The column-picker dialog's derived-column and rename behavior: columns are
// referenced as {name} in expressions, each column has exactly one live name,
// and a rename rewrites the references in already added derived columns.
class PlotDataDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_plotdatadialog"};
            app                 = new QApplication(argc, argv);
        }
    }

    void SetUp() override
    {
        // a rejected input opens a modal warning box, which would block the
        // test forever; close it from the event loop and count that it was there
        warnings = 0;
        watchdog = new QTimer;
        watchdog->setInterval(50);
        QObject::connect(watchdog, &QTimer::timeout, [this] {
            if (auto *m = QApplication::activeModalWidget()) {
                ++warnings;
                m->close();
            }
        });
        watchdog->start();
    }

    void TearDown() override
    {
        delete watchdog;
        watchdog = nullptr;
    }

    // the derive widgets have no object names; they are identified the same
    // way a user does, by their placeholder or button text
    static QLineEdit *deriveName(QDialog &d)
    {
        for (auto *e : d.findChildren<QLineEdit *>())
            if (e->placeholderText() == "new column name") return e;
        return nullptr;
    }
    static QLineEdit *deriveExpr(QDialog &d)
    {
        for (auto *e : d.findChildren<QLineEdit *>())
            if (e->placeholderText().startsWith("expression with")) return e;
        return nullptr;
    }
    static QPushButton *addButton(QDialog &d)
    {
        for (auto *b : d.findChildren<QPushButton *>())
            if (b->text() == "Add column") return b;
        return nullptr;
    }
    static QList<QLineEdit *> columnEdits(QDialog &d)
    {
        QList<QLineEdit *> edits;
        for (auto *e : d.findChildren<QLineEdit *>())
            if (e->placeholderText() == "column name") edits.append(e);
        return edits;
    }
    // commit a rename the way the widget does on Enter/focus-out
    static void rename(QLineEdit *edit, const QString &name)
    {
        edit->setText(name);
        QMetaObject::invokeMethod(edit, "editingFinished");
    }

    static PlotData sampleData()
    {
        PlotData data;
        data.setColumnNames({"Step", "c_rdf[1]", "c_rdf[2]"});
        data.appendRow({0.0, 1.0, 10.0});
        data.appendRow({1.0, 2.0, 20.0});
        data.appendRow({2.0, 3.0, 30.0});
        return data;
    }

    static QApplication *app;
    QTimer *watchdog = nullptr;
    int warnings     = 0;
};

QApplication *PlotDataDialogTest::app = nullptr;

// bracketed column names and accessors work in a derived-column expression
TEST_F(PlotDataDialogTest, DerivedColumnWithBracketName)
{
    PlotDataDialog dialog(sampleData());
    deriveName(dialog)->setText("scaled");
    deriveExpr(dialog)->setText("{c_rdf[2]}/{c_rdf[2]:max} + {c_rdf[1]:first}");
    addButton(dialog)->click();
    EXPECT_EQ(warnings, 0);

    const PlotData result = dialog.buildData();
    ASSERT_EQ(result.columnCount(), 4);
    EXPECT_EQ(result.columnName(3), QString("scaled"));
    EXPECT_DOUBLE_EQ(result.column(3)[0], 10.0 / 30.0 + 1.0);
    EXPECT_DOUBLE_EQ(result.column(3)[1], 20.0 / 30.0 + 1.0);
    EXPECT_DOUBLE_EQ(result.column(3)[2], 30.0 / 30.0 + 1.0);
}

// a bare column name is not a column lookup and is rejected with a warning
TEST_F(PlotDataDialogTest, BareNameIsRejected)
{
    PlotDataDialog dialog(sampleData());
    deriveName(dialog)->setText("bad");
    deriveExpr(dialog)->setText("Step + 1");
    addButton(dialog)->click();
    EXPECT_EQ(warnings, 1);
    EXPECT_EQ(dialog.buildData().columnCount(), 3);

    deriveExpr(dialog)->setText("{nope} + 1");
    addButton(dialog)->click();
    EXPECT_EQ(warnings, 2);
    EXPECT_EQ(dialog.buildData().columnCount(), 3);
}

// a rename takes effect immediately and rewrites stored derived expressions
TEST_F(PlotDataDialogTest, LiveRenameRewritesReferences)
{
    PlotDataDialog dialog(sampleData());
    deriveName(dialog)->setText("doubled");
    deriveExpr(dialog)->setText("{c_rdf[2]}*2");
    addButton(dialog)->click();
    ASSERT_EQ(dialog.buildData().columnCount(), 4);

    rename(columnEdits(dialog)[2], "g(r)");
    EXPECT_EQ(warnings, 0);
    EXPECT_EQ(dialog.buildData().columnName(2), QString("g(r)"));

    // the old name is gone, the new one resolves
    deriveName(dialog)->setText("old");
    deriveExpr(dialog)->setText("{c_rdf[2]}");
    addButton(dialog)->click();
    EXPECT_EQ(warnings, 1);
    deriveName(dialog)->setText("new");
    deriveExpr(dialog)->setText("{g(r)}*3");
    addButton(dialog)->click();
    EXPECT_EQ(warnings, 1);
    const PlotData result = dialog.buildData();
    ASSERT_EQ(result.columnCount(), 5);
    EXPECT_DOUBLE_EQ(result.column(4)[1], 60.0);
}

// empty, duplicate, and syntactically unusable names are refused and reverted
TEST_F(PlotDataDialogTest, RenameRejectsUnusableNames)
{
    PlotDataDialog dialog(sampleData());
    const auto edits = columnEdits(dialog);

    rename(edits[1], "Step"); // duplicate
    EXPECT_EQ(warnings, 1);
    EXPECT_EQ(edits[1]->text(), QString("c_rdf[1]"));

    rename(edits[1], "T:ps"); // colon collides with the accessor separator
    EXPECT_EQ(warnings, 2);
    EXPECT_EQ(edits[1]->text(), QString("c_rdf[1]"));

    rename(edits[1], "   "); // emptied field falls back, without a warning
    EXPECT_EQ(warnings, 2);
    EXPECT_EQ(edits[1]->text(), QString("c_rdf[1]"));
    EXPECT_EQ(dialog.buildData().columnName(1), QString("c_rdf[1]"));
}

// renames and derived columns survive a change of the block reduction: the
// derived expression is re-evaluated against the renamed reduced table
TEST_F(PlotDataDialogTest, BlockReductionKeepsRenamesAndDerived)
{
    PlotBlockData blocks;
    for (int b = 0; b < 2; ++b) {
        PlotDataBlock block;
        block.step = 100 * (b + 1);
        block.rows.setColumnNames({"Bin", "c_rdf[1]"});
        // block 2 holds different data so the reduction change is observable
        block.rows.appendRow({1.0, 10.0 * (b + 1)});
        block.rows.appendRow({2.0, 20.0 * (b + 1)});
        blocks.blocks.push_back(std::move(block));
    }
    PlotDataDialog dialog(blocks);

    // derive first, then rename: the reduction change below re-evaluates the
    // stored expression, so it only survives if the rename rewrote it
    deriveName(dialog)->setText("half");
    deriveExpr(dialog)->setText("{c_rdf[1]}/2");
    addButton(dialog)->click();
    rename(columnEdits(dialog)[1], "g(r)");
    EXPECT_EQ(warnings, 0);

    // switch from the default reduction to single block 1
    QSpinBox *blockSpin = nullptr;
    for (auto *s : dialog.findChildren<QSpinBox *>())
        if (s->toolTip().isEmpty() && s->isEnabled()) blockSpin = s;
    ASSERT_NE(blockSpin, nullptr);
    blockSpin->setValue(1);

    const PlotData result = dialog.buildData();
    ASSERT_EQ(result.columnCount(), 3);
    EXPECT_EQ(result.columnName(1), QString("g(r)"));
    EXPECT_EQ(result.columnName(2), QString("half"));
    EXPECT_DOUBLE_EQ(result.column(1)[0], 10.0);
    EXPECT_DOUBLE_EQ(result.column(2)[0], 5.0);
}

// fromFile() reads a file and picks the dialog for its format: the flat table
// parsers for a plain file, the block reduction for fix ave/* output, and no
// dialog at all -- with a reason -- for a file that neither can read.
TEST_F(PlotDataDialogTest, FromFilePicksTheParserForTheFormat)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    auto writeFile = [&](const QString &name, const QString &text) {
        QFile f(dir.filePath(name));
        EXPECT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream(&f) << text;
        return f.fileName();
    };

    const QString flat = writeFile("flat.dat", "# Step Temp\n0 1.5\n10 1.6\n20 1.7\n");
    QString error;
    auto dialog = PlotDataDialog::fromFile(flat, nullptr, &error);
    ASSERT_TRUE(dialog);
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(dialog->buildData().columnCount(), 2);
    EXPECT_EQ(dialog->buildData().rowCount(), 3);

    const QString blocks = writeFile("ave.dat", "# Time-averaged data for fix ave1\n"
                                                "# TimeStep Number-of-rows\n# Row c_1[1]\n"
                                                "0 2\n1 0.5\n2 0.6\n100 2\n1 0.7\n2 0.8\n");
    dialog               = PlotDataDialog::fromFile(blocks, nullptr, &error);
    ASSERT_TRUE(dialog);
    EXPECT_TRUE(error.isEmpty());
    // the block dialog offers the reduced table, not the raw block lines
    EXPECT_EQ(dialog->buildData().rowCount(), 2);

    dialog = PlotDataDialog::fromFile(dir.filePath("missing.dat"), nullptr, &error);
    EXPECT_FALSE(dialog);
    EXPECT_FALSE(error.isEmpty());
}

// Local Variables:
// c-basic-offset: 4
// End:
