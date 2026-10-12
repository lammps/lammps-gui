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

#include "shellprompt.h"

#include <gtest/gtest.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QWidget>

// What a completing prompt does with Tab and Enter is decided as much by
// QCompleter as by ShellPrompt: while the popup is up the completer takes the
// keys first and hands them on itself, and what it does with what comes back
// depends on whether the widget accepted them.  These tests drive the pair the
// way the window manager does -- to the popup while there is one, to the line
// edit otherwise -- so they pin the behavior of the combination rather than of
// the override alone.
class ShellPromptTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_shellprompt"};
            app                 = new QApplication(argc, argv);
        }
    }

    static void TearDownTestSuite()
    {
        delete app;
        app = nullptr;
    }

    void SetUp() override
    {
        container = new QWidget;
        auto *box = new QVBoxLayout(container);
        prompt    = new ShellPrompt;
        // something for the focus to land on if Tab ever gets away
        neighbor = new QLineEdit;
        box->addWidget(prompt);
        box->addWidget(neighbor);

        model     = new QStringListModel({"less", "ls", "ls -l", "lsblk"}, container);
        completer = new QCompleter(container);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setCaseSensitivity(Qt::CaseSensitive);
        completer->setModel(model);
        prompt->setCompleter(completer);

        container->show();
        prompt->setFocus();
        QApplication::processEvents();

        submitted = 0;
        QObject::connect(prompt, &QLineEdit::returnPressed, [this]() {
            ++submitted;
        });
    }

    void TearDown() override
    {
        delete container;
        container = nullptr;
    }

    /// Deliver a key the way Qt does: a visible popup grabs the keyboard, and
    /// the completer is what passes the key on to the line edit from there.
    void typeKey(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
    {
        QWidget *target = completer->popup()->isVisible()
                              ? static_cast<QWidget *>(completer->popup())
                              : static_cast<QWidget *>(prompt);
        QKeyEvent press(QEvent::KeyPress, key, mods);
        QApplication::sendEvent(target, &press);
        QApplication::processEvents();
    }

    /// Put a line in as if it had been typed, so the completer is in the state
    /// typing would have left it in.
    void typeLine(const QString &text)
    {
        prompt->setText(text);
        emit prompt->textEdited(text);
        QApplication::processEvents();
    }

    bool listing() const { return completer->popup()->isVisible(); }

    static QApplication *app;
    QWidget *container      = nullptr;
    ShellPrompt *prompt     = nullptr;
    QLineEdit *neighbor     = nullptr;
    QCompleter *completer   = nullptr;
    QStringListModel *model = nullptr;
    int submitted           = 0;
};

QApplication *ShellPromptTest::app = nullptr;

// The whole point: a prompt where Tab moves the focus is not a prompt.  Qt gives
// the key away in QWidget::event(), before the line edit ever sees it.
TEST_F(ShellPromptTest, TabKeepsTheFocus)
{
    ASSERT_TRUE(prompt->hasFocus());
    typeLine("ls");
    typeKey(Qt::Key_Tab);
    EXPECT_TRUE(prompt->hasFocus());
    EXPECT_FALSE(neighbor->hasFocus());
}

TEST_F(ShellPromptTest, TabOffersTheMatches)
{
    typeLine("ls");
    EXPECT_FALSE(listing());
    typeKey(Qt::Key_Tab);
    EXPECT_TRUE(listing());
    // offered, but not chosen for the user
    EXPECT_EQ(prompt->text(), "ls");
}

// One match is an answer, not a choice, so it is taken without a list appearing.
TEST_F(ShellPromptTest, SingleMatchIsTakenWithoutAList)
{
    typeLine("lsb");
    typeKey(Qt::Key_Tab);
    EXPECT_EQ(prompt->text(), "lsblk");
    EXPECT_FALSE(listing());
}

TEST_F(ShellPromptTest, TabWalksTheMatches)
{
    typeLine("ls");
    typeKey(Qt::Key_Tab); // offer
    typeKey(Qt::Key_Tab); // first
    EXPECT_EQ(prompt->text(), "ls");
    typeKey(Qt::Key_Tab); // second
    EXPECT_EQ(prompt->text(), "ls -l");
    typeKey(Qt::Key_Tab); // third
    EXPECT_EQ(prompt->text(), "lsblk");
    // and around again rather than stopping at the end
    typeKey(Qt::Key_Tab);
    EXPECT_EQ(prompt->text(), "ls");
    EXPECT_TRUE(listing());
    // nothing has been run by any of that
    EXPECT_EQ(submitted, 0);
}

TEST_F(ShellPromptTest, BacktabWalksTheOtherWay)
{
    typeLine("ls");
    typeKey(Qt::Key_Tab);
    typeKey(Qt::Key_Backtab, Qt::ShiftModifier);
    EXPECT_EQ(prompt->text(), "lsblk");
    typeKey(Qt::Key_Backtab, Qt::ShiftModifier);
    EXPECT_EQ(prompt->text(), "ls -l");
}

// The bug this class exists for.  Qt runs the line *and* puts the completion
// back into it, so the command is executed and still typed, and the Enter meant
// to confirm it runs it a second time.
TEST_F(ShellPromptTest, EnterTakesTheMatchWithoutRunningIt)
{
    typeLine("ls");
    typeKey(Qt::Key_Tab);
    typeKey(Qt::Key_Tab);
    ASSERT_EQ(prompt->text(), "ls");
    ASSERT_TRUE(listing());

    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 0);
    EXPECT_FALSE(listing());
    EXPECT_EQ(prompt->text(), "ls");

    // the line stands, and the next Enter runs it -- once
    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 1);
}

// The same, reached the way it is reached in practice: the list comes up while
// typing and the entry is picked with the arrow keys, which QCompleter handles
// itself and never passes on.  Enter is still the key that has to be caught.
TEST_F(ShellPromptTest, EnterTakesAnArrowedMatchWithoutRunningIt)
{
    typeLine("ls");
    completer->setCompletionPrefix("ls");
    completer->complete();
    ASSERT_TRUE(listing());

    typeKey(Qt::Key_Down);
    ASSERT_EQ(prompt->text(), "ls");

    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 0);
    EXPECT_FALSE(listing());
    EXPECT_EQ(prompt->text(), "ls");

    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 1);
}

// Typing a line that happens to be the front of longer ones puts a list up
// without anything chosen in it.  Enter has to mean what it always means there,
// or a command could never be run without first being turned into another one.
TEST_F(ShellPromptTest, EnterRunsTheLineWhenNothingIsChosen)
{
    typeLine("ls");
    typeKey(Qt::Key_Tab);
    ASSERT_TRUE(listing());

    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 1);
    EXPECT_FALSE(listing());
    EXPECT_EQ(prompt->text(), "ls");
}

TEST_F(ShellPromptTest, EnterRunsTheLineWithNoListAtAll)
{
    typeLine("echo hello");
    typeKey(Qt::Key_Return);
    EXPECT_EQ(submitted, 1);
}

// The owner is told before the matches are computed, because which list a word
// is completed from is not something the prompt can know.
TEST_F(ShellPromptTest, CompletingIsAnnouncedBeforeMatching)
{
    QString announced;
    int calls = 0;
    QObject::connect(prompt, &ShellPrompt::completing, [&](const QString &text) {
        announced = text;
        ++calls;
        // a switch of models here is the point of the signal, and has to be in
        // force for the matching that follows
        model->setStringList({"lsof"});
    });

    typeLine("ls");
    typeKey(Qt::Key_Tab);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(announced, "ls");
    // the model set from the signal is the one that was matched against
    EXPECT_EQ(prompt->text(), "lsof");
}

// Every command there is is not an answer to anything.
TEST_F(ShellPromptTest, TabOnAnEmptyLineOffersNothing)
{
    typeKey(Qt::Key_Tab);
    EXPECT_FALSE(listing());
    EXPECT_TRUE(prompt->text().isEmpty());
    EXPECT_TRUE(prompt->hasFocus());
}

// A read-only prompt is one with a command running in front of it.  The key is
// still swallowed: moving the focus is not what Tab is for here either.
TEST_F(ShellPromptTest, ReadOnlyPromptCompletesNothing)
{
    typeLine("ls");
    prompt->setReadOnly(true);
    typeKey(Qt::Key_Tab);
    EXPECT_FALSE(listing());
    EXPECT_EQ(prompt->text(), "ls");
    EXPECT_TRUE(prompt->hasFocus());
}

// Ctrl-Tab belongs to whoever else is listening for it -- switching dock tabs,
// usually -- so it is not taken here.
TEST_F(ShellPromptTest, ModifiedTabIsLeftAlone)
{
    typeLine("ls");
    typeKey(Qt::Key_Tab, Qt::ControlModifier);
    EXPECT_FALSE(listing());
    EXPECT_EQ(prompt->text(), "ls");
}
