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

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QCompleter>
#include <QItemSelectionModel>
#include <QKeyEvent>

bool ShellPrompt::event(QEvent *event)
{
    if (event->type() != QEvent::KeyPress) return QLineEdit::event(event);

    auto *key                = static_cast<QKeyEvent *>(event);
    QCompleter *comp         = completer();
    QAbstractItemView *popup = comp ? comp->popup() : nullptr;
    const bool listing       = popup && popup->isVisible();

    switch (key->key()) {
        case Qt::Key_Tab:
            // Ctrl-Tab and the like belong to whoever else is listening for them
            if (key->modifiers() != Qt::NoModifier) break;
            completeWord(1);
            event->accept();
            return true;

        case Qt::Key_Backtab:
            // walks a list that is up the other way; with no list this stays the
            // one way left of moving the focus off the prompt from the keyboard
            if (!listing) break;
            completeWord(-1);
            event->accept();
            return true;

        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if (!listing) break;
            // Enter takes the highlighted entry and stops there; the line it
            // completed is run by the next Enter.  Left alone Qt does both at
            // once, and neither of them all the way: the line edit emits
            // returnPressed() as the completer passes the key through, which
            // runs the command and clears the line, and the completer then puts
            // the completion back into the line it was just cleared from.  What
            // is left is a command that has run and is still typed, so that one
            // more Enter runs it a second time.
            const bool taken = popup->selectionModel()->hasSelection();
            popup->hide();
            // nothing was highlighted, so there is nothing to take and Enter
            // means what it always means.  The popup is out of the way now, so
            // the line edit handles it as it would with no completer at all.
            if (!taken) QLineEdit::event(event);
            // either way the completer is not given the chance to insert
            // anything after this
            event->accept();
            return true;
        }

        default:
            break;
    }
    return QLineEdit::event(event);
}

void ShellPrompt::completeWord(int step)
{
    QCompleter *comp = completer();
    // a read-only prompt is one with a command running in front of it; the key
    // is still swallowed, because moving the focus is not what it is for here
    if (!comp || isReadOnly()) return;

    QAbstractItemView *popup    = comp->popup();
    QAbstractItemModel *matches = comp->completionModel();

    // a list is already up, so this walks it.  Each entry reached is put in the
    // line as it is reached -- that is what QCompleter does for the arrow keys
    // and what makes Enter run what can be seen.
    if (popup->isVisible()) {
        const int rows = matches->rowCount();
        if (rows < 1) return;
        // the walk starts at whichever end it is heading away from, and wraps
        // rather than stopping, so a list can be gone all the way around
        const int current = popup->currentIndex().row();
        int row           = (step > 0) ? 0 : rows - 1;
        if (popup->selectionModel()->hasSelection() && (current >= 0))
            row = ((current + step) % rows + rows) % rows;
        popup->setCurrentIndex(matches->index(row, 0));
        return;
    }

    // an empty line matches every command there is, which is a list of thousands
    // and an answer to nothing
    if (text().isEmpty()) return;

    // which list the word is completed from is the owner's to decide
    emit completing(text());
    comp->setCompletionPrefix(text());

    const int count = comp->completionCount();
    if (count < 1) return;

    // one match needs no list: take it and let the next word be typed
    if (count == 1) {
        comp->setCurrentRow(0);
        setText(comp->currentCompletion());
        return;
    }
    comp->complete();
}

// Local Variables:
// c-basic-offset: 4
// End:
