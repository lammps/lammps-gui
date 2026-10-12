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

#include <gtest/gtest.h>

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFont>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QWidget>

#include <memory>

// WindowLayout is the presentation policy for the output views.  Docked, a view
// is a child of a dock widget rather than a window of its own, and two things
// that are obvious for a window are not obvious there: closing the view has to
// take the dock with it, and the dock's title bar is a widget we supply.  These
// tests pin both down.
class WindowLayoutTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_windowlayout"};
            app                 = new QApplication(argc, argv);
        }
        // WindowLayout reads and writes QSettings; send those to a throw-away
        // directory so a test run cannot touch the real configuration
        QCoreApplication::setOrganizationName("LAMMPS-GUI-Test");
        QCoreApplication::setApplicationName("test_windowlayout");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        settingsdir = new QTemporaryDir;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsdir->path());
    }

    static void TearDownTestSuite()
    {
        delete settingsdir;
        settingsdir = nullptr;
    }

    static QApplication *app;
    static QTemporaryDir *settingsdir;
};

QApplication *WindowLayoutTest::app          = nullptr;
QTemporaryDir *WindowLayoutTest::settingsdir = nullptr;

// A bare QWidget has no layout and therefore an *invalid* size hint of (-1,-1).
// Qt takes the height of a dock's title bar straight from that hint, so using
// one as the "no title bar" placeholder puts a -1 into the dock's own minimum
// height, where it surfaces as "QWidget::setMinimumSize: Negative sizes (0,-1)
// are not possible".
TEST_F(WindowLayoutTest, DockTitleBarsHaveAValidSizeHint)
{
    QMainWindow window;
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked);

    // two panels of one group, so they are tabbed: a dock that is alone keeps a
    // title bar naming it, and only a tabbed one is given the empty placeholder
    layout.place(ViewSlot::Log, new QPlainTextEdit);
    layout.place(ViewSlot::Command, new QPlainTextEdit);
    layout.show(ViewSlot::Log);
    layout.show(ViewSlot::Command);
    QApplication::processEvents();

    const auto docks = window.findChildren<QDockWidget *>();
    ASSERT_FALSE(docks.isEmpty());
    for (const auto *dock : docks) {
        const auto *title = dock->titleBarWidget();
        if (!title) continue;
        EXPECT_TRUE(title->sizeHint().isValid())
            << "dock " << qPrintable(dock->objectName()) << " has a title bar of size ("
            << title->sizeHint().width() << "," << title->sizeHint().height() << ")";
        EXPECT_GE(dock->minimumSizeHint().height(), 0);
    }
}

// Closing a docked view used to hide the widget and leave the dock -- and with
// it the tab naming it, and the space it holds -- behind and empty.
TEST_F(WindowLayoutTest, ClosingADockedViewClosesItsDock)
{
    QMainWindow window;
    // a dock of a window that was never shown reports itself hidden either way
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked);

    auto *view = new QPlainTextEdit;
    layout.place(ViewSlot::Log, view);
    layout.show(ViewSlot::Log);
    QApplication::processEvents();
    ASSERT_TRUE(layout.isVisible(ViewSlot::Log));

    view->close();
    QApplication::processEvents();

    EXPECT_FALSE(layout.isVisible(ViewSlot::Log));
    // the widget itself stays, so showing the panel again brings it back
    EXPECT_FALSE(view->isHidden());
}

// A transient viewer is closed for good, unlike a fixed panel that is only put
// away: the widget goes, and its dock -- the tab naming it -- with it.  Left
// behind, that dock was an empty panel which collapsed the group it shared and
// took the tabs of the views beside it out of reach.
TEST_F(WindowLayoutTest, ClosingATransientViewRemovesItsDock)
{
    QMainWindow window;
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked);

    const int fixeddocks = window.findChildren<QDockWidget *>().size();
    auto *first          = new QPlainTextEdit;
    auto *second         = new QPlainTextEdit;
    layout.addAuxiliaryView(first, ViewSlot::Chart, "first");
    layout.addAuxiliaryView(second, ViewSlot::Chart, "second");
    QApplication::processEvents();
    ASSERT_EQ(window.findChildren<QDockWidget *>().size(), fixeddocks + 2);

    const QPointer<QPlainTextEdit> closed = first;
    first->close();
    QApplication::processEvents();
    // the widget deletes itself, and the dock follows on the next round
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_TRUE(closed.isNull());
    EXPECT_EQ(window.findChildren<QDockWidget *>().size(), fixeddocks + 1);
    // the panel it shared the group with is untouched
    EXPECT_FALSE(second->isHidden());
}

// Undocked, a view is a window and closing it is Qt's business, not ours.
TEST_F(WindowLayoutTest, ClosingAFloatingViewIsLeftAlone)
{
    WindowLayout layout(nullptr, LayoutMode::Windows);

    auto *view = new QPlainTextEdit;
    layout.place(ViewSlot::Log, view);
    layout.show(ViewSlot::Log);
    ASSERT_TRUE(layout.isVisible(ViewSlot::Log));

    view->close();

    EXPECT_FALSE(layout.isVisible(ViewSlot::Log));
    EXPECT_TRUE(view->isHidden());
    delete view;
}

// The saved dock arrangement is a QMainWindow::saveState() blob whose format
// belongs to the Qt release that reads it back, and restoring one written by a
// different feature release can crash.  The settings key therefore carries the
// running Qt's feature version -- and only that, so a patch-level update does
// not silently discard the arrangement the user set up.
TEST_F(WindowLayoutTest, DockStateKeyIsQualifiedByQtFeatureVersion)
{
    const QString version = QString::fromLatin1(qVersion());
    const QString feature = version.section('.', 0, 1); // "6.9" out of "6.9.1"
    ASSERT_FALSE(feature.isEmpty());

    EXPECT_EQ(Keys::DOCKSTATE, QString("dockstate_%1").arg(feature));

    // it must not be the unqualified key any more, nor carry the patch level
    EXPECT_NE(Keys::DOCKSTATE, Keys::DOCKSTATE_LEGACY);
    if (version != feature) {
        EXPECT_FALSE(Keys::DOCKSTATE.contains(version));
    }
}

// The arrangement has to go out under the versioned key, and an unversioned one
// left over from before -- written by an unknown Qt, so unsafe to restore -- has
// to be gone rather than sit there waiting to be picked up.
TEST_F(WindowLayoutTest, SavingUsesTheVersionedKeyAndDropsTheLegacyOne)
{
    {
        QSettings settings;
        settings.setValue(Keys::DOCKSTATE_LEGACY, QByteArray("stale-blob-from-some-other-qt"));
        settings.remove(Keys::DOCKSTATE);
        settings.sync();
    }

    QMainWindow window;
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked); // reads, and drops the legacy key
    layout.place(ViewSlot::Log, new QPlainTextEdit);
    layout.show(ViewSlot::Log);
    QApplication::processEvents();
    layout.saveState();

    QSettings settings;
    settings.sync();
    EXPECT_FALSE(settings.contains(Keys::DOCKSTATE_LEGACY));
    EXPECT_FALSE(settings.value(Keys::DOCKSTATE).toByteArray().isEmpty());
}

// Closing a panel used to move the splitter and leave it moved.  Qt divides the
// space a hidden panel frees among the ones that stay by their size hints, and
// the event filter then recorded that geometry as if the user had dragged the
// splitter there, so the proportions drifted a little further with every panel
// that was opened and closed.
TEST_F(WindowLayoutTest, ClosingAPanelKeepsTheProportions)
{
    constexpr double kSplitH = 0.4;
    constexpr double kSplitV = 0.3;
    {
        QSettings settings;
        settings.setValue(Keys::DOCKSPLITH, kSplitH);
        settings.setValue(Keys::DOCKSPLITV, kSplitV);
        settings.remove(Keys::DOCKSTATE); // no saved arrangement to override them
        settings.sync();
    }

    QMainWindow window;
    window.resize(1200, 900);
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked);

    // two panels of the right hand group, with size hints far enough apart that
    // the group would resize itself when one of them goes away
    auto *wide = new QPlainTextEdit;
    wide->setMinimumWidth(500);
    auto *narrow = new QPlainTextEdit;
    narrow->setMinimumWidth(80);
    layout.place(ViewSlot::Chart, wide);
    layout.place(ViewSlot::SlideShow, narrow);
    layout.place(ViewSlot::Log, new QPlainTextEdit);
    layout.show(ViewSlot::Chart);
    layout.show(ViewSlot::SlideShow);
    layout.show(ViewSlot::Log);
    QApplication::processEvents();
    QApplication::processEvents(); // the split is applied from a zero timer

    layout.hide(ViewSlot::SlideShow);
    QApplication::processEvents();
    QApplication::processEvents();

    layout.saveState();
    QSettings settings;
    settings.sync();
    EXPECT_NEAR(settings.value(Keys::DOCKSPLITH).toDouble(), kSplitH, 0.02);
    EXPECT_NEAR(settings.value(Keys::DOCKSPLITV).toDouble(), kSplitV, 0.02);
}

// On the way out, a dock can be destroyed before the transient view it holds:
// the dock's own teardown then deletes the view, and the view's
// destroyed-handler runs while the dock is half destructed, its dynamic type
// already decayed to QWidget.  The handler must not form a QDockWidget pointer
// to it then -- that downcast is what the undefined-behavior sanitizer used to
// flag on quitting with an auxiliary tab open -- so it tracks liveness at the
// QObject level and never dereferences the typed pointer.
TEST_F(WindowLayoutTest, DockFirstTeardownOfATransientView)
{
    auto *window = new QMainWindow;
    window->show();
    // a QObject child of the window, deleted along with it after the docks
    auto *layout = new WindowLayout(window, LayoutMode::Docked);
    layout->place(ViewSlot::Chart, new QPlainTextEdit);
    layout->show(ViewSlot::Chart);
    layout->addAuxiliaryView(new QPlainTextEdit, ViewSlot::Chart, "aux");
    QApplication::processEvents();

    // the dock deletes the view from inside its own teardown, firing the
    // handler against the half-destructed dock
    const auto docks = window->findChildren<QDockWidget *>();
    for (auto *dock : docks)
        if (dock->objectName().startsWith("dock_aux")) delete dock;
    QApplication::processEvents();

    delete window; // takes the fixed docks and the layout with it
    QApplication::processEvents();
}

// Docked, a view inherits the main window's proportional font, and
// QPlainTextEdit adopts the inherited font as its document font: the mono font
// the view's constructor put on the document is silently replaced.  FileViewer
// and LogWindow therefore re-assert it from their changeEvent() overrides, and
// this pins the mechanism that fix relies on: becoming a dock's child fires
// FontChange, and reassertMonoFont() puts the configured fixed-width font back.
namespace {
class MonoKeepingView : public QPlainTextEdit {
protected:
    void changeEvent(QEvent *event) override
    {
        QPlainTextEdit::changeEvent(event);
        if (event->type() == QEvent::FontChange) reassertMonoFont(document());
    }
};
} // namespace

TEST_F(WindowLayoutTest, DockedViewKeepsItsMonoDocumentFont)
{
    // normally allocated in main(); monoFontFromSettings() falls back to it
    if (!GUI_MONOFONT) GUI_MONOFONT = std::make_unique<QFont>();

    QMainWindow window;
    // a main window font that cannot resolve equal to the views' default font,
    // so becoming its child is guaranteed to change theirs
    QFont prop = window.font();
    prop.setPointSize(prop.pointSize() + 2);
    window.setFont(prop);
    window.show();
    WindowLayout layout(&window, LayoutMode::Docked);

    auto *keeping = new MonoKeepingView;
    keeping->document()->setDefaultFont(monoFontFromSettings());
    auto *bare = new QPlainTextEdit;
    bare->document()->setDefaultFont(monoFontFromSettings());
    layout.addAuxiliaryView(keeping, ViewSlot::Chart, "keeping");
    layout.addAuxiliaryView(bare, ViewSlot::Chart, "bare");
    QApplication::processEvents();

    EXPECT_EQ(keeping->document()->defaultFont(), monoFontFromSettings());
    // without the override the font is lost -- and if this ever starts to
    // fail, Qt no longer clobbers the document font and the guard is moot
    EXPECT_NE(bare->document()->defaultFont(), monoFontFromSettings());
}

// Local Variables:
// c-basic-offset: 4
// End:
