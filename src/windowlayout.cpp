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

#include "windowlayout.h"

#include "constants.h"
#include "helpers.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QPointer>
#include <QResizeEvent>
#include <QSettings>
#include <QShortcut>
#include <QString>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

namespace {

constexpr int TAB_TITLE_MARGIN = 2;
// the chrome widgets are found on the dock by name, so a transient dock needs
// no bookkeeping of its own
const QString EMPTY_TITLE_NAME = QStringLiteral("dockEmptyTitle");
const QString TAB_TITLE_NAME   = QStringLiteral("dockTabTitle");

// Settings key recording the visibility of a slot, empty for the slots that
// have no "show by default" preference.  Only the keys listed here are written
// back when a view is toggled from the View menu.
QString visibilityKey(ViewSlot slot)
{
    switch (slot) {
        case ViewSlot::Log:
            return Keys::VIEWLOG;
        case ViewSlot::Chart:
            return Keys::VIEWCHART;
        default:
            return {};
    }
}

// Short label for the dock tab.  The views set a window title that names the
// input file and the run number, which is useful for a task bar entry but far
// too long for a tab, so the docked layout uses these instead.
QString dockTitle(ViewSlot slot)
{
    switch (slot) {
        case ViewSlot::Log:
            return QStringLiteral("Output");
        case ViewSlot::Chart:
            return QStringLiteral("Charts");
        case ViewSlot::Image:
            return QStringLiteral("Image");
        case ViewSlot::SlideShow:
            return QStringLiteral("Slide Show");
        case ViewSlot::Variables:
            return QStringLiteral("Variables");
        case ViewSlot::Command:
            return QStringLiteral("Commands");
        default:
            return {};
    }
}

// Stable object name; QMainWindow::saveState()/restoreState() match the docks
// of a saved arrangement to the existing ones by this name.
QString dockObjectName(ViewSlot slot)
{
    return QStringLiteral("dock_") + dockTitle(slot).remove(' ').toLower();
}

// Whether the keyboard focus is in a view: on it, or on any widget it holds.
bool holdsFocus(const QWidget *view)
{
    const QWidget *focus = QApplication::focusWidget();
    return view && focus && ((view == focus) || view->isAncestorOf(focus));
}

// Qt draws a tab bar only once two docks share an area.  A panel that is alone
// therefore gets this instead of the plain title bar: a label drawn like the
// single tab it stands in for, with the frame open at the bottom towards the
// panel it labels.
QWidget *makeTabTitle(QWidget *parent, const QString &title)
{
    auto *bar    = new QWidget(parent);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(TAB_TITLE_MARGIN, TAB_TITLE_MARGIN, TAB_TITLE_MARGIN, 0);
    layout->setSpacing(0);

    bar->setObjectName(TAB_TITLE_NAME);
    auto *label = new QLabel(title, bar);
    label->setObjectName("dockTabTitleLabel");
    label->setStyleSheet("QLabel#dockTabTitleLabel {"
                         "  border: 1px solid palette(dark);"
                         "  border-bottom: none;"
                         "  border-top-left-radius: 4px;"
                         "  border-top-right-radius: 4px;"
                         "  padding: 3px 12px;"
                         "  background: palette(button);"
                         "}");
    layout->addWidget(label);
    layout->addStretch();
    return bar;
}

} // namespace

WindowLayout::WindowLayout(QMainWindow *_mainwindow, LayoutMode mode) :
    QObject(_mainwindow), mainwindow(_mainwindow), layoutmode(mode)
{
    if (layoutmode == LayoutMode::Docked && mainwindow) createDocks();
}

WindowLayout::~WindowLayout() = default;

void WindowLayout::createDocks()
{
    const ShowGuard guard(showing);

    // the bottom area spans the full window width, so the log sits underneath
    // the editor *and* the right hand group rather than beside them
    mainwindow->setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    mainwindow->setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    // the tab already names the view, so the docks carry no title bar of their
    // own; put the tabs on top, where a tab bar is normally looked for
    mainwindow->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    auto make = [this](ViewSlot slot, Qt::DockWidgetArea area) {
        auto *d = new QDockWidget(dockTitle(slot), mainwindow);
        d->setObjectName(dockObjectName(slot));
        d->setAllowedAreas(Qt::AllDockWidgetAreas);
        // the panels have fixed places, so they are neither dragged nor floated;
        // the View menu shows and hides them
        d->setFeatures(QDockWidget::NoDockWidgetFeatures);
        // updateDockChrome() decides per dock whether its name is carried by a
        // tab or by a title bar, and needs this widget to collapse the latter
        makeDockChrome(d, dockTitle(slot));
        connect(d, &QDockWidget::visibilityChanged, this, [this, d](bool visible) {
            updateDockChrome();
            // Clicking a tab raises its dock without moving the keyboard focus,
            // so nothing would tell the menu bar that a different panel is now
            // in front.  Move the focus there, which also puts the panel's own
            // shortcuts in scope -- but only when the change came from the user
            // and not from us showing a view as a run produces it.
            if (!visible || showing) return;
            if (auto *w = d->widget()) {
                w->setFocus(Qt::OtherFocusReason);
                emit viewActivated(w);
            }
        });
        mainwindow->addDockWidget(area, d);
        // an empty dock would be a blank panel; the views show themselves as
        // they are created, through place() and the usual show()/hide() calls
        d->hide();
        docks[static_cast<int>(slot)] = d;
        return d;
    };

    // charts, image and slide show share the tab group on the right
    auto *chartDock = make(ViewSlot::Chart, Qt::RightDockWidgetArea);
    auto *imageDock = make(ViewSlot::Image, Qt::RightDockWidgetArea);
    auto *slideDock = make(ViewSlot::SlideShow, Qt::RightDockWidgetArea);
    mainwindow->tabifyDockWidget(chartDock, imageDock);
    mainwindow->tabifyDockWidget(imageDock, slideDock);

    // log, variables and the command prompt share the group across the bottom
    auto *logDock = make(ViewSlot::Log, Qt::BottomDockWidgetArea);
    auto *varDock = make(ViewSlot::Variables, Qt::BottomDockWidgetArea);
    auto *cmdDock = make(ViewSlot::Command, Qt::BottomDockWidgetArea);
    mainwindow->tabifyDockWidget(logDock, varDock);
    mainwindow->tabifyDockWidget(varDock, cmdDock);

    mainwindow->installEventFilter(this);
    // watch the docks, so the cached fractions follow a splitter the user drags
    // and not just a resize of the window
    for (auto *d : docks)
        if (d) d->installEventFilter(this);

    // a saved arrangement wins over the default one above; it is matched to
    // these docks by object name, which is why they all exist by now
    QSettings settings;
    // an arrangement stored before the key was qualified by Qt version may have
    // been written by any Qt release and restoring it can crash; see constants.h
    settings.remove(Keys::DOCKSTATE_LEGACY);
    const QByteArray state = settings.value(Keys::DOCKSTATE).toByteArray();
    if (!state.isEmpty() && mainwindow->restoreState(state, Cfg::DOCK_STATE_VERSION)) {
        // restoreState() also restores visibility, but a dock that has no view
        // in it yet must not show as an empty panel
        for (auto *d : docks)
            if (d && !d->widget()) d->hide();
    }

    // The proportions are kept separately rather than left to restoreState():
    // that runs while the docks are still empty, so the sizes it restores are
    // replaced by the size hint of each view as soon as one is put in.  They are
    // re-applied from show() once a view is actually there -- resizeDocks() has
    // no effect before the docks are laid out and visible anyway.
    hsplit = settings.value(Keys::DOCKSPLITH, Cfg::DOCK_SPLIT_HORIZONTAL).toDouble();
    vsplit = settings.value(Keys::DOCKSPLITV, Cfg::DOCK_SPLIT_VERTICAL).toDouble();
    scheduleSplit();
}

// The docks of one group share a size, so any visible one of them can be
// resized to set it -- but only a visible one: resizeDocks() ignores a hidden
// dock, and which member of a group is up varies with what the run produced.
void WindowLayout::makeDockChrome(QDockWidget *d, const QString &title)
{
    auto *empty = new QWidget(d);
    empty->setObjectName(EMPTY_TITLE_NAME);
    // An empty title bar is how a dock is told to have none, but a bare QWidget
    // has no layout and so reports an *invalid* size hint of (-1,-1) -- and Qt
    // takes the height of the title bar from exactly that hint.  The -1 travels
    // into the dock's own minimum height, where it surfaces as "Negative sizes
    // (0,-1) are not possible".  An empty layout costs nothing and makes the
    // hint the (0,0) that was meant all along.
    auto *nothing = new QHBoxLayout(empty);
    nothing->setContentsMargins(0, 0, 0, 0);
    nothing->setSpacing(0);
    // The tab title is not installed yet, and a bare child that was never
    // explicitly hidden becomes visible along with the dock -- floating over
    // the view at its default geometry, where it shines through a view that
    // paints no background of its own.  Start it hidden; updateDockChrome()
    // shows it if and when it makes it the title bar.
    makeTabTitle(d, title)->hide();
    d->setTitleBarWidget(empty);
}

QDockWidget *WindowLayout::sizingDock(std::initializer_list<ViewSlot> group) const
{
    // note: "slots" is a Qt keyword macro and cannot be used as a name here
    for (auto slot : group) {
        auto *d = dock(slot);
        if (d && d->isVisible()) return d;
    }
    return nullptr;
}

// Coalesce into a single application at the end of the current event handling:
// several views can be placed in one go, and resizeDocks() has no effect until
// the docks holding them are laid out.
void WindowLayout::scheduleSplit()
{
    if (layoutmode != LayoutMode::Docked || !mainwindow || splitpending) return;
    splitpending = true;
    QTimer::singleShot(0, mainwindow, [this]() {
        applySplit();
    });
}

void WindowLayout::applySplit()
{
    splitpending = false;
    if (!mainwindow) return;

    // the resizes below are not the user changing the split, so keep the event
    // filter from recording them as a new target
    applying         = true;
    auto *rightDock  = sizingDock({ViewSlot::Chart, ViewSlot::Image, ViewSlot::SlideShow});
    auto *bottomDock = sizingDock({ViewSlot::Log, ViewSlot::Variables, ViewSlot::Command});
    // the transient viewers join the right hand group too, and once the fixed
    // panels of that group are closed one of them is all it holds
    if (!rightDock)
        for (auto *d : auxdocks)
            if (d && d->isVisible()) {
                rightDock = d;
                break;
            }
    if (rightDock)
        mainwindow->resizeDocks({rightDock}, {static_cast<int>(mainwindow->width() * hsplit)},
                                Qt::Horizontal);
    if (bottomDock)
        mainwindow->resizeDocks({bottomDock}, {static_cast<int>(mainwindow->height() * vsplit)},
                                Qt::Vertical);
    applying = false;
}

// Qt only draws a tab bar once two dock widgets share an area, so a panel that
// is alone would end up with no visible name at all.  Give such a panel its
// title bar back and take it away again as soon as a tab names it.
void WindowLayout::updateDockChrome()
{
    if (layoutmode != LayoutMode::Docked || !mainwindow) return;

    QList<QDockWidget *> all;
    for (auto *d : docks)
        if (d) all << d;
    all += auxdocks;

    for (auto *d : all) {
        bool tabbed = false;
        for (const auto *sibling : mainwindow->tabifiedDockWidgets(d)) {
            if (sibling && sibling->isVisible()) {
                tabbed = true;
                break;
            }
        }
        QWidget *wanted = d->findChild<QWidget *>(tabbed ? EMPTY_TITLE_NAME : TAB_TITLE_NAME,
                                                  Qt::FindDirectChildrenOnly);
        if (wanted && d->titleBarWidget() != wanted) {
            // the one being replaced stays owned by the dock, so it can be
            // handed back and forth without leaking
            if (auto *previous = d->titleBarWidget()) previous->hide();
            d->setTitleBarWidget(wanted);
            wanted->show();
        }
    }
}

// keep the dock proportions across a resize of the main window: the sizes
// before the resize are relative to the old size, so re-applying the same
// fractions preserves whatever split the user last set.
bool WindowLayout::eventFilter(QObject *watched, QEvent *event)
{
    // Only once the window is on screen and the default proportions have been
    // applied: during start-up the docks are not laid out yet, so their size is
    // still zero and the fraction computed from it would be zero too -- which
    // this would then enforce, collapsing the panel for good.
    if (event->type() == QEvent::Resize && layoutmode == LayoutMode::Docked && !splitpending &&
        !applying && mainwindow && mainwindow->isVisible()) {
        // a dock changed size under the user's hands: record what fraction of
        // the window its group now holds
        for (int i = 0; i < static_cast<int>(ViewSlot::Count); ++i) {
            const auto *d = docks[i];
            if (d != watched || !d || d->isHidden()) continue;
            const auto slot = static_cast<ViewSlot>(i);
            if (slot == ViewSlot::Log || slot == ViewSlot::Variables || slot == ViewSlot::Command) {
                if (d->height() > 0 && mainwindow->height() > 0)
                    vsplit = static_cast<double>(d->height()) / mainwindow->height();
            } else {
                if (d->width() > 0 && mainwindow->width() > 0)
                    hsplit = static_cast<double>(d->width()) / mainwindow->width();
            }
            break;
        }
        // and the same for a transient viewer, which is always in the right hand
        // group, so that a splitter dragged while one of them is in front is
        // remembered like any other
        for (const auto *d : auxdocks) {
            if ((d != watched) || d->isHidden()) continue;
            if ((d->width() > 0) && (mainwindow->width() > 0))
                hsplit = static_cast<double>(d->width()) / mainwindow->width();
            break;
        }
    }

    // Docked, a view is a child of its dock, so closing the view -- from its
    // Close entry or with the shortcut -- hid the widget and left the dock, and
    // with it the tab, behind and empty.  The dock is what has to go, so send
    // the close there instead.  Only the fixed slots need this: a transient
    // viewer deletes itself when closed and its dock follows it out.
    if (event->type() == QEvent::Close && layoutmode == LayoutMode::Docked) {
        for (int i = 0; i < static_cast<int>(ViewSlot::Count); ++i) {
            if (views[i] != watched) continue;
            event->ignore(); // the widget stays, so the dock can show it again
            hide(static_cast<ViewSlot>(i));
            return true;
        }
    }

    if (watched == mainwindow && event->type() == QEvent::Resize &&
        layoutmode == LayoutMode::Docked && !splitpending && mainwindow->isVisible()) {
        auto *re            = static_cast<QResizeEvent *>(event);
        const QSize oldsize = re->oldSize();
        const QSize newsize = re->size();

        applying         = true;
        auto *rightDock  = sizingDock({ViewSlot::Chart, ViewSlot::Image, ViewSlot::SlideShow});
        auto *bottomDock = sizingDock({ViewSlot::Log, ViewSlot::Variables, ViewSlot::Command});
        if (oldsize.width() > 0 && newsize.width() != oldsize.width() && rightDock)
            mainwindow->resizeDocks({rightDock}, {static_cast<int>(newsize.width() * hsplit)},
                                    Qt::Horizontal);
        if (oldsize.height() > 0 && newsize.height() != oldsize.height() && bottomDock)
            mainwindow->resizeDocks({bottomDock}, {static_cast<int>(newsize.height() * vsplit)},
                                    Qt::Vertical);
        applying = false;
    }
    return QObject::eventFilter(watched, event);
}

// A transient view -- a file viewer -- gets a dock of its own, tabbed into the
// group of an existing panel.  It is not one of the fixed slots: several can be
// open at once and each lives only as long as its widget, so the dock follows
// the widget out.
void WindowLayout::addAuxiliaryView(QWidget *view, ViewSlot group, const QString &title)
{
    if (!view) return;
    // A transient view is closed for good, and it is the destruction of the
    // widget that takes the dock -- the tab -- with it (see below).  Not every
    // one of them was built to delete itself, though, and one that only hides
    // left its dock behind: an empty tab, or an empty panel that collapses the
    // group and takes the tabs of the views beside it out of reach.  As a
    // window of its own it lingered instead, hidden, until the application
    // ended.  So this is settled here, for whatever is made a transient view,
    // rather than left to each maker to remember.
    view->setAttribute(Qt::WA_DeleteOnClose);
    if (layoutmode != LayoutMode::Docked || !mainwindow) {
        view->show();
        return;
    }

    const ShowGuard guard(showing);
    auto *d = new QDockWidget(title, mainwindow);
    // named so QMainWindow does not complain when the arrangement is saved; a
    // stale entry for a viewer that is gone is simply ignored on restore
    d->setObjectName(QStringLiteral("dock_aux_%1").arg(++auxcounter));
    d->setAllowedAreas(Qt::AllDockWidgetAreas);
    d->setFeatures(QDockWidget::DockWidgetClosable);
    makeDockChrome(d, title);
    mainwindow->addDockWidget(Qt::RightDockWidgetArea, d);
    if (auto *sibling = dock(group)) mainwindow->tabifyDockWidget(sibling, d);

    d->setWidget(view);
    prepareDockedView(view);
    auxdocks << d;
    // watched and sized like the fixed panels: it shares their group, so it
    // follows the same proportions and a splitter dragged over it is recorded
    d->installEventFilter(this);
    scheduleSplit();

    connect(d, &QDockWidget::visibilityChanged, this, [this, d](bool visible) {
        updateDockChrome();
        if (!visible || showing) return;
        if (auto *w = d->widget()) {
            w->setFocus(Qt::OtherFocusReason);
            emit viewActivated(w);
        }
    });
    // The viewers delete themselves when closed, so the dock goes with them --
    // held weakly, because on the way out the main window may take the dock
    // first and destroy the view it holds along with it.  In that case this
    // fires from inside the dock's own teardown, where its dynamic type has
    // already decayed to QWidget: a QPointer to the QDockWidget must not be
    // formed then (its typed conversion is a downcast the sanitizer rightly
    // flags), so liveness is tracked at the QObject level and the pointer
    // value for the bookkeeping is captured separately, compared but never
    // dereferenced.
    connect(view, &QObject::destroyed, this, [this, dock = d, alive = QPointer<QObject>(d)]() {
        if (!alive) return;
        auxdocks.removeAll(dock);
        alive->deleteLater();
        // closing it frees space in the group, which the panels that
        // stay would otherwise divide up by size hint
        scheduleSplit();
    });

    d->show();
    d->raise();
    updateDockChrome();
    emit viewActivated(view);
}

void WindowLayout::saveState() const
{
    if (layoutmode != LayoutMode::Docked || !mainwindow) return;
    QSettings settings;
    settings.setValue(Keys::DOCKSTATE, mainwindow->saveState(Cfg::DOCK_STATE_VERSION));

    // Store the proportions alongside it; see createDocks() for why.  The cached
    // values are used rather than the current geometry: this runs while the main
    // window is on its way out, where the widget sizes no longer reflect the
    // layout the user was looking at.
    if (hsplit > 0.0) settings.setValue(Keys::DOCKSPLITH, hsplit);
    if (vsplit > 0.0) settings.setValue(Keys::DOCKSPLITV, vsplit);
}

// A dock panel lives inside the main window, so a sequence it binds is in scope
// at the same time as the main window's own binding for it and Qt fires neither.
// Leave those to the menu: the panel's menu entry still works, only its
// accelerator goes.  This is decided here, when a widget actually becomes a
// panel, rather than when it is built -- a view that stays a window of its own
// (the file viewers, the find dialog, the standalone viewer modes) has no
// ambiguity and keeps everything.
// A dock area sizes its panel, so the view has to be able to follow it down.
// Its own minimum, and the one its layout derives from all of its controls,
// would otherwise be a floor on the whole dock area -- for the charts view wide
// enough to push the editor to its minimum and make the split unreachable.
void WindowLayout::prepareDockedView(QWidget *view)
{
    if (!view) return;
    // watched for the close it may send itself; see eventFilter()
    view->installEventFilter(this);
    view->setMinimumSize(0, 0);
    if (auto *l = view->layout()) l->setSizeConstraint(QLayout::SetNoConstraint);
    deferShortcutsToMainWindow(view);
    // clicking a label or a plot would otherwise not move the keyboard focus,
    // and the main window's menu bar follows the focus
    view->setFocusPolicy(Qt::ClickFocus);
}

void WindowLayout::deferShortcutsToMainWindow(QWidget *view)
{
    if (!view) return;
    for (auto *shortcut : view->findChildren<QShortcut *>())
        if (isMainWindowShortcut(shortcut->key())) shortcut->setEnabled(false);
    for (auto *action : view->findChildren<QAction *>())
        if (isMainWindowShortcut(action->shortcut())) action->setShortcut(QKeySequence());
}

void WindowLayout::place(ViewSlot slot, QWidget *view)
{
    const int idx = static_cast<int>(slot);
    if (views[idx] == view) return;

    if (views[idx]) disconnect(views[idx], &QObject::destroyed, this, nullptr);
    views[idx] = view;
    // the widgets are owned by LammpsGui, so the slot has to be emptied when
    // one of them is deleted rather than at some point of our choosing
    if (view) connect(view, &QObject::destroyed, this, &WindowLayout::forget);

    const ShowGuard guard(showing);
    if (auto *d = dock(slot)) {
        prepareDockedView(view);
        // only the content changes; the dock keeps its area and tab position,
        // so a view that is rebuilt does not move
        d->setWidget(view);
        if (!view) d->hide();
        updateDockChrome();
        // a newly built view brings its own size hint into the dock area, which
        // would otherwise take the split with it
        scheduleSplit();
    }
}

QWidget *WindowLayout::view(ViewSlot slot) const
{
    return views[static_cast<int>(slot)];
}

QWidget *WindowLayout::presenter(ViewSlot slot) const
{
    if (auto *d = dock(slot)) return views[static_cast<int>(slot)] ? d : nullptr;
    return views[static_cast<int>(slot)];
}

void WindowLayout::forget(QObject *view)
{
    for (int i = 0; i < static_cast<int>(ViewSlot::Count); ++i) {
        if (views[i] != view) continue;
        views[i] = nullptr;
        // the dock's content went away with it; keep the dock but empty
        if (docks[i]) docks[i]->hide();
    }
}

void WindowLayout::show(ViewSlot slot)
{
    auto *w = presenter(slot);
    if (!w) return;
    const ShowGuard guard(showing);
    // before the change and not after it: scheduling first is what marks the
    // resizes it causes as ours, so the event filter does not take them for the
    // user having moved a splitter
    scheduleSplit();
    w->show();

    // deliberately no raise() here: this runs on every periodic update during a
    // run (each new dump image shows the slide show view), and raising would
    // pull the tab group away from whatever the user is looking at
}

void WindowLayout::raise(ViewSlot slot)
{
    auto *w = presenter(slot);
    if (!w) return;
    show(slot);
    // in a tab group showing a dock leaves it behind its siblings
    if (auto *d = dock(slot))
        d->raise();
    else
        w->raise();

    // An explicit request to see this view, so put the keyboard focus there as
    // well: showing a panel as a run produces it deliberately does not, but
    // asking for one from a menu should leave it ready to be typed into.  The
    // focus belongs on the view, not on the dock holding it -- a QDockWidget
    // takes none itself.
    if (auto *v = view(slot)) v->setFocus(Qt::OtherFocusReason);
    if (layoutmode == LayoutMode::Docked) emit viewActivated(view(slot));
}

void WindowLayout::hide(ViewSlot slot)
{
    const ShowGuard guard(showing);
    // Closing a panel re-lays out the ones that stay, and Qt hands the freed
    // space out by size hint rather than by the proportions the window was set
    // to -- so the split moved whenever a panel was closed, and stayed moved,
    // because the event filter recorded that transient geometry as the new
    // target.  Scheduling before the hide covers both halves: it makes the
    // resizes ours, and it puts the proportions back once the layout settles.
    scheduleSplit();
    if (auto *w = presenter(slot)) w->hide();
}

void WindowLayout::setVisible(ViewSlot slot, bool visible)
{
    if (visible)
        show(slot);
    else
        hide(slot);
}

bool WindowLayout::isVisible(ViewSlot slot) const
{
    const auto *w = presenter(slot);
    return w && w->isVisible();
}

QList<QDockWidget *> WindowLayout::orderedPanels() const
{
    QList<QDockWidget *> all;
    for (auto *d : docks)
        if (d) all << d;
    all += auxdocks;

    // by dock area rather than by slot, so a transient viewer is walked where
    // it actually sits and the order follows what is on the screen
    QList<QDockWidget *> panels;
    for (auto area : {Qt::RightDockWidgetArea, Qt::BottomDockWidgetArea, Qt::LeftDockWidgetArea,
                      Qt::TopDockWidgetArea})
        for (auto *d : all)
            if (d->isVisible() && d->widget() && (mainwindow->dockWidgetArea(d) == area))
                panels << d;
    return panels;
}

void WindowLayout::focusNextPane(bool forward)
{
    if ((layoutmode != LayoutMode::Docked) || !mainwindow) return;

    // the editor is a pane like the panels are, and it is the one the walk
    // starts from when the focus is somewhere that belongs to none of them
    auto *editor      = mainwindow->centralWidget();
    const auto panels = orderedPanels();
    const int count   = panels.size() + 1;
    if (count < 2) return; // nothing but the editor

    int current = 0;
    for (int i = 0; i < panels.size(); ++i)
        if (holdsFocus(panels[i])) {
            current = i + 1;
            break;
        }

    const int next = (current + (forward ? 1 : count - 1)) % count;
    if (next == 0) {
        if (editor) editor->setFocus(Qt::OtherFocusReason);
        return;
    }
    auto *d = panels[next - 1];
    d->raise(); // it shares a tab group, so being shown is not being in front
    // the focus belongs on the view, not on the dock, which takes none itself
    if (auto *w = d->widget()) w->setFocus(Qt::OtherFocusReason);
}

bool WindowLayout::toggle(ViewSlot slot)
{
    if (!presenter(slot)) return false;

    // Docked, a panel has three states rather than two: it can be on screen
    // without being the one that is worked in.  Asking for it then means going
    // to it, not dismissing it, so the key that opened a panel is also the key
    // that returns to it -- and pressing it twice still gets rid of it, because
    // the first press is what put the focus there.  With individual windows the
    // stacking and the focus belong to the window manager, so the plain toggle
    // stays: a raise that the window manager declines would leave the key with
    // nothing to hide.
    if ((layoutmode == LayoutMode::Docked) && isVisible(slot) && !holdsFocus(view(slot))) {
        raise(slot);
        return true;
    }

    const bool visible = !isVisible(slot);
    // an explicit request from the View menu: bring it to the front of its tab
    // group, otherwise turning it "on" would appear to do nothing
    if (visible)
        raise(slot);
    else
        hide(slot);

    const QString key = visibilityKey(slot);
    if (!key.isEmpty()) QSettings().setValue(key, visible);
    return visible;
}

// Local Variables:
// c-basic-offset: 4
// End:
