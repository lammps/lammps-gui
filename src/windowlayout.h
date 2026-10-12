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

#ifndef WINDOWLAYOUT_H
#define WINDOWLAYOUT_H

#include <QList>
#include <QObject>

#include <initializer_list>

class QDockWidget;
class QEvent;
class QMainWindow;
class QWidget;

/**
 * @brief The output views LammpsGui presents alongside the editor
 *
 * One enumerator per view that LammpsGui keeps for the lifetime of a session
 * (as opposed to the transient file viewers and inspection windows, which are
 * created and closed on demand).  Used to address a view in WindowLayout
 * without the layout having to know the concrete widget classes.
 */
enum class ViewSlot {
    Log,       ///< Output window with the captured LAMMPS log
    Chart,     ///< Charts window with the thermo data of the current run
    Image,     ///< Snapshot image viewer
    SlideShow, ///< Slide show viewer for dump image sequences
    Variables, ///< Variables window listing the active index variables
    Command,   ///< Shell prompt with a scrollback
    Count      ///< Number of slots; not a view itself
};

/**
 * @brief How the output views are presented
 */
enum class LayoutMode {
    Windows, ///< Each view is an individual, freely placed top-level window
    Docked   ///< The views are docked into the main window around the editor
};

/**
 * @brief Presentation policy for the output views of the main window
 *
 * WindowLayout is the single place that decides *how* an output view is put
 * in front of the user.  LammpsGui creates and owns the view widgets and
 * keeps its typed pointers to them, but does not call show(), hide() or
 * isVisible() on them directly: it hands each widget to the layout with
 * place() and then addresses it by its ViewSlot.
 *
 * Two policies are available, chosen once at construction from the user
 * preference:
 *
 * - LayoutMode::Windows keeps every view an individual top-level window,
 *   freely placed and stacked, which is what the application has always done.
 * - LayoutMode::Docked puts the views into dock areas around the editor, which
 *   stays the central widget: the charts, image and slide show views share a
 *   tabbed group on the right, the log, the variables view and the command
 *   window share a group across the full width at the bottom.
 *
 * In docked mode the layout owns one QDockWidget per slot, created up front so
 * that a saved arrangement can be restored before the views themselves exist.
 * place() only swaps the content of the dock, so a view that is destroyed and
 * rebuilt (as the image viewer is on every render) keeps its position and its
 * place in the tab order.
 *
 * The layout never owns the view widgets themselves.  It watches them for
 * destruction, so a slot whose widget is deleted elsewhere empties itself and
 * never hands out a dangling pointer.
 *
 * @see LammpsGui for the owner of both the layout and the view widgets
 */
class WindowLayout : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param mainwindow Main window the views belong to; also becomes the
     *                   parent object, so the layout is deleted along with it
     * @param mode       Presentation policy to apply
     *
     * In docked mode the dock widgets are created and a previously saved
     * arrangement is restored here, before any view exists.
     */
    WindowLayout(QMainWindow *mainwindow, LayoutMode mode);

    /**
     * @brief Destructor
     */
    ~WindowLayout() override;

    WindowLayout()                                = delete;
    WindowLayout(const WindowLayout &)            = delete;
    WindowLayout(WindowLayout &&)                 = delete;
    WindowLayout &operator=(const WindowLayout &) = delete;
    WindowLayout &operator=(WindowLayout &&)      = delete;

    /**
     * @brief The policy this layout applies
     * @return The mode passed to the constructor
     */
    LayoutMode mode() const { return layoutmode; }

    /**
     * @brief Put a view widget into a slot
     * @param slot Slot the widget belongs to
     * @param view Widget to present; may be nullptr to empty the slot
     *
     * Replaces whatever the slot held before without deleting it -- the
     * widgets stay owned by LammpsGui.  Safe to call again with the same
     * widget, which is what a reused Output or Charts window does.
     */
    void place(ViewSlot slot, QWidget *view);

    /**
     * @brief Widget currently in a slot
     * @param slot Slot to query
     * @return The widget, or nullptr if the slot is empty
     */
    QWidget *view(ViewSlot slot) const;

    /**
     * @brief Show the view in a slot
     * @param slot Slot to show
     *
     * Does nothing when the slot is empty.
     */
    void show(ViewSlot slot);

    /**
     * @brief Hide the view in a slot
     * @param slot Slot to hide
     *
     * Does nothing when the slot is empty.
     */
    void hide(ViewSlot slot);

    /**
     * @brief Show the view in a slot and bring it to the front
     * @param slot Slot to raise
     *
     * Use for an explicit request from the user.  Unlike show(), this pulls the
     * view to the front of its tab group, which is not wanted for the periodic
     * updates during a run.
     */
    void raise(ViewSlot slot);

    /**
     * @brief Show or hide the view in a slot
     * @param slot Slot to update
     * @param visible true to show the view, false to hide it
     */
    void setVisible(ViewSlot slot, bool visible);

    /**
     * @brief Flip the visibility of the view in a slot
     * @param slot Slot to toggle
     * @return Visibility of the view after the call (false for an empty slot)
     *
     * Docked, a view that is on screen without holding the keyboard focus is
     * raised and focused rather than hidden, so the key that opened a panel is
     * also the key that goes back to it; a second press, with the focus in it
     * by then, hides it as before.
     *
     * Persists the new state for the slots that have a "show by default"
     * preference (Output and Charts), so the next session starts the way the
     * session ended.
     */
    bool toggle(ViewSlot slot);

    /**
     * @brief Move the keyboard focus to the neighboring pane
     * @param forward true for the next pane, false for the previous one
     *
     * The panes are the editor and the panels that are on screen, walked in
     * the order they are arranged around it and wrapping at either end.  A
     * panel behind a tab is not a pane of its own: it is reached with the key
     * that opens it, which raises it within its group.  Does nothing with
     * individual windows, where the window manager has a key for this.
     */
    void focusNextPane(bool forward);

    /**
     * @brief Check whether the view in a slot is visible
     * @param slot Slot to query
     * @return true if the slot holds a widget and that widget is visible
     */
    bool isVisible(ViewSlot slot) const;

signals:
    /**
     * @brief A view was brought to the front of its group
     * @param view The view now in front, or nullptr if the slot was empty
     *
     * The combined layout shows one menu bar for the whole window, so whoever
     * owns it needs to know which panel the user just asked to see.
     */
    void viewActivated(QWidget *view);

public:
    /**
     * @brief Show a transient view as a tab beside an existing panel
     * @param view  Widget to show; it keeps its own lifetime
     * @param group Slot whose dock the new tab joins
     * @param title Short label for the tab -- the window title of a viewer is
     *              far too long to sit in one
     *
     * With individual windows this just shows the widget.  Docked, it gets a
     * dock of its own tabbed into that group, which is removed again when the
     * widget is destroyed.
     */
    void addAuxiliaryView(QWidget *view, ViewSlot group, const QString &title);

    /**
     * @brief Store the current dock arrangement in the settings
     *
     * Does nothing in windowed mode, where the views carry their own geometry.
     * Call before the main window is destroyed.
     */
    void saveState() const;

protected:
    /**
     * @brief Track the docked views and their containers
     * @param watched Object being watched: the main window (resizes keep the
     *                dock proportions), a dock (a dragged splitter updates
     *                them), or a view (its close is redirected to its dock)
     * @param event Event to inspect
     * @return true if the event was consumed
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Marks the stretch of code in which we are the ones changing a dock's
    /// visibility, so the handler can tell that apart from the user clicking a
    /// tab.  Deliberately not a QSignalBlocker: the chrome update still has to
    /// run, only the focus move must not.
    struct ShowGuard {
        bool &flag;
        explicit ShowGuard(bool &f) : flag(f) { flag = true; }
        ~ShowGuard() { flag = false; }
        ShowGuard(const ShowGuard &)            = delete;
        ShowGuard &operator=(const ShowGuard &) = delete;
    };

    /// Let a widget that just became a dock panel follow its dock area: drop the
    /// minimum size it and its layout ask for, hand its conflicting shortcuts to
    /// the main window, and make it take a click focus.
    void prepareDockedView(QWidget *view);

    /// Disable the shortcuts of a widget that just became a dock panel and
    /// whose sequences the main window menus already bind.
    void deferShortcutsToMainWindow(QWidget *view);

    /// Give a panel that is alone in its area its title bar back, and take it
    /// away again once a tab names it (Qt draws no tab bar for a single dock).
    void updateDockChrome();

    /// Drop the widget from whichever slot holds it (connected to its
    /// QObject::destroyed signal, so a slot never keeps a dangling pointer).
    void forget(QObject *view);

    /// Ask for the stored proportions to be applied at the end of the current
    /// event handling; coalesced, so placing several views costs one pass.
    void scheduleSplit();

    /// Apply the stored proportions.  Deferred through scheduleSplit(), because
    /// resizeDocks() does nothing before the docks are laid out.
    void applySplit();

    /// Build the two title-bar stand-ins a dock switches between, as named
    /// children of it, and start out with the collapsed one.
    void makeDockChrome(QDockWidget *d, const QString &title);

    /// A visible dock of a group, or nullptr if the whole group is hidden;
    /// resizeDocks() needs one to set the size the group shares.
    QDockWidget *sizingDock(std::initializer_list<ViewSlot> group) const;

    /// Build the dock widgets, arrange them, and restore a saved arrangement.
    void createDocks();

    /// The panels that are on screen, in the order focusNextPane() walks them:
    /// the group on the right before the one across the bottom, and within a
    /// group the fixed slots before the transient viewers tabbed into it.
    QList<QDockWidget *> orderedPanels() const;

    /// The dock holding a slot, or nullptr in windowed mode.
    QDockWidget *dock(ViewSlot slot) const { return docks[static_cast<int>(slot)]; }

    /// The widget whose visibility represents a slot: the dock when docked,
    /// the view itself otherwise.
    QWidget *presenter(ViewSlot slot) const;

    QMainWindow *mainwindow;   ///< Main window the views are shown in or docked into
    LayoutMode layoutmode;     ///< Presentation policy chosen at construction
    bool splitpending = false; ///< An application of the proportions is scheduled
    bool applying     = false; ///< Resizing the docks ourselves, so do not track it
    bool showing      = false; ///< Changing a dock's visibility ourselves
    double hsplit     = 0.0;   ///< Fraction of the width held by the right hand group
    double vsplit     = 0.0;   ///< Fraction of the height held by the bottom group

    QWidget *views[static_cast<int>(ViewSlot::Count)]{};     ///< Widget in each slot
    QDockWidget *docks[static_cast<int>(ViewSlot::Count)]{}; ///< Dock per slot (docked mode only)
    QList<QDockWidget *> auxdocks; ///< Docks of the transient views, in creation order
    int auxcounter = 0;            ///< Serial number for their object names
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
