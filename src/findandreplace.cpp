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

#include "findandreplace.h"

#include "codeeditor.h"
#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTextCursor>

/* ---------------------------------------------------------------------- */

namespace {
constexpr int LAYOUT_SPACING = 6;
}

FindAndReplace::FindAndReplace(CodeEditor *_editor, QWidget *parent) :
    QDialog(parent), editor(_editor), search(nullptr), replace(nullptr), withcase(nullptr),
    wrap(nullptr), whole(nullptr)
{
    auto *layout  = new QGridLayout;
    search        = new QLineEdit;
    replace       = new QLineEdit;
    withcase      = new QCheckBox("Match case");
    wrap          = new QCheckBox("Wrap around");
    whole         = new QCheckBox("Whole word");
    auto *next    = new QPushButton("&Next");
    auto *replone = new QPushButton("&Replace");
    auto *replall = new QPushButton("Replace &All");
    auto *done    = new QPushButton("&Done");

    layout->addWidget(new QLabel("Find:"), 0, 0, Qt::AlignRight);
    layout->addWidget(search, 0, 1, 1, 2, Qt::AlignLeft);
    layout->addWidget(new QLabel("Replace with:"), 1, 0, Qt::AlignRight);
    layout->addWidget(replace, 1, 1, 1, 2, Qt::AlignLeft);
    layout->addWidget(withcase, 2, 0, Qt::AlignLeft);
    layout->addWidget(wrap, 2, 1, Qt::AlignLeft);
    layout->addWidget(whole, 2, 2, Qt::AlignLeft);
    wrap->setChecked(true);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(next);
    buttons->addWidget(replone);
    buttons->addWidget(replall);
    buttons->addWidget(done);
    buttons->setSpacing(LAYOUT_SPACING);
    layout->addLayout(buttons, 3, 0, 1, 3, Qt::AlignHCenter);
    layout->setSpacing(LAYOUT_SPACING);

    connect(next, &QPushButton::released, this, &FindAndReplace::findNext);
    connect(replone, &QPushButton::released, this, &FindAndReplace::replaceNext);
    connect(replall, &QPushButton::released, this, &FindAndReplace::replaceAll);
    connect(done, &QPushButton::released, this, &QDialog::accept);

    addShortcut(this, QKeySequence(Qt::CTRL | Qt::Key_Q), this, &FindAndReplace::quit);

    setLayout(layout);
    setWindowIcon(QIcon(Cfg::MAIN_ICON));
    setWindowTitle("LAMMPS-GUI - Find and Replace");
}

/* ---------------------------------------------------------------------- */

void FindAndReplace::findNext()
{
    auto text = search->text();

    auto find_flags = QTextDocument::FindFlags();
    if (withcase->isChecked()) find_flags |= QTextDocument::FindCaseSensitively;
    if (whole->isChecked()) find_flags |= QTextDocument::FindWholeWords;

    if (!text.isEmpty()) {
        if (!editor->find(text, find_flags) && wrap->isChecked()) {
            // nothing found from the current position to the end, reposition cursor at the
            // beginning
            editor->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
            editor->find(text, find_flags);
        }
    }
}

/* ---------------------------------------------------------------------- */

void FindAndReplace::replaceNext()
{
    auto text = search->text();
    if (text.isEmpty()) return;

    auto cursor = editor->textCursor();
    auto flag   = withcase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;

    // if selected text at cursor location matches search text, replace
    if (QString::compare(cursor.selectedText(), text, flag) == 0)
        cursor.insertText(replace->text());

    findNext();
}

/* ---------------------------------------------------------------------- */

void FindAndReplace::replaceAll()
{
    auto text = search->text();
    if (text.isEmpty()) return;

    // drop selection if we have one
    auto cursor = editor->textCursor();
    if (cursor.hasSelection()) cursor.movePosition(QTextCursor::Left);

    findNext();
    cursor = editor->textCursor();

    // keep replacing until findNext() does not find anything anymore
    while (cursor.hasSelection()) {
        cursor.insertText(replace->text());
        findNext();
        cursor = editor->textCursor();
    }
}

/* ---------------------------------------------------------------------- */

void FindAndReplace::quit()
{
    auto *main = qobject_cast<LammpsGui *>(parent());
    if (main) main->quit();
}

// Local Variables:
// c-basic-offset: 4
// End:
