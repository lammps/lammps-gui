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

#ifndef SHELLPROMPT_H
#define SHELLPROMPT_H

#include <QLineEdit>

class QEvent;

/**
 * @brief Input line that completes with Tab, the way a shell prompt does
 *
 * A plain QLineEdit gives Tab away to the focus chain and completes with nothing,
 * which is the wrong half of both.  This takes the key back: Tab offers what
 * matches the word being typed, Tab again walks the offers, and a word with only
 * one match is completed without a list appearing at all.
 *
 * Both interceptions have to happen in event() rather than in keyPressEvent() or
 * an event filter, for two separate reasons:
 *
 *   - @c QWidget::event() hands the focus on when it sees Tab, before
 *     @c keyPressEvent() is ever reached.
 *   - While the completion popup is up, QCompleter delivers keys to this widget
 *     by calling @c event() on it directly instead of sending them through the
 *     event loop, so an event filter installed on the line edit never sees them.
 *
 * The same override is what keeps Enter from doing two things at once; see
 * event() for what Qt does with it when left alone.
 *
 * The widget completes from whatever QCompleter it was given with setCompleter().
 * What it does not know is which list the word being typed should be completed
 * from -- in a shell that depends on where in the line the word is -- so it emits
 * completing() first and leaves that choice to its owner.
 */
class ShellPrompt : public QLineEdit {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget (optional)
     */
    explicit ShellPrompt(QWidget *parent = nullptr) : QLineEdit(parent) {}

    /**
     * @brief Destructor
     */
    ~ShellPrompt() override = default;

    ShellPrompt(const ShellPrompt &)            = delete;
    ShellPrompt(ShellPrompt &&)                 = delete;
    ShellPrompt &operator=(const ShellPrompt &) = delete;
    ShellPrompt &operator=(ShellPrompt &&)      = delete;

signals:
    /**
     * @brief Emitted before completions are computed, so the completer can be
     *        pointed at the list this word should be completed from
     * @param text The line as it currently stands
     */
    void completing(const QString &text);

protected:
    /**
     * @brief Filter out the keys a completing prompt has to own
     * @param event Event to handle
     * @return true if the event was consumed
     */
    bool event(QEvent *event) override;

private:
    /// Offer the matches for the word being typed, or walk the ones on offer.
    /// @param step 1 to take the next match, -1 for the one before it
    void completeWord(int step);
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
