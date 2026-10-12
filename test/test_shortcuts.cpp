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

#include "constants.h"
#include "helpers.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QSettings>
#include <QShortcut>
#include <QWidget>

// The output windows repeat several main window accelerators (Ctrl+S, Ctrl+Q,
// Ctrl+N, Ctrl+/, ...).  While each of them is a window of its own the default
// Qt::WindowShortcut context keeps those apart, but once they are docked into
// the main window they share its window and the duplicate bindings become
// ambiguous overloads.  Both helpers therefore have to scope their shortcut to
// the owning widget; these tests pin that contract down.
class ShortcutsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_shortcuts"};
            app                 = new QApplication(argc, argv);
        }
    }

    static QApplication *app;
};

QApplication *ShortcutsTest::app = nullptr;

TEST_F(ShortcutsTest, AddShortcutIsScopedToTheWidget)
{
    QWidget widget;
    int fired = 0;

    auto *sc = addShortcut(&widget, QKeySequence(Qt::CTRL | Qt::Key_S), &widget, [&]() {
        ++fired;
    });

    ASSERT_NE(sc, nullptr);
    EXPECT_EQ(sc->context(), Qt::WidgetWithChildrenShortcut);
    EXPECT_EQ(sc->key(), QKeySequence(Qt::CTRL | Qt::Key_S));
    EXPECT_EQ(qobject_cast<QWidget *>(sc->parent()), &widget);
    EXPECT_EQ(fired, 0);
}

TEST_F(ShortcutsTest, ScopeShortcutBindsActionToTheWidget)
{
    QWidget widget;
    QAction action("test", &widget);

    scopeShortcut(&widget, &action, QKeySequence(Qt::CTRL | Qt::Key_W));

    EXPECT_EQ(action.shortcut(), QKeySequence(Qt::CTRL | Qt::Key_W));
    EXPECT_EQ(action.shortcutContext(), Qt::WidgetWithChildrenShortcut);
    // a menu action is associated only with its (popup) menu, which never holds
    // the focus -- without this association the widget context could not match
    EXPECT_TRUE(widget.actions().contains(&action));
}

TEST_F(ShortcutsTest, ScopeShortcutToleratesNullArguments)
{
    QWidget widget;
    QAction action("test", &widget);

    scopeShortcut(nullptr, &action, QKeySequence(Qt::CTRL | Qt::Key_W));
    EXPECT_TRUE(action.shortcut().isEmpty());

    scopeShortcut(&widget, nullptr, QKeySequence(Qt::CTRL | Qt::Key_W));
    EXPECT_TRUE(widget.actions().isEmpty());
}

TEST_F(ShortcutsTest, ScopedShortcutsOnSiblingWidgetsDoNotCollide)
{
    // the docked case in miniature: two views inside one window, both binding
    // the same sequence.  With widget scope only the focused one is a candidate
    QWidget window;
    auto *left  = new QWidget(&window);
    auto *right = new QWidget(&window);

    int leftFired = 0, rightFired = 0;
    auto *ls = addShortcut(left, QKeySequence(Qt::CTRL | Qt::Key_S), left, [&]() {
        ++leftFired;
    });
    auto *rs = addShortcut(right, QKeySequence(Qt::CTRL | Qt::Key_S), right, [&]() {
        ++rightFired;
    });

    EXPECT_EQ(ls->context(), Qt::WidgetWithChildrenShortcut);
    EXPECT_EQ(rs->context(), Qt::WidgetWithChildrenShortcut);
    EXPECT_NE(qobject_cast<QWidget *>(ls->parent()), qobject_cast<QWidget *>(rs->parent()));
}

TEST_F(ShortcutsTest, StandaloneWindowKeepsShortcutsTheMainWindowAlsoBinds)
{
    // A view only has to give up a sequence once it is embedded in the main
    // window.  Deciding that from the layout preference alone disabled the
    // shortcut in windows that stand on their own -- the file viewers, the find
    // dialog, the standalone viewer modes -- where nothing is ambiguous.
    QSettings().setValue(Keys::DOCKED, true);
    setMainWindowShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Q)});

    QWidget standalone;
    auto *sc = addShortcut(&standalone, QKeySequence(Qt::CTRL | Qt::Key_Q), &standalone, []() {
    });
    EXPECT_TRUE(sc->isEnabled());

    QAction action("quit", &standalone);
    scopeShortcut(&standalone, &action, QKeySequence(Qt::CTRL | Qt::Key_Q));
    EXPECT_EQ(action.shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));

    QSettings().remove(Keys::DOCKED);
    setMainWindowShortcuts({});
}
