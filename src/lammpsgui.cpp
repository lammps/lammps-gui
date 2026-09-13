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

#include "lammpsgui.h"

#include "aboutdialog.h"
#include "chartviewer.h"
#include "codeeditor.h"
#include "commandwindow.h"
#include "downloadprogress.h"
#include "fileviewer.h"
#include "findandreplace.h"
#include "helpers.h"
#include "highlighter.h"
#include "imageviewer.h"
#include "lammpsrunner.h"
#include "logwindow.h"
#include "plotdata.h"
#include "plotdatadialog.h"
#include "preferences.h"
#include "qaddon.h"
#include "setvariables.h"
#include "slideshow.h"
#include "stdcapture.h"
#include "syntaxcheck.h"
#include "tutorialwizard.h"
#include "urldownloader.h"
#include "windowlayout.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWizard>
#include <QWizardPage>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

#include "constants.h"
#include "tutorials.h"

namespace {

// read one thermo column value, converting from its native datatype
double lastThermoData(LammpsWrapper &lammps, int datatype, int column)
{
    if (datatype == 0) // int
        return lammps.lastThermoAs<int>("data", column);
    if (datatype == 2) // double
        return lammps.lastThermoAs<double>("data", column);
    if (datatype == 4) // bigint
        return static_cast<double>(lammps.lastThermoAs<int64_t>("data", column));
    return 0.0;
}

// export the https_proxy environment variable into the LAMMPS instance, taken
// from the environment or, failing that, from the preferences
void applyProxySetting(LammpsWrapper &lammps, QSettings &settings)
{
    auto proxy = QString::fromLocal8Bit(qgetenv("https_proxy"));
    if (proxy.isEmpty()) proxy = settings.value(Keys::HTTPS_PROXY, "").toString();
    if (!proxy.isEmpty()) lammps.command(QString("shell putenv https_proxy=") + proxy);
}

// Remove leftover files from replacing the downloaded LAMMPS shared library
// in the configuration folder: backups of an updated or reset library (which
// may have been locked and thus undeletable on Windows while it was loaded)
// and partial downloads left behind by a crash.  The names for all platforms
// are checked since the configuration folder may be shared between different
// machines.  The configured plugin file itself is never removed.
void purgeLibraryLeftovers()
{
    const auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) return;

    const QDir dir(configDir);
    const QString plugin =
        QFileInfo(QSettings().value(Keys::PLUGIN_PATH, "").toString()).canonicalFilePath();
    for (const auto &libname :
         {Cfg::LAMMPS_LIB_MACOS, Cfg::LAMMPS_LIB_WINDOWS, Cfg::LAMMPS_LIB_LINUX}) {
        // the "?*" glob requires at least one extra character, so the pattern
        // matches only backups and partial downloads, never the library itself
        for (const auto &f : dir.entryList({libname + "?*"}, QDir::Files)) {
            const QString path = dir.absoluteFilePath(f);
            if (!plugin.isEmpty() && (QFileInfo(path).canonicalFilePath() == plugin)) continue;
            QFile::remove(path);
        }
    }
}

const QString citeme("# When using LAMMPS-GUI in your project, please cite: "
                     "https://doi.org/10.33011/livecoms.6.1.3037\n");
const QString bannerstyle("CodeEditor {background-position: center center; "
                          "padding: 0px; "
                          "background-repeat: no-repeat; "
                          "background-image: url(:/icons/lammps-gui-banner.png);}");
} // namespace

void LammpsGui::setupUi(QSettings &settings, QFont &allFont, QFont &monoFont)
{
    setObjectName("LammpsGui");
    setWindowTitle("LAMMPS-GUI");
    setWindowIcon(QIcon(Cfg::MAIN_ICON));

    // set up central widget
    textEdit = new CodeEditor(this);
    textEdit->setSyntax(&syntax);
    textEdit->setEnabled(true);
    textEdit->setAcceptDrops(true);
    textEdit->setStyleSheet(bannerstyle);
    // combined layout: the editor shares the window with the dock areas, so a
    // minimum of its own is a floor under all of them
    if (!dockedLayout()) textEdit->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);

    // set up menu bar and menus with their actions and shortcuts
    menubar = new QMenuBar(this);
    createFileMenu();
    createEditMenu();
    createRunMenu();
    createViewMenu();
    createTutorialMenu();
    createAboutMenu();
    setMenuBar(menubar);

    // publish the menu accelerators so the output views can leave those
    // sequences to this window when they are docked inside it
    {
        // addMenuAction() parents the actions to this window rather than to the
        // menu they are shown in, so this is where they are found
        QList<QKeySequence> menukeys;
        for (const auto *action : findChildren<QAction *>())
            if (!action->shortcut().isEmpty()) menukeys << action->shortcut();
        setMainWindowShortcuts(menukeys);

        // The combined layout swaps File and Edit out of the menu bar while a
        // panel has the focus, and an action in a menu that is attached nowhere
        // has no shortcut context left -- Ctrl+Q, Ctrl+N, Ctrl+O and Ctrl+S
        // would all stop working there.  Associating them with the window keeps
        // their accelerators alive whichever menu is currently shown.
        for (auto *menu : {filemenu, editmenu})
            if (menu)
                for (auto *action : menu->actions())
                    addAction(action);
    }

    // Status bar
    createStatusBar();

    // document settings
    auto *document = textEdit->document();
    document->setPlainText(citeme);
    document->setModified(false);
    // load the command spec table before the first highlight so command
    // category and argument role colors are correct from the first paint;
    // the introspected name lists are added later by populateSyntax()
    syntax.loadCommandSpecs(Cfg::SYNTAX_SPEC_TABLE);
    // the dump image color names are independent of the LAMMPS instance
    syntax.setStyles(StyleCat::Color, lammpsImageColors());
    highlighter = new Highlighter(&syntax, document);
    connect(document, &QTextDocument::modificationChanged, this, &LammpsGui::modified);
    // track the cursor so unknown-name marking spares the word being typed
    connect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        const auto cursor = textEdit->textCursor();
        highlighter->setCursorPos(cursor.blockNumber(), cursor.positionInBlock());
    });

    // apply font settings
    setFont(allFont);
    textEdit->setFont(monoFont);
    document->setDefaultFont(monoFont);
    setCentralWidget(textEdit);

    // set width and height of main window
    // use default so the background logo is fully shown
    // use last values unless overridden from command-line
    // do not accept a geometry smaller than minimum, revert to default instead
    // the two layouts want very different window sizes -- the combined window
    // has to fit the editor, a dock group beside it and one below -- so each
    // remembers its own
    const bool docked = dockedLayout();
    int defaultx      = docked ? Cfg::DOCK_MAIN_DEFAULT_WIDTH : Cfg::MAIN_DEFAULT_WIDTH;
    int defaulty      = docked ? Cfg::DOCK_MAIN_DEFAULT_HEIGHT : Cfg::MAIN_DEFAULT_HEIGHT;
    // the combined default is deliberately large; do not open off-screen with it
    if (const auto *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        defaultx          = qMin(defaultx, avail.width());
        defaulty          = qMin(defaulty, avail.height());
    }
    if (mainx < Cfg::MINIMUM_WIDTH)
        mainx = settings.value(docked ? Keys::DOCKMAINX : Keys::MAINX, defaultx).toInt();
    if (mainy < Cfg::MINIMUM_HEIGHT)
        mainy = settings.value(docked ? Keys::DOCKMAINY : Keys::MAINY, defaulty).toInt();
    resize(mainx, mainy);

    // the docked layout sizes its dock areas relative to the main window, so
    // this has to come after the resize() above and before the first view
    viewlayout = new WindowLayout(this, docked ? LayoutMode::Docked : LayoutMode::Windows);

    // combined layout: the leading menus follow the focused panel, and also the
    // panel the user just asked to see (a click on a tab moves neither focus)
    if (docked) {
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
            updateMenuBarForFocus(now);
        });
        connect(viewlayout, &WindowLayout::viewActivated, this, &LammpsGui::updateMenuBarForFocus);
    }

    createVariableWindow();
}

template <typename Func>
QAction *LammpsGui::addMenuAction(QMenu *menu, const QString &iconpath, const QString &text,
                                  const QString &shortcut, Func slot)
{
    auto *action = new QAction(iconpath.isEmpty() ? QIcon() : QIcon(iconpath), text, this);
    if (!shortcut.isEmpty()) action->setShortcut(QKeySequence(shortcut));
    connect(action, &QAction::triggered, this, slot);
    menu->addAction(action);
    return action;
}

void LammpsGui::createFileMenu()
{
    auto *menu = new QMenu("&File", this);
    menubar->addMenu(menu);
    filemenu = menu;
    addMenuAction(menu, ":/icons/document-new.svg", "&New Input File", "Ctrl+N",
                  &LammpsGui::newDocument);
    addMenuAction(menu, ":/icons/document-open.svg", "&Open Input File", "Ctrl+O",
                  &LammpsGui::open);
    addMenuAction(menu, ":/icons/document-save.svg", "&Save Input File", "Ctrl+S",
                  &LammpsGui::save);
    addMenuAction(menu, ":/icons/document-save-as.svg", "Save Input File &As", "Ctrl+Shift+S",
                  &LammpsGui::saveAs);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/txt-file-icon.svg", "&View Text File", "Ctrl+Shift+F",
                  &LammpsGui::view);
    addMenuAction(menu, ":/icons/image-x-generic.svg", "View &Image or Movie File(s)...",
                  "Ctrl+Shift+J", &LammpsGui::openImages);
    addMenuAction(menu, ":/icons/x-office-drawing.svg", "&Plot Data File...", "Ctrl+Shift+P",
                  &LammpsGui::plotDataFile);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/binary-file-icon.svg", "Inspect &Restart File", "Ctrl+Shift+R",
                  &LammpsGui::inspect);
    addMenuAction(menu, ":/icons/document-save-as.svg", "&Write Restart File...", "",
                  &LammpsGui::writeRestart);
    menu->addSeparator();

    recentActions.resize(Cfg::NUM_RECENT_FILES);
    for (int i = 0; i < Cfg::NUM_RECENT_FILES; ++i) {
        recentActions[i] = addMenuAction(menu, ":/icons/document-open-recent.svg",
                                         QString("&%1.").arg(i + 1), "", &LammpsGui::openRecent);
    }
    menu->addSeparator();

    // Name the roles rather than leaving macOS to guess them from the label:
    // it moves these into the application menu by matching the text, which is
    // fragile and does nothing on the other platforms either way.
    addMenuAction(menu, ":/icons/application-exit.svg", "&Quit", "Ctrl+Q", &LammpsGui::quit)
        ->setMenuRole(QAction::QuitRole);
}

void LammpsGui::createEditMenu()
{
    auto *menu = new QMenu("&Edit", this);
    menubar->addMenu(menu);
    editmenu = menu;
    addMenuAction(menu, ":/icons/edit-undo.svg", "&Undo", "Ctrl+Z", &LammpsGui::undo);
    addMenuAction(menu, ":/icons/edit-redo.svg", "&Redo", "Ctrl+Shift+Z", &LammpsGui::redo);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/edit-copy.svg", "&Copy", "Ctrl+C", &LammpsGui::copy)
        ->setEnabled(hasClipboard);
    addMenuAction(menu, ":/icons/edit-cut.svg", "Cu&t", "Ctrl+X", &LammpsGui::cut)
        ->setEnabled(hasClipboard);
    addMenuAction(menu, ":/icons/edit-paste.svg", "&Paste", "Ctrl+V", &LammpsGui::paste)
        ->setEnabled(hasClipboard);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/search.svg", "&Find and Replace...", "Ctrl+F",
                  &LammpsGui::findAndReplace);
    menu->addSeparator();
}

// Combined layout: one menu bar for the whole window, whose leading menus
// belong to whichever view has the focus.  The application-wide half never
// changes, so only the front of the bar is rebuilt.  A view publishes its own
// menu by object name rather than through a common base class, which is how the
// dialogs in this code base are wired as well.
void LammpsGui::updateMenuBarForFocus(QWidget *focused)
{
    if (!menubar || !viewlayout || viewlayout->mode() != LayoutMode::Docked) return;

    // Walk up from the focused widget to the view that owns it.  The search has
    // to stop below this window: searching it would find every view's menu at
    // once, since the panels are its descendants.  The search within a view is
    // recursive, because a view may hang its menu off its own menu bar rather
    // than off itself.
    QMenu *viewfile = nullptr;
    for (auto *w = focused; w && w != this; w = w->parentWidget()) {
        if (auto *found = w->findChild<QMenu *>(Cfg::VIEW_FILE_MENU)) {
            viewfile = found;
            break;
        }
    }
    if (viewfile == currentviewmenu) return; // nothing moved between views

    currentviewmenu = viewfile;
    menubar->clear(); // removes the actions, the menus are owned elsewhere
    if (viewfile) {
        menubar->addMenu(viewfile);
    } else {
        menubar->addMenu(filemenu);
        menubar->addMenu(editmenu);
    }
    for (auto *shared : sharedMenus())
        menubar->addMenu(shared);
}

QList<QMenu *> LammpsGui::sharedMenus() const
{
    QList<QMenu *> shared;
    for (auto *menu : {runmenu, viewmenu, tutorialmenu, aboutmenu})
        if (menu) shared << menu;
    return shared;
}

void LammpsGui::createRunMenu()
{
    auto *menu = new QMenu("&Run", this);
    menubar->addMenu(menu);
    runmenu = menu;
    addMenuAction(menu, ":/icons/system-run.svg", "&Run LAMMPS from Editor Buffer", "Ctrl+Return",
                  &LammpsGui::runBuffer);
    addMenuAction(menu, ":/icons/run-file.svg", "Run LAMMPS from &File", "Ctrl+Shift+Return",
                  &LammpsGui::runFile);
    addMenuAction(menu, ":/icons/process-stop.svg", "&Stop LAMMPS", "Ctrl+/", &LammpsGui::stopRun);
    addMenuAction(menu, ":/icons/extend-run.svg", "&Extend Run...", "Ctrl+E",
                  &LammpsGui::extendRun);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/system-restart.svg", "Relaunch &LAMMPS Instance", "",
                  &LammpsGui::restartLammps);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/document-check.svg", "Chec&k Input via Heuristics", "Ctrl+K",
                  &LammpsGui::checkInput);
    addMenuAction(menu, ":/icons/system-dryrun.svg", "Check Input via &Dry Run", "Ctrl+Shift+K",
                  &LammpsGui::dryRunBuffer);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/preferences-desktop.svg", "Set &Variables...", "Ctrl+Shift+V",
                  &LammpsGui::editVariables);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/image-viewer.svg", "Create &Image", "Ctrl+I",
                  &LammpsGui::renderImage);
    menu->addSeparator();
    addMenuAction(menu, ":/icons/utilities-terminal.svg", "Open Comman&d Window", "Ctrl+Shift+X",
                  &LammpsGui::openCommandWindow);
    menu->addSeparator();

    auto *ovito = addMenuAction(menu, ":/icons/ovito.png", "View in &OVITO", "Ctrl+Shift+O",
                                &LammpsGui::startExe);
    ovito->setEnabled(hasExe("ovito"));
    ovito->setData("ovito");

    auto *vmd = addMenuAction(menu, ":/icons/vmd.svg", "View in VM&D", "Ctrl+Shift+D",
                              &LammpsGui::startExe);
    vmd->setEnabled(hasExe("vmd"));
    vmd->setData("vmd");
}

void LammpsGui::createViewMenu()
{
    auto *menu = new QMenu("&View", this);
    menubar->addMenu(menu);
    viewmenu = menu;
    addMenuAction(menu, ":/icons/utilities-terminal.svg", "&Output Window", "Ctrl+Shift+L",
                  &LammpsGui::viewLog);
    addMenuAction(menu, ":/icons/x-office-drawing.svg", "&Charts Window", "Ctrl+Shift+C",
                  &LammpsGui::viewChart);
    addMenuAction(menu, ":/icons/image-viewer.svg", "&Image Window", "Ctrl+Shift+I",
                  &LammpsGui::viewImage);
    addMenuAction(menu, ":/icons/image-x-generic.svg", "&Slide Show Window", "Ctrl+L",
                  &LammpsGui::viewSlides);
    addMenuAction(menu, ":/icons/utilities-terminal.svg", "&Variables Window", "Ctrl+Shift+W",
                  &LammpsGui::viewVariables);
    // no shortcut of its own: Ctrl+Shift+X belongs to Run > Open Command Window
    addMenuAction(menu, ":/icons/utilities-terminal.svg", "Co&mmand Window", "",
                  &LammpsGui::viewCommand);

    // Walking the panels is only meaningful while they are panels: with
    // individual windows this is the window manager's job and its key does it.
    // The layout is read rather than asked of viewlayout, which does not exist
    // yet when the menus are built.
    if (dockedLayout()) {
        menu->addSeparator();
        addMenuAction(menu, ":/icons/go-next-2.svg", "&Next Panel", "F6", [this]() {
            if (viewlayout) viewlayout->focusNextPane(true);
        });
        addMenuAction(menu, ":/icons/go-previous-2.svg", "&Previous Panel", "Shift+F6", [this]() {
            if (viewlayout) viewlayout->focusNextPane(false);
        });
    }
    menu->addSeparator();
    // this menu decides how the windows are arranged, and the layout style is
    // itself a preference, so the settings live here rather than in Edit, which
    // belongs to the editor alone and is not shown while a panel has the focus
    addMenuAction(menu, ":/icons/preferences-desktop.svg", "P&references...", "Ctrl+P",
                  &LammpsGui::preferences)
        ->setMenuRole(QAction::PreferencesRole);
    addMenuAction(menu, ":/icons/preferences-reset.svg", "Reset Preferences to &Defaults", "",
                  &LammpsGui::defaults);
}

void LammpsGui::createTutorialMenu()
{
    auto *menu = new QMenu("&Tutorials", this);
    menubar->addMenu(menu);
    tutorialmenu            = menu;
    const auto &collections = tutorialCollections();
    for (int c = 0; c < collections.size(); ++c) {
        const auto &coll = collections[c];
        QString title    = QString("&%1: %2").arg(c + 1).arg(coll.name);
        if (!coll.published) title += " (" + coll.status + ")";
        auto *sub = menu->addMenu(QIcon(":/icons/tutorial-logo.png"), title);
        // a planned collection has no tutorials yet: show it as a disabled entry
        if (coll.count() == 0) {
            sub->menuAction()->setEnabled(false);
            continue;
        }
        for (int i = 0; i < coll.count(); ++i) {
            QAction *action;
            int ip1 = i + 1;
            int dec = ip1 / 10;
            if (i < 9) {
                action =
                    addMenuAction(sub, coll.logoFor(ip1),
                                  QString("Tutorial  &%1: %2").arg(ip1).arg(coll.titles.value(i)),
                                  "", [this, c, ip1]() {
                                      startTutorial(c, ip1);
                                  });
            } else {
                action = addMenuAction(sub, coll.logoFor(ip1),
                                       QString("Tutorial %1&%2: %3")
                                           .arg(dec)
                                           .arg(ip1 - dec * 10)
                                           .arg(coll.titles.value(i)),
                                       "", [this, c, ip1]() {
                                           startTutorial(c, ip1);
                                       });
            }

            // Tutorials beyond the available count appear as a "coming attractions"
            // teaser: their titles are visible but the entries cannot be launched yet.
            if (i >= coll.available) action->setEnabled(false);
        }
    }
}

void LammpsGui::createAboutMenu()
{
    auto *menu = new QMenu("&About", this);
    menubar->addMenu(menu);
    aboutmenu = menu;
    addMenuAction(menu, ":/icons/lammps-gui-icon-128x128.png", "&About LAMMPS-GUI", "Ctrl+Shift+A",
                  &LammpsGui::about)
        ->setMenuRole(QAction::AboutRole);
    addMenuAction(menu, ":/icons/help-faq.svg", "Quick &Help", "Ctrl+Shift+H", &LammpsGui::help);
    addMenuAction(menu, ":/icons/system-help.svg", "LAMMPS-&GUI Documentation", "Ctrl+Shift+G",
                  &LammpsGui::howto);
    addMenuAction(menu, ":/icons/help-browser.svg", "LAMMPS Online &Manual", "Ctrl+Shift+M",
                  &LammpsGui::manual);
    addMenuAction(menu, ":/icons/tutorial-logo.png", "LAMMPS &Tutorial Website", "Ctrl+Shift+T",
                  &LammpsGui::tutorialWeb);

#if defined(LAMMPS_GUI_USE_PLUGIN)
    menu->addSeparator();
    addMenuAction(menu, ":/icons/lammps-plugin.png", "Check for &LAMMPS update", "Ctrl+Shift+U",
                  &LammpsGui::checkUpdate);
#endif
}

void LammpsGui::createStatusBar()
{
    statusbar = new QStatusBar(this);
    setStatusBar(statusbar);

    lammpsstatus = new QLabel(QString());
    auto pix     = QPixmap(Cfg::LAMMPS_ICON);
    lammpsstatus->setPixmap(pix.scaled(Cfg::ICON_SCALE, Cfg::ICON_SCALE, Qt::KeepAspectRatio));
    lammpsstatus->setToolTip("LAMMPS instance is active");
    lammpsstatus->hide();
    statusbar->addWidget(lammpsstatus);

    auto *savebtn = new QPushButton(QIcon(":/icons/document-save.svg"), "");
    savebtn->setToolTip("Save edit buffer to file");
    connect(savebtn, &QPushButton::released, this, &LammpsGui::save);
    statusbar->addWidget(savebtn);

    auto *runbtn = new QPushButton(QIcon(":/icons/system-run.svg"), "");
    runbtn->setToolTip("Run LAMMPS on input");
    connect(runbtn, &QPushButton::released, this, &LammpsGui::runBuffer);
    statusbar->addWidget(runbtn);

    auto *stopbtn = new QPushButton(QIcon(":/icons/process-stop.svg"), "");
    stopbtn->setToolTip("Stop LAMMPS");
    connect(stopbtn, &QPushButton::released, this, &LammpsGui::stopRun);
    statusbar->addWidget(stopbtn);

    auto *imgbtn = new QPushButton(QIcon(":/icons/image-viewer.svg"), "");
    imgbtn->setToolTip("Create snapshot image");
    connect(imgbtn, &QPushButton::released, this, &LammpsGui::renderImage);
    statusbar->addWidget(imgbtn);

    // square status-bar buttons with a snug, uniform icon (shared policy)
    styleToolButtons(toolButtonSize(savebtn), {savebtn, runbtn, stopbtn, imgbtn});

    cpuuse = new QLabel(Cfg::STATUS_ZERO_CPU);
    cpuuse->setFixedWidth(90);
    statusbar->addWidget(cpuuse);
    cpuuse->hide();

    status = new QLabel(Cfg::STATUS_READY);
    // The status bar sets the floor under the whole window: two 400-wide
    // minimums plus labels that report their full text width leave it unable to
    // shrink past ~1100.  With individual windows that is a fair price for a
    // readable directory line, but the combined window has to be free to be made
    // small, so there the labels are allowed to be clipped instead.
    const bool docked = dockedLayout();
    status->setFixedWidth(300);
    statusbar->addWidget(status);

    // QSizePolicy::Ignored drops the preferred width along with the minimum, and
    // QStatusBar lays its non-permanent widgets out with a trailing stretch item
    // that then claims all the free space: without a stretch factor of their own
    // the two widgets below end up shown but zero pixels wide.  The individual
    // windows keep their fixed 400 and need no stretch.
    const int stretch = docked ? 1 : 0;

    dirstatus = new QLabel(QString(" Directory: (unknown)"));
    if (docked)
        dirstatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    else
        dirstatus->setMinimumWidth(Cfg::MINIMUM_WIDTH);
    dirstatus->show();
    statusbar->addWidget(dirstatus, stretch);

    progress = new QProgressBar();
    progress->setRange(0, Cfg::PROGRESS_MAXIMUM);
    if (docked)
        progress->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    else
        progress->setMinimumWidth(Cfg::MINIMUM_WIDTH);
    progress->hide();
    statusbar->addWidget(progress, stretch);
}

#if defined(LAMMPS_GUI_USE_PLUGIN)
void LammpsGui::setupPlugin(QSettings &settings)
{
    // first try to load from existing setting
    pluginPath = settings.value(Keys::PLUGIN_PATH, "").toString();
    if (!pluginPath.isEmpty()) {
        // make canonical and try loading; reset to empty string if loading failed
        pluginPath = QFileInfo(pluginPath).canonicalFilePath();
        if (!lammps.loadLib(pluginPath)) {
            pluginPath.clear();
            // could not load successfully -> remove any existing setting.
            settings.remove(Keys::PLUGIN_PATH);
        }
    }

    // set platform specific paths, library file name, config directory, and filename patterns
    QStringList dirlist{"."};
    const auto libName = getLammpsLibName();
#if defined(Q_OS_MACOS)
    const QString pattern = QStringLiteral("LAMMPS shared library (liblammps*.dylib)");
    QStringList filter("liblammps*.dylib");
    dirlist.append(
        QString::fromLocal8Bit(qgetenv("DYLD_LIBRARY_PATH")).split(":", Qt::SkipEmptyParts));
    // library may be included in an application bundle:
    dirlist.append({"/Applications/LAMMPS-GUI.app/Contents/Frameworks",
                    "/Applications/LAMMPS.app/Contents/Frameworks"});
#elif defined(Q_OS_WIN32)
    const QString pattern = QStringLiteral("LAMMPS shared library (liblammps*.dll)");
    QStringList filter("liblammps*.dll");
    dirlist.append(QString::fromLocal8Bit(qgetenv("PATH")).split(";", Qt::SkipEmptyParts));
#else
    // for Linux and other unix-like systems
    const QString pattern = QStringLiteral("LAMMPS shared library (liblammps*.so*)");
    QStringList filter("liblammps*.so*");
    dirlist.append(
        QString::fromLocal8Bit(qgetenv("LD_LIBRARY_PATH")).split(":", Qt::SkipEmptyParts));
#endif

    if (pluginPath.isEmpty()) {
        // construct list of possible standard choices for the shared library file
        // we prefer the current directory, then the dynamic library path, then some system folders
        // adapt file pattern and paths to the different operating systems

        // also check in the config dir location for a previously downloaded library
        dirlist.append(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        // check some more system paths (only relevant for Linux and Unix-like
        // systems; they simply do not exist elsewhere)
        dirlist.append({"/usr/lib", "/usr/lib64", "/lib/x86_64-linux-gnu", "/usr/local/lib",
                        "/usr/local/lib64"});

        // construct list of matching files
        QFileInfoList entries;
        for (const auto &dir : dirlist)
            entries.append(QDir(dir).entryInfoList(filter));

        // convert list of paths to list of canonical file names
        QStringList choices;
        for (const auto &fn : entries)
            choices.append(fn.canonicalFilePath());
        choices.removeDuplicates();
        for (const auto &libpath : choices) {
            if (lammps.loadLib(libpath)) {
                pluginPath = libpath;
                settings.setValue(Keys::PLUGIN_PATH, pluginPath);
                settings.sync();
                break;
            }
        }

        // No suitable plugin was found automatically.  Show a dialog with three choices:
        // 1) Download a pre-compiled shared library from the LAMMPS webserver
        //    (not offered when no compatible pre-compiled library exists, i.e. with MSVC)
        // 2) Browse the filesystem for a suitable shared library file
        // 3) Exit LAMMPS-GUI
        const bool candownload = !getLammpsDownloadUrl().isEmpty();
        while (pluginPath.isEmpty()) {
            // remove key for path to the plugin so we won't get stuck in a loop reading a bad file
            settings.remove(Keys::PLUGIN_PATH);

            QMessageBox mb(this);
            mb.setWindowTitle("LAMMPS-GUI - No LAMMPS Shared Library");
            mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
            mb.setIconPixmap(QPixmap(":/icons/lammps-plugin.png").scaled(96, 96));
            mb.setText("No suitable LAMMPS shared library found.");
            QString infotext =
                "<p align=\"justify\">Either the shared library path has been reset, the "
                "configured or default library file was not found, or the selected library failed "
                "to load.</p><p align=\"justify\">You may now either ";
            if (candownload)
                infotext += "download a pre-compiled LAMMPS shared library file for your platform "
                            "from the LAMMPS webserver, browse the ";
            else
                infotext += "browse the ";
            infotext += "filesystem for a suitable LAMMPS library file, or exit LAMMPS-GUI.</p>";
            mb.setInformativeText(infotext);

            QPushButton *downloadBtn = nullptr;
            if (candownload) {
                downloadBtn = mb.addButton("Download Library...", QMessageBox::ApplyRole);
                downloadBtn->setIcon(QIcon(":/icons/download-file.svg"));
            }
            auto *browseBtn = mb.addButton("Browse Filesystem...", QMessageBox::AcceptRole);
            browseBtn->setIcon(QIcon(":/icons/document-open.svg"));
            auto *exitBtn = mb.addButton("Exit", QMessageBox::NoRole);
            exitBtn->setIcon(QIcon(":/icons/application-exit.svg"));

            mb.setDefaultButton(candownload ? downloadBtn : browseBtn);
            mb.setEscapeButton(exitBtn);
            mb.exec();

            if (mb.clickedButton() == exitBtn) {
                // we cannot use QApplication::exit() here since we are still in the constructor
                exit(1);

            } else if (mb.clickedButton() == browseBtn) {
                QString pluginfile = QFileDialog::getOpenFileName(
                    this, "Select LAMMPS shared library to use", ".", pattern, nullptr,
                    QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly);
                if (!pluginfile.isEmpty() && pluginfile.contains("liblammps", Qt::CaseSensitive)) {
                    auto canonical = QFileInfo(pluginfile).canonicalFilePath();
                    settings.setValue(Keys::PLUGIN_PATH, canonical);
                    settings.sync();
                    // must re-launch LAMMPS-GUI to cleanly load the selected new plugin
                    relaunchApplication();
                    // This should not happen...
                    critical(this, "LAMMPS-GUI Error", "Relaunching LAMMPS-GUI failed.",
                             "LAMMPS-GUI must be restarted to correctly load the selected "
                             "LAMMPS shared library. Click on 'Close' to exit.");
                    exit(1);
                }
                // user cancelled file dialog -> loop back to show the dialog again

            } else if (mb.clickedButton() == downloadBtn) {
                // store in the same config directory where QSettings stores preferences
                const auto configDir =
                    QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
                if (configDir.isEmpty() || !QDir().mkpath(configDir)) {
                    critical(this, "LAMMPS-GUI Error", "Cannot determine configuration directory.",
                             "Unable to create a writable directory in the user configuration "
                             "folder for storing the downloaded LAMMPS shared library.");
                    continue;
                }
                auto libPath = configDir + QDir::separator() + libName;
                auto dlUrl   = getLammpsDownloadUrl();

                URLDownloader downloader(this);
                if (downloader.download(dlUrl, libPath, true, true)) {
                    // try loading the downloaded library
                    if (lammps.loadLib(libPath)) {
                        pluginPath = libPath;
                        settings.setValue(Keys::PLUGIN_PATH, pluginPath);
                        settings.sync();
                        // must re-launch LAMMPS-GUI to cleanly load the selected new plugin
                        relaunchApplication();
                        // This should not happen...
                        critical(this, "LAMMPS-GUI Error", "Relaunching LAMMPS-GUI failed.",
                                 "LAMMPS-GUI must be restarted to correctly load the selected "
                                 "LAMMPS shared library. Click on 'Close' to exit.");
                        exit(1);
                    } else {
                        QFile::remove(libPath);
                        critical(this, "LAMMPS-GUI Error",
                                 "Downloaded LAMMPS library could not be loaded.",
                                 "<p align=\"justify\">The downloaded shared library file "
                                 "does not seem to be compatible with this system.</p>");
                    }
                } else {
                    critical(this, "LAMMPS-GUI Error", "Failed to download LAMMPS shared library.",
                             downloader.errorString());
                }
            }
        }
    }
}
#else
// dummy function when linking against library directly
void LammpsGui::setupPlugin(QSettings &) {}
#endif

void LammpsGui::setupAccelerators(QSettings &settings)
{
    // default accelerator package is OPENMP, but we switch the configured accelerator to
    // "none" if the selected package is not available to have an option that always works
    int accel = settings.value(Keys::ACCELERATOR, AcceleratorTab::OpenMP).toInt();
    switch (accel) {
        case AcceleratorTab::Opt:
            if (!lammps.configHasPackage("OPT")) accel = AcceleratorTab::None;
            break;
        case AcceleratorTab::OpenMP:
            if (!lammps.configHasPackage("OPENMP")) accel = AcceleratorTab::None;
            break;
        case AcceleratorTab::Intel:
            if (!lammps.configHasPackage("INTEL")) accel = AcceleratorTab::None;
            break;
        case AcceleratorTab::Gpu:
            if (!lammps.configHasPackage("GPU")) accel = AcceleratorTab::None;
            break;
        case AcceleratorTab::Kokkos:
            if (!lammps.configHasPackage("KOKKOS")) accel = AcceleratorTab::None;
            break;
        case AcceleratorTab::None: // fallthrough
        default:                   // do nothing
            break;
    }
    settings.setValue(Keys::ACCELERATOR, accel);

    // Check and initialize some settings for individual accelerator packages and commit
    // GPU neighbor list on GPU versus host
    bool gpuneigh = settings.value(Keys::GPUNEIGH, true).toBool();
    settings.setValue(Keys::GPUNEIGH, gpuneigh);
    // accelerate only pair style (i.e. run PPPM completely on host)
    bool gpupaironly = settings.value(Keys::GPUPAIRONLY, false).toBool();
    settings.setValue(Keys::GPUPAIRONLY, gpupaironly);
    // INTEL package precision
    int intelprec = settings.value(Keys::INTELPREC, AcceleratorTab::Mixed).toInt();
    settings.setValue(Keys::INTELPREC, intelprec);

    // Check and initialize nthreads setting for when OpenMP support is compiled in.
    // Default is to use OMP_NUM_THREADS setting, if that is not available, then half of max
    // (assuming hyper-threading is enabled) and no more than Cfg::MAX_DEFAULT_THREADS
    // (=16). This is only if there is no preference set but do not override OMP_NUM_THREADS
    int default_threads = std::min(QThread::idealThreadCount() / 2, Cfg::MAX_DEFAULT_THREADS);
    default_threads     = std::max(default_threads, 1);
    if (qEnvironmentVariableIsSet("OMP_NUM_THREADS"))
        default_threads = qEnvironmentVariable("OMP_NUM_THREADS").toInt();
    nthreads = settings.value(Keys::NTHREADS, default_threads).toInt();

    // reset nthreads if accelerator does not support threads
    if ((accel == AcceleratorTab::Opt) || (accel == AcceleratorTab::None)) nthreads = 1;

    // set OMP_NUM_THREADS environment variable, if not set
    if (!qEnvironmentVariableIsSet("OMP_NUM_THREADS"))
        qputenv("OMP_NUM_THREADS", QByteArray::number(nthreads));
}

/* -------------------------------------------------------------------- */

LammpsGui::LammpsGui(QWidget *parent, const QString &filename, int width, int height) :
    QMainWindow(parent), textEdit(nullptr), menubar(nullptr), filemenu(nullptr), editmenu(nullptr),
    currentviewmenu(nullptr), runmenu(nullptr), viewmenu(nullptr), tutorialmenu(nullptr),
    aboutmenu(nullptr), highlighter(nullptr), capturer(new StdCapture), status(nullptr),
    cpuuse(nullptr), lastCpuBucket(-1), logwindow(nullptr), imagewindow(nullptr),
    chartwindow(nullptr), slideshow(nullptr), commandwindow(nullptr), logupdater(nullptr),
    dirstatus(nullptr), progress(nullptr), prefdialog(nullptr), lammpsstatus(nullptr),
    varwindow(nullptr), wizard(nullptr), viewlayout(nullptr), runner(nullptr), runCounter(0),
    extendSteps(Cfg::EXTEND_STEPS_DEFAULT), nthreads(1), mainx(width), mainy(height)
{
#if QT_CONFIG(clipboard)
    hasClipboard = true;
#else
    hasClipboard = false;
#endif
    docver = "";

#if !defined(Q_OS_MACOS)
    // minimize window so we don't see it while it is being constructed and configured.
    // this hack does not work as expected on macOS but it is also not really needed.
    showMinimized();
#endif

    // restore and initialize settings
    QSettings settings;

    // configure fonts
    QFont allFont;
    QFontInfo allInfo(*GUI_ALLFONT);
    allFont.setFamily(settings.value(Keys::ALLFAMILY, allInfo.family()).toString());
    allFont.setPointSize(settings.value(Keys::ALLSIZE, allInfo.pointSize()).toInt());
    allFont.setStyleHint(GUI_ALLFONT->styleHint());
    settings.setValue(Keys::ALLFAMILY, allFont.family());
    settings.setValue(Keys::ALLSIZE, allFont.pointSize());

    QFont monoFont = monoFontFromSettings();
    settings.setValue(Keys::MONOFAMILY, monoFont.family());
    settings.setValue(Keys::MONOSIZE, monoFont.pointSize());

    // create and connect GUI elements
    setupUi(settings, allFont, monoFont);

    currentFile.clear();
    currentDir = QDir(".").absolutePath();
    // use $HOME if we get dropped to "/" like on macOS or the installation folder or
    // system folder like on Windows
    if ((currentDir == "/") || (currentDir.contains("AppData")) ||
        (currentDir.contains("system32")))
        currentDir = QDir::homePath();
    QDir::setCurrent(currentDir);
    dirstatus->setText(QString(" Directory: ") + currentDir);

    setAutoFillBackground(true);

    // clean up backup files and partial downloads from updating the LAMMPS
    // shared library before it is loaded below; a file that was still locked
    // when it was replaced or reset is deletable again after a restart
    purgeLibraryLeftovers();

    setupPlugin(settings);
    setupAccelerators(settings);

    // set up default LAMMPS thread arguments
    lammpsArgs.clear();
    lammpsArgs.push_back("LAMMPS-GUI");
    lammpsArgs.push_back("-log");
    lammpsArgs.push_back("none");

    installEventFilter(this);

    settings.sync();

    updateRecents();

    if ((filename.size() > 0) && !filename.endsWith("lammps-gui.exe")) {
        openFile(filename);
    } else {
        updateEditorTitle(QString());
    }

    // start LAMMPS, fill the syntax registry from introspection, and feed the
    // completers from the registry (single source of the valid name lists)
    startLammps();
    populateSyntax();
    textEdit->setCommandList(syntax.completionList(StyleCat::Command, false));
    textEdit->setVariableList(syntax.completionList(StyleCat::Variable, false));
    textEdit->setUnitsList(syntax.completionList(StyleCat::Units, false));
    textEdit->setExtraList(syntax.completionList(StyleCat::Extra, false));
    textEdit->setColorList(syntax.completionList(StyleCat::Color, false));
    textEdit->setImageKwList(syntax.completionList(StyleCat::ImageKw, false));
    textEdit->setFileList();
    textEdit->setFixList(syntax.completionList(StyleCat::Fix, false));
    textEdit->setComputeList(syntax.completionList(StyleCat::Compute, false));
    textEdit->setDumpList(syntax.completionList(StyleCat::Dump, false));
    textEdit->setAtomList(syntax.completionList(StyleCat::Atom, false));
    textEdit->setPairList(syntax.completionList(StyleCat::Pair, true));
    textEdit->setBondList(syntax.completionList(StyleCat::Bond, true));
    textEdit->setAngleList(syntax.completionList(StyleCat::Angle, true));
    textEdit->setDihedralList(syntax.completionList(StyleCat::Dihedral, true));
    textEdit->setImproperList(syntax.completionList(StyleCat::Improper, true));
    textEdit->setKspaceList(syntax.completionList(StyleCat::Kspace, true));
    textEdit->setRegionList(syntax.completionList(StyleCat::Region, false));
    textEdit->setIntegrateList(syntax.completionList(StyleCat::Integrate, false));
    textEdit->setMinimizeList(syntax.completionList(StyleCat::Minimize, false));

    settings.beginGroup(Keys::GROUP_REFORMAT);
    textEdit->setReformatOnReturn(settings.value(Keys::RETURN, false).toBool());
    textEdit->setAutoComplete(settings.value(Keys::AUTOMATIC, true).toBool());
    settings.endGroup();

    // apply https proxy setting: prefer environment variable or fall back to preferences value
    applyProxySetting(lammps, settings);

    // finally show the window
    // only the combined window opens maximized; see the preferences dialog
    if (dockedLayout() && settings.value(Keys::MAXIMIZED, false).toBool())
        showMaximized();
    else
        showNormal();
}

LammpsGui::~LammpsGui()
{
    // remember the dock arrangement while the docks are still around
    viewlayout->saveState();

    delete highlighter;
    delete capturer;
    delete status;
    delete cpuuse;
    delete logwindow;
    delete imagewindow;
    delete chartwindow;
    delete dirstatus;
    delete varwindow;
    delete slideshow;
    delete commandwindow;
}

void LammpsGui::newDocument()
{
    if (textEdit->document()->isModified()) {
        int rv = showUnsavedChangesDialog(
            this, currentFile, "Do you want to save the current file before starting a new input?");
        switch (rv) {
            case QMessageBox::Yes:
                save();
                break;
            case QMessageBox::Cancel:
                return;
            case QMessageBox::No: // fallthrough
            default:
                // do nothing
                break;
        }
    }
    currentFile.clear();
    textEdit->document()->setPlainText(citeme);
    textEdit->document()->setModified(false);
    textEdit->setStyleSheet(bannerstyle);

    if (lammps.isRunning()) {
        stopRun();
        runner->wait();
        runner->deleteLater();
        runner = nullptr;
    }
    // close windows
    delete chartwindow;
    delete logwindow;
    delete slideshow;
    delete imagewindow;
    delete varwindow;
    chartwindow = nullptr;
    logwindow   = nullptr;
    slideshow   = nullptr;
    imagewindow = nullptr;
    varwindow   = nullptr;

    {
        StdoutSilencer guard;
        lammps.close();
    }
    lammpsstatus->hide();
    updateEditorTitle(QString());
    runCounter = 0;
}

void LammpsGui::open()
{
    QString fileName =
        QFileDialog::getOpenFileName(this, "Open the file", QDir::currentPath(), Cfg::FILTER_INPUT);
    openFile(fileName);
}

void LammpsGui::view()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open the file", QDir::currentPath());
    viewFile(fileName);
}

void LammpsGui::inspect()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open the restart file",
                                                    QDir::currentPath(), Cfg::FILTER_RESTART);
    inspectFile(fileName);
}

bool LammpsGui::hasSystemState()
{
    return lammps.isOpen() && !lammps.isRunning() && (lammps.extractSetting("box_exist") != 0);
}

void LammpsGui::writeRestart()
{
    // LAMMPS is not re-entrant, so we can only issue commands when it is not running
    if (lammps.isRunning()) {
        warning(this, "LAMMPS-GUI Warning",
                "Must stop the current run before writing a restart file");
        return;
    }
    if (!hasSystemState()) {
        warning(this, "LAMMPS-GUI Warning",
                "Cannot write a restart file without a system state.\n"
                "Must run the input at least to the point where the system is defined.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, "Write Restart File",
        QDir::current().absoluteFilePath(defaultFileStem(currentFile) + ".restart"),
        Cfg::FILTER_RESTART);
    if (fileName.isEmpty()) return;
    fileName = ensureFileSuffix(fileName, "restart");

    {
        StdoutSilencer guard;
        lammps.command(QString("write_restart '%1'").arg(fileName));
    }

    const QString errmsg = lammps.lastErrorMessage();
    if (!errmsg.isEmpty()) {
        critical(this, "LAMMPS-GUI Error", "<p>Error writing restart file:</p>",
                 QString("<p><pre>%1</pre></p>").arg(errmsg));
    } else {
        status->setText(QString("Wrote restart file %1").arg(fileName));
    }
}

void LammpsGui::openRecent()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (act) openFile(act->data().toString());
}

void LammpsGui::getDirectory()
{
    if (wizard) {
        auto *line = wizard->findChild<QLineEdit *>("t_directory");
        if (line) {
            auto curdir = line->text();
            QFileDialog dialog(this, "Choose Directory for Tutorial Files", curdir);
            dialog.setFileMode(QFileDialog::Directory);
            dialog.setOption(QFileDialog::ShowDirsOnly, false);
            dialog.exec();
            line->setText(dialog.directory().path());
        }
    }
}

void LammpsGui::startExe()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (act) {
        auto exe = act->data().toString();
        QStringList args;
        if (lammps.extractSetting("box_exist")) {
            QString datacmd = "write_data '";
            QDir datadir(QDir::tempPath());
            QFile datafile(datadir.absoluteFilePath(currentFile + ".data"));
            datacmd += datafile.fileName() + "'";
            if (exe == "vmd") {
                QFile vmdfile(datadir.absoluteFilePath("tmp-loader.vmd"));
                if (vmdfile.open(QIODevice::WriteOnly)) {
                    vmdfile.write("package require topotools\n");
                    vmdfile.write("topo readlammpsdata {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write("}\ntopo guessatom lammps data\n");
                    vmdfile.write("animate write psf {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".psf}\nanimate write dcd {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".dcd}\nmol delete top\nmol new {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".psf} type psf waitfor all\nmol addfile {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".dcd} type dcd waitfor all\nfile delete {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write("} {");
                    vmdfile.write(vmdfile.fileName().toLocal8Bit());
                    vmdfile.write("} {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".dcd} {");
                    vmdfile.write(datafile.fileName().toLocal8Bit());
                    vmdfile.write(".psf}\n");
                    vmdfile.close();
                    args << "-e" << vmdfile.fileName();
                    {
                        StdoutSilencer guard;
                        lammps.command(datacmd);
                    }
                    auto *vmd = new QProcess(this);
                    vmd->start(findExe(exe), args);
                } else {
                    warning(this, "LAMMPS-GUI Error",
                            "Cannot create temporary file for loading system in VMD",
                            vmdfile.errorString());
                }
            }
            if (exe == "ovito") {
                args << datafile.fileName();
                {
                    StdoutSilencer guard;
                    lammps.command(datacmd);
                }
                auto *ovito = new QProcess(this);
                ovito->start(findExe(exe), args);
            }
        } else {
            // launch program without arguments when no system exists (yet)
            auto *proc = new QProcess(this);
            proc->start(findExe(exe), args);
        }
    }
}

void LammpsGui::updateRecents(const QString &filename)
{
    QSettings settings;
    if (settings.contains(Keys::RECENT))
        recent = settings.value(Keys::RECENT).value<QList<QString>>();

    recent.removeIf([](const QString &f) {
        return !QFileInfo(f).isReadable();
    });

    if (!filename.isEmpty() && !recent.contains(filename)) recent.prepend(filename);
    if (recent.size() > Cfg::NUM_RECENT_FILES) recent.removeLast();
    if (!recent.empty())
        settings.setValue(Keys::RECENT, QVariant::fromValue(recent));
    else
        settings.remove(Keys::RECENT);

    for (int i = 0; i < Cfg::NUM_RECENT_FILES; ++i) {
        recentActions[i]->setVisible(false);
        if (i < recent.size() && !recent[i].isEmpty()) {
            QFileInfo fi(recent[i]);
            recentActions[i]->setText(QString("&%1. ").arg(i + 1) + fi.fileName());
            recentActions[i]->setData(recent[i]);
            recentActions[i]->setVisible(true);
        }
    }
}

// delete all current variables in the LAMMPS instance
void LammpsGui::clearVariables()
{
    int nvar = lammps.idCount("variable");

    // delete from back so they are not re-indexed
    for (int i = nvar - 1; i >= 0; --i) {
        const QString name = lammps.idName("variable", i);
        if (!name.isEmpty()) lammps.command(QString("variable %1 delete").arg(name));
    }
}

void LammpsGui::updateVariables()
{
    // fresh parse: any dialog overrides for the previous buffer are dropped
    variables = parseInputVariables(textEdit->toPlainText());
    textEdit->setVariableOverrides(variables);
}

void LammpsGui::refreshVariables()
{
    variables = mergeInputVariables(parseInputVariables(textEdit->toPlainText()), variables);
    textEdit->setVariableOverrides(variables);
}

// open file and switch CWD to path of file
void LammpsGui::openFile(const QString &fileName)
{
    // do nothing, if no file name provided
    if (fileName.isEmpty()) return;

    // A name that does not exist yet is a new file and perfectly fine.  One that
    // does and is not text is almost always a mistake, and asking has to happen
    // here, before the run is ended and the output windows are closed for it.
    if (QFileInfo::exists(fileName) && looksLikeBinaryFile(fileName) &&
        !confirmUnexpectedFile(this, fileName, "text"))
        return;

    if (lammps.isRunning()) {
        stopRun();
        runner->wait();
        runner->deleteLater();
        runner = nullptr;
    }
    // close windows
    delete chartwindow;
    delete logwindow;
    delete slideshow;
    delete imagewindow;
    delete varwindow;
    chartwindow = nullptr;
    logwindow   = nullptr;
    slideshow   = nullptr;
    imagewindow = nullptr;
    varwindow   = nullptr;
    {
        StdoutSilencer guard;
        lammps.close();
    }

    purgeInspectList();
    textEdit->setStyleSheet("");
    if (textEdit->document()->isModified()) {
        int rv = showUnsavedChangesDialog(
            this, currentFile, "Do you want to save the file before opening a new file?");
        switch (rv) {
            case QMessageBox::Yes:
                save();
                break;
            case QMessageBox::Cancel:
                return;
            case QMessageBox::No: // fallthrough
            default:
                // do nothing
                break;
        }
    }
    textEdit->setHighlight(CodeEditor::NO_HIGHLIGHT, false);

    QFileInfo path(fileName);
    currentFile = path.fileName();
    currentDir  = path.absolutePath();
    QFile file(path.absoluteFilePath());

    updateRecents(path.absoluteFilePath());

    QDir::setCurrent(currentDir);
    if (!file.open(QIODevice::ReadOnly | QFile::Text)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot open file " + path.absoluteFilePath() + ":",
                file.errorString() + "\n\nWill create new file on saving editor buffer.");
        textEdit->document()->clear();
        textEdit->document()->setPlainText(citeme);
        textEdit->document()->setModified(false);
        textEdit->setStyleSheet(bannerstyle);
    } else {
        QTextStream in(&file);
        QString text = in.readAll();
        textEdit->document()->clear();
        textEdit->document()->setPlainText(text);
        textEdit->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
        file.close();
    }
    updateEditorTitle(currentFile);
    runCounter = 0;
    textEdit->document()->setModified(false);
    textEdit->setGroupList();
    textEdit->setVarNameList();
    textEdit->setComputeIDList();
    textEdit->setFixIDList();
    textEdit->setFileList();
    dirstatus->setText(QString(" Directory: ") + currentDir);
    status->setText(Cfg::STATUS_READY);
    cpuuse->hide();

    updateVariables();
}

// open file in read-only mode for viewing in separate window
void LammpsGui::viewFile(const QString &fileName)
{
    // empty name means the file dialog was canceled. nothing to do here
    if (fileName.isEmpty()) return;

    // a movie file is also an image file when it is an animated GIF
    if (isMovieFile(fileName)) {
        warning(this, "Cannot View Movie as Text",
                "\"" + QFileInfo(fileName).fileName() +
                    "\" is a movie file and cannot be displayed in the text viewer.\n"
                    "Use \"View Image or Movie File(s)...\" (Ctrl+Shift+J) to open it.");
        return;
    }

    if (isImageFile(fileName)) {
        warning(this, "Cannot View Image as Text",
                "\"" + QFileInfo(fileName).fileName() +
                    "\" is an image file and cannot be displayed in the text viewer.\n"
                    "Use \"View Image or Movie File(s)...\" (Ctrl+Shift+J) to open it.");
        return;
    }

    // unlike an image or a movie, which have a viewer of their own to be sent
    // to, a binary file has nowhere else to go -- so this is the user's call
    if (looksLikeBinaryFile(fileName) && !confirmUnexpectedFile(this, fileName, "text")) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QFile::Text)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot open file " + fileName + ":",
                file.errorString());
    } else {
        file.close();
        auto *viewer = new FileViewer(fileName, this);
        // combined layout: a text viewer joins the tab group on the right
        viewlayout->addAuxiliaryView(viewer, ViewSlot::Chart, QFileInfo(fileName).fileName());
    }
}

// open one or more image or movie files in a standalone snapshot viewer
void LammpsGui::openImages()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Open Image or Movie File(s)", currentDir,
        "Image and movie files (*.png *.jpg *.jpeg *.bmp *.ppm *.pgm *.gif *.tif *.tiff *.tga "
        "*.eps *.sgi *.webp *.mp4 *.m4v *.mkv *.webm *.avi *.mov *.mpg *.mpeg *.ogv *.wmv "
        "*.flv);;Image files (*.png *.jpg *.jpeg *.bmp *.ppm *.pgm *.gif *.tif *.tiff *.tga *.eps "
        "*.sgi *.webp);;Movie files (*.mp4 *.m4v *.mkv *.webm *.avi *.mov *.mpg *.mpeg *.ogv *.wmv "
        "*.flv *.gif);;All files (*)");
    openImageFiles(files);
}

// the same without the dialog, for a list that is already in hand -- the command
// window's "open" hands one over after the shell has expanded it
void LammpsGui::openImageFiles(const QStringList &files)
{
    if (files.isEmpty()) return;

    // the file dialog offers "All files" as well, so what arrives here need not
    // be a picture at all.  One question for the action rather than one per
    // file: it names the first that does not fit and covers the whole list.
    for (const auto &f : files) {
        if (isImageFile(f) || isMovieFile(f)) continue;
        if (!confirmUnexpectedFile(this, f, "image or movie")) return;
        break;
    }

    auto *viewer = new SlideShow(files.first());
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // combined layout: the viewer joins the tab group on the right, next to the
    // slide show of the current run rather than in a window of its own
    viewlayout->addAuxiliaryView(viewer, ViewSlot::SlideShow,
                                 QString("Slides: %1").arg(QFileInfo(files.first()).fileName()));

    // the import dialog of a movie file is modal to the (already visible)
    // slide show window, so a movie must not be added before it is shown
    for (const QString &f : files) {
        if (isMovieFile(f))
            viewer->addMovie(f);
        else
            viewer->addImage(f);
    }

    // every movie import was canceled or failed and no image was selected
    if (viewer->imageCount() == 0) viewer->close();
}

void LammpsGui::purgeInspectList()
{
    // iterator loop: erase() both removes the entry (a range-for would be left
    // with invalidated iterators) and hands back the next valid position
    for (auto it = inspectList.begin(); it != inspectList.end();) {
        auto *item = *it;
        if (item->info && !item->info->isVisible()) {
            delete item->info;
            item->info = nullptr;
        }
        if (item->data && !item->data->isVisible()) {
            delete item->data;
            item->data = nullptr;
        }
        if (item->image && !item->image->isVisible()) {
            delete item->image;
            item->image = nullptr;
        }
        if (!item->image && !item->data && !item->info) {
            delete item;
            it = inspectList.erase(it);
        } else {
            ++it;
        }
    }
}

// read restart file into LAMMPS instance and launch image viewer
void LammpsGui::inspectFile(const QString &fileName)
{
    // empty name means the file dialog was canceled. nothing to do here
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    auto shortName = QFileInfo(fileName).fileName();

    purgeInspectList();
    auto *ilist  = new InspectData;
    ilist->info  = nullptr;
    ilist->data  = nullptr;
    ilist->image = nullptr;
    inspectList.append(ilist);

    if (file.size() > Cfg::INSPECT_WARN_SIZE) {
        QMessageBox mb;
        mb.setWindowTitle("  Warning:  Large Restart File  ");
        mb.setWindowIcon(windowIcon());
        mb.setText(QString("<center>The restart file ") + shortName + " is large</center>");
        QString details = "Inspecting the restart file %1 with LAMMPS-GUI may need an additional "
                          "%2 GB of free RAM (or more) to proceed";
        mb.setDetailedText(details.arg(shortName).arg(file.size() / Cfg::INSPECT_GB_PER_BYTE));
        mb.setInformativeText("Do you want to continue?");
        mb.setIconPixmap(
            QIcon(":/icons/warning.svg").pixmap(QSize(64, 64), mb.devicePixelRatioF()));
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setDefaultButton(QMessageBox::No);
        mb.setEscapeButton(QMessageBox::No);
        mb.setFont(font());

        auto *button = mb.button(QMessageBox::Yes);
        button->setIcon(QIcon(":/icons/dialog-ok.svg"));
        button = mb.button(QMessageBox::No);
        button->setIcon(QIcon(":/icons/dialog-no.svg"));

        int rv = mb.exec();
        switch (rv) {
            case QMessageBox::No:
                return;
            case QMessageBox::Yes: // fallthrough
            default:
                // do nothing
                break;
        }
    }

    if (!file.open(QIODevice::ReadOnly)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot open file " + fileName + ":",
                file.errorString());
        return;
    }
    file.close();

    if (!isRestartFile(fileName)) {
        warning(this, "LAMMPS-GUI Warning", "File " + fileName + " is not a LAMMPS restart file.");
        return;
    }

    // LAMMPS is not re-entrant, so we can only query LAMMPS when it is not running a simulation
    if (!lammps.isRunning()) {
        startLammps();
        {
            StdoutSilencer guard;
            lammps.command("clear");
            clearVariables();
            lammps.command(QString("read_restart %1").arg(fileName));
        }
        capturer->beginCapture();
        lammps.command("info system group compute fix");
        capturer->endCapture();
        auto info    = capturer->getCapture();
        auto infolog = QString("%1.info.log").arg(fileName);
        QFile dumpinfo(infolog);
        if (dumpinfo.open(QIODevice::WriteOnly)) {
            auto infodata = QString("%1.tmp.data").arg(fileName);
            dumpinfo.write(info.c_str(), info.size());
            dumpinfo.close();
            auto *infoviewer = new FileViewer(
                infolog, this, QString("LAMMPS-GUI: restart info for %1").arg(shortName));
            // this is output from the info command, so it belongs with the log
            viewlayout->addAuxiliaryView(infoviewer, ViewSlot::Log,
                                         QString("Info: %1").arg(shortName));
            ilist->info = infoviewer;
            dumpinfo.remove();
            // read_restart restores the pair style but not the kspace style, so a
            // pair style that uses long-range Coulomb/dispersion would make the
            // render "run 0" in the image viewer abort with "...requires a KSpace
            // style". Probe with a silenced no-op run and let LAMMPS be the oracle:
            // only when it reports that a KSpace style is required do we add the
            // harmless "kspace_style zero" (which computes nothing but satisfies the
            // requirement). Pair styles that do not use kspace are left as "none"
            // (the read_restart default) -- adding a kspace style would itself error
            // there (no per-atom charge / incompatible pair style). lastErrorMessage()
            // both reads and clears the error, so the probe leaves LAMMPS clean.
            QString kspaceerr;
            {
                StdoutSilencer guard;
                lammps.command("run 0 post no");
                kspaceerr = lammps.lastErrorMessage();
            }
            if (kspaceerr.contains("requires a KSpace style")) {
                StdoutSilencer guard;
                lammps.command("kspace_style zero 1.0e-6");
            }
            {
                StdoutSilencer guard;
                lammps.command(QString("write_data %1 pair ij noinit").arg(infodata));
            }
            auto *dataviewer = new FileViewer(
                infodata, this, QString("LAMMPS-GUI: data file for %1").arg(shortName));
            viewlayout->addAuxiliaryView(dataviewer, ViewSlot::Chart,
                                         QString("Data: %1").arg(shortName));
            ilist->data = dataviewer;
            QFile(infodata).remove();
            auto *inspect_image = new ImageViewer(fileName, &lammps, this);
            inspect_image->setFont(font());
            if (!dockedLayout())
                inspect_image->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
            viewlayout->addAuxiliaryView(inspect_image, ViewSlot::Chart,
                                         QString("Image: %1").arg(shortName));
            ilist->image = inspect_image;
        }
    }
}

// write file and update CWD to its folder

void LammpsGui::writeFile(const QString &fileName)
{
    // empty name means the save dialog was cancelled: nothing to save to
    if (fileName.isEmpty()) return;

    QFileInfo path(fileName);
    QFile file(path.absoluteFilePath());

    if (!file.open(QIODevice::WriteOnly | QFile::Text)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot save to file " + fileName + ":",
                file.errorString());
        return;
    }
    // update the session state only after the file was opened successfully
    currentFile = path.fileName();
    currentDir  = path.absolutePath();
    updateEditorTitle(currentFile);
    QDir::setCurrent(currentDir);

    updateRecents(path.absoluteFilePath());

    QTextStream out(&file);
    QString text = textEdit->toPlainText();
    out << text;
    if (!text.endsWith('\n')) out << "\n"; // add final newline if missing
    file.close();
    dirstatus->setText(QString(" Directory: ") + currentDir);
    // update list of files for completion since we may have changed the working directory
    textEdit->setFileList();
    textEdit->document()->setModified(false);
}

void LammpsGui::save()
{
    purgeInspectList();
    QString fileName = currentFile;
    // If we don't have a filename from before, get one.
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getSaveFileName(
            this, "Save", QDir::current().absoluteFilePath(defaultFileStem(currentFile) + ".lmp"),
            Cfg::FILTER_INPUT);
        fileName = ensureFileSuffix(fileName, "lmp");
    }

    writeFile(fileName);
}

void LammpsGui::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Save as", QDir::current().absoluteFilePath(defaultFileStem(currentFile) + ".lmp"),
        Cfg::FILTER_INPUT);
    fileName = ensureFileSuffix(fileName, "lmp");
    writeFile(fileName);
}

void LammpsGui::quit()
{
    if (lammps.isRunning()) {
        stopRun();
        runner->wait();
        runner->deleteLater();
        runner = nullptr;
    }

    autoSave();
    if (textEdit->document()->isModified()) {
        int rv = showUnsavedChangesDialog(this, currentFile,
                                          "Do you want to save the file before exiting?");
        switch (rv) {
            case QMessageBox::Yes:
                save();
                break;
            case QMessageBox::Cancel:
                return;
            case QMessageBox::No: // fallthrough
            default:
                // do nothing
                break;
        }
    }

    // store some global settings
    QSettings settings;
    if (!isMaximized()) {
        const bool docked = dockedLayout();
        settings.setValue(docked ? Keys::DOCKMAINX : Keys::MAINX, width());
        settings.setValue(docked ? Keys::DOCKMAINY : Keys::MAINY, height());
    }
    settings.sync();

#if QT_CONFIG(clipboard)
    if (auto *clip = QGuiApplication::clipboard()) clip->clear();
#endif

    // tear down LAMMPS-GUI and close / finalize LAMMPS instance

    removeEventFilter(this);
    {
        StdoutSilencer guard;
        lammps.finalize();
    }
    lammpsstatus->hide();

    // quit application
    QCoreApplication::quit();
}

void LammpsGui::copy()
{
#if QT_CONFIG(clipboard)
    textEdit->copy();
#endif
}

void LammpsGui::cut()
{
#if QT_CONFIG(clipboard)
    textEdit->cut();
#endif
}

void LammpsGui::paste()
{
#if QT_CONFIG(clipboard)
    textEdit->paste();
#endif
}

void LammpsGui::undo()
{
    textEdit->undo();
}

void LammpsGui::redo()
{
    textEdit->redo();
}

void LammpsGui::stopRun()
{
    lammps.forceTimeout();
}

void LammpsGui::logUpdate()
{
    progress->setValue(updateRunStatus());

    if (logwindow) {
        const auto text = capturer->getChunk();
        if (!text.empty()) {
            logwindow->moveCursor(QTextCursor::End);
            logwindow->insertPlainText(text.c_str());
            logwindow->moveCursor(QTextCursor::End);
        }
    }

    // get timestep
    int step = 0;
    if (lammps.extractSetting("bigint") == 4)
        step = lammps.lastThermoAs<int>("step", 0);
    else
        step = static_cast<int>(lammps.lastThermoAs<int64_t>("step", 0));

    // extract cached thermo data when LAMMPS is executing a minimize or run command;
    // never during a dry run, where a kept chart window belongs to a previous run
    if (chartwindow && !dryRunActive && lammps.isRunning()) {
        // thermo data is not yet valid during setup
        if (lammps.lastThermoAs<int>("setup", 0)) return;

        lammps.lastThermo("lock", 0);
        const int ncols = lammps.lastThermoAs<int>("num", 0);
        if (ncols > 0) updateChartData(step, ncols);
        lammps.lastThermo("unlock", 0);
    }

    updateSlideShow();
}

int LammpsGui::updateRunStatus()
{
    if (!lammps.isRunning()) return 1000;

    // estimate completion percentage
    double t_elapsed = lammps.getThermo("cpu");
    double t_remain  = lammps.getThermo("cpuremain");
    double t_total   = t_elapsed + t_remain + 1.0e-10;
    int completed    = t_elapsed / t_total * 1000.0;
    // update cpu usage
    int percent_cpu = static_cast<int>(lammps.getThermo("cpuuse"));
    // clear any pending error messages from polling those thermo keywords
    (void)lammps.lastErrorMessage(); // read-and-clear any pending error

    cpuuse->setText(QString("%1%CPU").arg(percent_cpu, 4));
    // pick a color bucket for the CPU-usage label. Re-applying a stylesheet
    // forces an expensive Qt style re-parse/polish, and this runs on every
    // poll tick (~100 Hz) during a run, so only restyle when the bucket
    // actually changes rather than every tick.
    int bucket; // 0=black 1=darkblue 2=firebrick 3=gold 4=forestgreen
    if (percent_cpu < 25.0 * nthreads)
        bucket = 0;
    else if (percent_cpu < 50.0 * nthreads)
        bucket = 1;
    else if (percent_cpu > 100.0 * nthreads + 50.0)
        bucket = 2;
    else if (percent_cpu < 100.0 * nthreads - 50.0)
        bucket = 2;
    else if (percent_cpu > 100.0 * nthreads + 20.0)
        bucket = 3;
    else if (percent_cpu < 100.0 * nthreads - 20.0)
        bucket = 3;
    else
        bucket = 4;
    if (bucket != lastCpuBucket) {
        lastCpuBucket = bucket;
        switch (bucket) {
            case 0:
                cpuuse->setStyleSheet("QLabel {background-color: black; color: white;}");
                break;
            case 1:
                cpuuse->setStyleSheet("QLabel {background-color: darkblue; color: white;}");
                break;
            case 2:
                cpuuse->setStyleSheet("QLabel {background-color: firebrick; color: white;}");
                break;
            case 3:
                cpuuse->setStyleSheet("QLabel {background-color: gold; color: black;}");
                break;
            default:
                cpuuse->setStyleSheet("QLabel {background-color: forestgreen; color: white;}");
                break;
        }
    }

    // lastThermo("line") is 0-based like the editor block numbers (the LAMMPS
    // thermo line counter starts at -1 and is pre-incremented), so the value
    // is passed to setHighlight() without an offset
    void *ptr = lammps.lastThermo("line", 0);
    if (ptr) textEdit->setHighlight(*static_cast<int *>(ptr), false);

    if (varwindow) {
        int nvar = lammps.idCount("variable");
        QString varinfo("\n");
        for (int i = 0; i < nvar; ++i)
            varinfo += lammps.variableInfo(i);
        if (nvar == 0) varinfo += "  (none)  ";

        varwindow->setText(varinfo);
        varwindow->adjustSize();
    }
    return completed;
}

void LammpsGui::updateChartData(int step, int ncols)
{
    // check if the column assignment has changed
    // if yes, delete charts and start over
    if (chartwindow->numCharts() > 0) {
        int count     = 0;
        bool do_reset = false;
        if (step < chartwindow->getStep()) do_reset = true;
        for (int i = 0, idx = 0; i < ncols; ++i) {
            QString label = lammps.lastThermoString("keyword", i);
            // no need to store the timestep column
            if (label == "Step") continue;
            if (!chartwindow->hasTitle(label, idx)) {
                do_reset = true;
            } else {
                ++count;
            }
            ++idx;
        }
        if (chartwindow->numCharts() != count) do_reset = true;
        if (do_reset) chartwindow->resetCharts();
    }

    if (chartwindow->numCharts() == 0) {
        for (int i = 0; i < ncols; ++i) {
            QString label = lammps.lastThermoString("keyword", i);
            // no need to store the timestep column
            if (label == "Step") continue;
            chartwindow->addChart(label, i);
        }
    }

    for (int i = 0; i < ncols; ++i) {
        const int datatype = lammps.lastThermoAs<int>("type", i);
        chartwindow->addData(step, lastThermoData(lammps, datatype, i), i);
    }
}

void LammpsGui::updateSlideShow()
{
    // update list of available image file names
    QString imagefile = lammps.lastThermoString("imagename", 0);
    if (imagefile.isEmpty()) return;

    const bool showslides = QSettings().value(Keys::VIEWSLIDE, true).toBool();
    if (!slideshow) {
        slideshow = new SlideShow(currentFile, this);
        viewlayout->place(ViewSlot::SlideShow, slideshow);
        viewlayout->setVisible(ViewSlot::SlideShow, showslides);
    } else {
        slideshow->setWindowTitle(
            QString("LAMMPS-GUI - Slide Show - %1 - Run %2").arg(currentFile).arg(runCounter));
        if (showslides) viewlayout->show(ViewSlot::SlideShow);
    }
    slideshow->addImage(imagefile);
}

void LammpsGui::modified()
{
    const QString modflag(" - *modified*");
    auto title = windowTitle().remove(modflag);
    if (textEdit->document()->isModified()) {
        textEdit->setStyleSheet("");
        setWindowTitle(title + modflag);
    } else
        setWindowTitle(title);
}

void LammpsGui::warnHighBufferUsage()
{
    // check stdout capture buffer utilization and print warning message if large

    double bufferuse = capturer->getBufferUse();
    if (bufferuse > Cfg::BUFFER_WARNING_THRESHOLD) {
        int thermo_val = lammps.extractSetting("thermo_every");
        int thermo_suggest =
            Cfg::THERMO_SUGGEST_MULTIPLIER * static_cast<int>(round(bufferuse * thermo_val));
        int update_val =
            QSettings().value(Keys::UPDFREQ, Cfg::DATA_UPDATE_INTERVAL_DEFAULT).toInt();
        int update_suggest = std::max(1, update_val / 5);

        QString mesg1("<p align=\"justify\">The I/O buffer for capturing the LAMMPS screen "
                      "output was used by up to %1%.</p>"
                      "<p align=\"justify\"><b>This can slow down the simulation.</b></p>");
        QString mesg2("<p align=\"justify\">Please consider reducing the amount of output "
                      "to the screen, for example by increasing the thermo interval in the "
                      "input from %1 to %2, or reducing the data update interval in the "
                      "preferences from %3 to %4, or something similar.</p>");

        critical(this, "LAMMPS-GUI Warning: High I/O Buffer Usage",
                 mesg1.arg(static_cast<int>(100.0 * bufferuse)),
                 mesg2.arg(thermo_val).arg(thermo_suggest).arg(update_val).arg(update_suggest));
    }
}

void LammpsGui::finalizeChartData()
{
    if (chartwindow) {
        int step = 0;
        if (lammps.extractSetting("bigint") == 4)
            step = lammps.lastThermoAs<int>("step", 0);
        else
            step = static_cast<int>(lammps.lastThermoAs<int64_t>("step", 0));
        const int ncols = lammps.lastThermoAs<int>("num", 0);
        // decide once before the loop: testing numCharts() per column would stop
        // creating charts as soon as the first addChart() call succeeded
        const bool needcharts = (chartwindow->numCharts() == 0);
        for (int i = 0; i < ncols; ++i) {
            if (needcharts) {
                QString label = lammps.lastThermoString("keyword", i);
                // no need to store the timestep column
                if (label == "Step") continue;
                chartwindow->addChart(label, i);
            }
            const int datatype = lammps.lastThermoAs<int>("type", i);
            chartwindow->addData(step, lastThermoData(lammps, datatype, i), i);
        }
        chartwindow->resetZoom();
        chartwindow->setRangeEnabled(true);
    }
}

void LammpsGui::runDone()
{
    if (logupdater) {
        logupdater->stop();
        delete logupdater;
        logupdater = nullptr;
    }
    progress->setValue(Cfg::PROGRESS_MAXIMUM);
    textEdit->setHighlight(CodeEditor::NO_HIGHLIGHT, false);

    // When a whole run produced not a single captured byte, find out which
    // side lost it while the capture is still active; see probeRunEnd().
    std::string capturereport;
    if (capturer->isUsable() && (capturer->totalRead() == 0))
        capturereport = capturer->probeRunEnd();

    capturer->endCapture();

    if (logwindow) {
        auto log = capturer->getCapture();
        logwindow->insertPlainText(log.c_str());
        // only when the final drain stayed empty too was the output really lost
        if (!capturereport.empty() && log.empty())
            logwindow->appendPlainText(
                QString("[no LAMMPS output was captured during this run: %1]\n")
                    .arg(QString::fromStdString(capturereport)));
        logwindow->moveCursor(QTextCursor::End);
    }

    warnHighBufferUsage();

    if (!dryRunActive) finalizeChartData();

    bool success         = true;
    bool valid           = true;
    const QString errmsg = lammps.lastErrorMessage();

    if (!errmsg.isEmpty()) {
        // ignore "Invalid LAMMPS handle", but report other errors
        if (!errmsg.contains("Invalid LAMMPS handle")) {
            success = false;
        } else {
            valid = false;
        }
    }

    int nline = CodeEditor::NO_HIGHLIGHT;
    if (valid) {
        // lastThermo("line") is 0-based like the editor block numbers, no offset needed
        void *ptr = lammps.lastThermo("line", 0);
        if (ptr) nline = *static_cast<int *>(ptr);
    }

    if (success) {
        status->setText(dryRunActive ? "Input check passed." : Cfg::STATUS_READY);
        cpuuse->setText(Cfg::STATUS_ZERO_CPU);
        if (dryRunActive)
            information(this, "LAMMPS-GUI - Dry Run",
                        "<p>The input passed the dry run "
                        "(setup executed, no timesteps).</p><p>Check the Output window "
                        "for LAMMPS warnings.</p>");
    } else {
        status->setText("Failed.");
        textEdit->setHighlight(nline, true);
        critical(this, "LAMMPS-GUI Error",
                 dryRunActive ? "<p>Error during input dry run:</p>"
                              : "<p>Error running LAMMPS:</p>",
                 QString("<p><pre>%1</pre></p>").arg(errmsg));
    }
    dryRunActive = false;
    textEdit->setCursor(nline);
    textEdit->setFileList();
    progress->hide();
    cpuuse->hide();
    dirstatus->show();
}

void LammpsGui::restartLammps()
{
    if (lammps.isRunning()) {
        warning(this, "LAMMPS-GUI Warning", "Must stop current run before relaunching LAMMPS");
        return;
    }
    {
        StdoutSilencer guard;
        lammps.close();
    }
}

void LammpsGui::createLogWindow(QSettings &settings)
{
    // reuse an existing window: it keeps the position and size it was given
    // on screen, and in a docked layout it stays where it was docked
    if (logwindow) {
        logwindow->reset(currentFile);
    } else {
        logwindow = new LogWindow(currentFile, this);
        logwindow->setReadOnly(true);
        logwindow->setCenterOnScroll(true);
        logwindow->setLineWrapMode(LogWindow::NoWrap);
    }
    logwindow->moveCursor(QTextCursor::End);
    logwindow->setWindowTitle(
        QString("LAMMPS-GUI - Output - %1 - Run %2").arg(currentFile).arg(runCounter));
    logwindow->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // a dock area decides the size of its panel, and an explicit minimum only
    // fights it: it becomes a hard floor that keeps the default split from
    // settling where it was asked to
    if (!dockedLayout()) logwindow->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);

    viewlayout->place(ViewSlot::Log, logwindow);
    viewlayout->setVisible(ViewSlot::Log, settings.value(Keys::VIEWLOG, true).toBool());
}

// The capture's own marker only proves that the *executable's* writes reach
// the pipe.  The library is a separate module with runtime state of its own,
// so push one line through it as well, while the instance is still idle, and
// drain the marker again.  Which library answers depends on the configured
// plugin path, not on what sits next to the executable, so a failure names
// the file actually loaded.  Must run after beginCapture() and before the
// runner thread starts issuing commands of its own.
void LammpsGui::verifyLibraryCapture()
{
    capturewarning.clear();
    if (!capturer->isUsable() || !lammps.isOpen()) return;

    lammps.command("print \"__LGUI_LIBCAP__\"");
    std::string got;
    for (int i = 0; i < 100; ++i) {
        got += capturer->getChunk();
        if (got.find("__LGUI_LIBCAP__") != std::string::npos) return; // proven; marker drained
        QThread::msleep(1);
    }
    capturewarning = "the library's own output does not reach the capture although the"
                     " executable's does; loaded library: ";
    capturewarning += pluginPath.isEmpty() ? QString("(linked into the executable)") : pluginPath;
}

// Must run *after* createLogWindow(): on the first run of a session there is no
// log window before that, and a message with nowhere to go is dropped -- which
// on a fresh start is exactly when it is needed.
void LammpsGui::reportCaptureFailure()
{
    if (!logwindow) return;
    // Say so rather than showing an empty window: when stdout cannot be
    // redirected there is no error anywhere else -- the runtime accepts the
    // library's output and drops it, and printf() reports success.
    if (!capturer->isUsable())
        logwindow->appendPlainText(QString("[LAMMPS output cannot be captured: %1]\n")
                                       .arg(QString::fromStdString(capturer->diagnostic())));
    if (!capturewarning.isEmpty())
        logwindow->appendPlainText(
            QString("[LAMMPS output will not be shown: %1]\n").arg(capturewarning));
}

void LammpsGui::createChartWindow(QSettings &settings)
{
    // reuse an existing window: it keeps the position and size it was given
    // on screen, and in a docked layout it stays where it was docked
    if (chartwindow) {
        chartwindow->reset(currentFile);
    } else {
        chartwindow = new ChartWindow(currentFile, this);
        // post-processing results with a new x axis open windows of their own;
        // adopting them keeps them as tabs in the docked layout
        connect(chartwindow, &ChartWindow::resultWindowCreated, this, &LammpsGui::adoptChartResult);
    }
    chartwindow->setWindowTitle(
        QString("LAMMPS-GUI - Charts - %1 - Run %2").arg(currentFile).arg(runCounter));
    chartwindow->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // a dock area decides the size of its panel, and an explicit minimum only
    // fights it: it becomes a hard floor that keeps the default split from
    // settling where it was asked to
    if (!dockedLayout()) chartwindow->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);

    const auto *unitptr = static_cast<const char *>(lammps.extractGlobal("units"));
    if (unitptr) chartwindow->setUnits(QString::fromUtf8(unitptr));
    auto normflag = lammps.extractSetting("thermo_norm");
    chartwindow->setNorm(normflag != 0);
    chartwindow->setRangeEnabled(false);

    viewlayout->place(ViewSlot::Chart, chartwindow);
    viewlayout->setVisible(ViewSlot::Chart, settings.value(Keys::VIEWCHART, true).toBool());
}

namespace {

// show the input check findings; with askRunAnyway the dialog offers
// Yes ("run anyway") / No, otherwise just OK
int showLintDialog(QWidget *parent, const QList<LintIssue> &issues, bool askRunAnyway)
{
    constexpr int MAX_SHOWN = 8;
    const int nerrors       = SyntaxChecker::countErrors(issues);

    QMessageBox mb(parent);
    mb.setWindowTitle("  LAMMPS-GUI - Input Check  ");
    mb.setWindowIcon(parent->windowIcon());
    QString text = QString("<p>The input check found %1 possible problem(s) "
                           "(%2 error(s), %3 warning(s)):</p><pre>%4</pre>")
                       .arg(issues.size())
                       .arg(nerrors)
                       .arg(issues.size() - nerrors)
                       .arg(SyntaxChecker::formatIssues(issues, MAX_SHOWN).toHtmlEscaped());
    if (askRunAnyway) text += "<p>Do you want to run the input anyway?</p>";
    mb.setText(text);
    if (issues.size() > MAX_SHOWN) mb.setDetailedText(SyntaxChecker::formatIssues(issues));
    mb.setIconPixmap(QIcon(":/icons/warning.svg").pixmap(QSize(64, 64), mb.devicePixelRatioF()));
    if (askRunAnyway) {
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setDefaultButton(QMessageBox::No);
        mb.setEscapeButton(QMessageBox::No);
        mb.button(QMessageBox::Yes)->setIcon(QIcon(":/icons/dialog-ok.svg"));
        mb.button(QMessageBox::No)->setIcon(QIcon(":/icons/dialog-no.svg"));
    } else {
        mb.setStandardButtons(QMessageBox::Ok);
        mb.button(QMessageBox::Ok)->setIcon(QIcon(":/icons/dialog-ok.svg"));
    }
    mb.setFont(parent->font());
    return mb.exec();
}

} // namespace

QStringList LammpsGui::presetVariableNames() const
{
    QStringList presets = {QStringLiteral("gui_run")};
    // only variables with a value are defined before the run (same filter as
    // the variable setup in doRun()); the Set Variables dialog also lists
    // used-but-undefined variables with an empty value
    for (const auto &var : variables)
        if (!var.name.isEmpty() && !var.value.isEmpty()) presets << var.name;
    return presets;
}

bool LammpsGui::confirmLintIssues()
{
    const SyntaxChecker checker(&syntax);
    const auto issues =
        checker.check(textEdit->toPlainText(), presetVariableNames(), QDir::currentPath());
    if (issues.isEmpty()) return true;

    // warnings never block a run; note them in the status bar
    if (SyntaxChecker::countErrors(issues) == 0) {
        status->setText(QString("Ready. The input check found %1 warning(s) - "
                                "see Run > Check Input")
                            .arg(issues.size()));
        return true;
    }

    if (showLintDialog(this, issues, true) == QMessageBox::Yes) return true;

    // move the cursor to the first error
    for (const auto &issue : issues) {
        if (issue.severity == LintSeverity::Error) {
            textEdit->setCursor(issue.line - 1);
            textEdit->setHighlight(issue.line - 1, true);
            break;
        }
    }
    return false;
}

void LammpsGui::checkInput()
{
    refreshVariables();
    const SyntaxChecker checker(&syntax);
    const auto issues =
        checker.check(textEdit->toPlainText(), presetVariableNames(), QDir::currentPath());
    if (issues.isEmpty()) {
        textEdit->setHighlight(CodeEditor::NO_HIGHLIGHT, false);
        information(this, "LAMMPS-GUI - Input Check", "No problems found.");
        return;
    }
    showLintDialog(this, issues, false);
    // highlight the first error-level finding like a run error; with only
    // warnings just move the cursor to the first finding
    for (const auto &issue : issues) {
        if (issue.severity == LintSeverity::Error) {
            textEdit->setHighlight(issue.line - 1, true);
            return;
        }
    }
    textEdit->setCursor(issues.first().line - 1);
}

void LammpsGui::doRun(bool use_buffer, bool dryrun)
{
    if (lammps.isRunning()) {
        warning(this, "LAMMPS-GUI Warning", "Must stop current run before starting a new run");
        return;
    }

    // a dry run executes the setup of every command: make the side effects clear
    if (dryrun) {
        QMessageBox mb(this);
        mb.setWindowTitle("  LAMMPS-GUI - Dry Run  ");
        mb.setWindowIcon(windowIcon());
        mb.setText("<p>A dry run executes the setup phase of every command in the "
                   "input without running any timesteps.</p>"
                   "<p>Output files (dumps, logs, data files) may still be created or "
                   "overwritten, and shell commands in the input will be executed.</p>"
                   "<p>Continue?</p>");
        mb.setIconPixmap(QIcon(":/icons/warning.svg").pixmap(QSize(64, 64), devicePixelRatioF()));
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setDefaultButton(QMessageBox::Yes);
        mb.setEscapeButton(QMessageBox::No);
        mb.button(QMessageBox::Yes)->setIcon(QIcon(":/icons/dialog-ok.svg"));
        mb.button(QMessageBox::No)->setIcon(QIcon(":/icons/dialog-no.svg"));
        mb.setFont(font());
        if (mb.exec() != QMessageBox::Yes) return;
    }

    purgeInspectList();
    autoSave();
    if (!use_buffer && textEdit->document()->isModified()) {
        int rv = showUnsavedChangesDialog(this, currentFile,
                                          "Do you want to save the buffer before running LAMMPS?");
        switch (rv) {
            case QMessageBox::Yes:
                save();
                break;
            case QMessageBox::No:
                break;
            case QMessageBox::Cancel: // fallthrough
            default:
                return;
        }
    }

    // fold input script edits into the variables list before it is consumed
    // by the pre-run input check and the variable setup below
    refreshVariables();

    QSettings settings;
    // pre-run input check: only error findings gate the run; a dry run
    // needs no gate since it is itself the check
    if (!dryrun && settings.value(Keys::LINTCHECK, true).toBool() && !confirmLintIssues()) return;

    progress->setValue(0);
    dirstatus->hide();
    progress->show();
    cpuuse->show();
    lastCpuBucket = -1; // force the cpuuse stylesheet to be applied on the first poll

    int numthreads = nthreads;
    int accel      = settings.value(Keys::ACCELERATOR, AcceleratorTab::OpenMP).toInt();
    if ((accel != AcceleratorTab::OpenMP) && (accel != AcceleratorTab::Intel) &&
        (accel != AcceleratorTab::Kokkos) && (accel != AcceleratorTab::Gpu))
        numthreads = 1;
    if (dryrun)
        status->setText(QString("Checking input with a dry run ..."));
    else if (numthreads > 1)
        status->setText(QString("Running LAMMPS with %1 thread(s)...").arg(numthreads));
    else
        status->setText(QString("Running LAMMPS ..."));
    status->repaint();
    startLammps();
    if (!lammps.isOpen()) return;
    capturer->beginCapture();
    verifyLibraryCapture();

    ++runCounter;
    updateEditorTitle(currentFile);

    // must delete all variables since clear does not delete them
    clearVariables();

    // define "gui_run" variable set to runCounter value
    lammps.command(QString("variable gui_run index %1").arg(runCounter));

    // re-create index variables from the Set Variables dialog so they
    // override definitions in the input, like -var does on the command line
    for (const auto &var : std::as_const(variables)) {
        if (!var.name.isEmpty() && !var.value.isEmpty())
            lammps.command(QString("variable %1 index %2").arg(var.name, var.value));
    }

    // apply https proxy setting: prefer environment variable or fall back to preferences value
    applyProxySetting(lammps, settings);

    dryRunActive = dryrun;
    if (dryrun) {
        // the equivalent of the -skiprun command line flag (see lammps.cpp):
        // run and minimize commands stop right after their setup phase.  The
        // timer command must be issued after our own "clear" since clear
        // recreates the Timer class (so the runner must not clear again);
        // no input lines are prepended, so error line numbers stay correct
        lammps.command("clear");
        lammps.command("timer timeout 0 every 1");
        launchRunner((textEdit->toPlainText() + "\n").toStdString(), {}, false);
    } else if (use_buffer) {
        // always add final newline since the text edit widget does not do it
        launchRunner((textEdit->toPlainText() + "\n").toStdString(), {}, true);
    } else {
        launchRunner({}, currentFile.toStdString(), true);
    }

    createLogWindow(settings);
    reportCaptureFailure();
    if (dryrun) {
        if (logwindow) logwindow->setWindowTitle(logwindow->windowTitle() + " (Dry Run)");
        // no chart window and no slide show reset: a dry run produces no
        // trajectory, and a stale chart window must not receive updates
        return;
    }

    createChartWindow(settings);

    if (slideshow) {
        slideshow->setWindowTitle(QString("LAMMPS-GUI - Slide Show - " + currentFile));
        slideshow->clear();
        viewlayout->hide(ViewSlot::SlideShow);
    }
}

void LammpsGui::launchRunner(std::string input, std::string file, bool clearfirst)
{
    runner = new LammpsRunner(this);
    runner->setupRun(&lammps, std::move(input), std::move(file), clearfirst);

    connect(runner, &LammpsRunner::resultReady, this, &LammpsGui::runDone);
    connect(runner, &LammpsRunner::finished, runner, &QObject::deleteLater);
    runner->start();

    QSettings settings;
    logupdater = new QTimer(this);
    connect(logupdater, &QTimer::timeout, this, &LammpsGui::logUpdate);
    logupdater->start(settings.value(Keys::UPDFREQ, Cfg::DATA_UPDATE_INTERVAL_DEFAULT).toInt());
}

void LammpsGui::extendRun()
{
    if (lammps.isRunning()) {
        warning(this, "LAMMPS-GUI Warning", "Must stop the current run before extending it");
        return;
    }
    if (!hasSystemState()) {
        warning(this, "LAMMPS-GUI Warning",
                "Cannot extend a run without a system state.\n"
                "Must run the input at least to the point where the system is defined.");
        return;
    }

    bool ok = false;
    const int nsteps =
        QInputDialog::getInt(this, "Extend Run", "Number of steps to add:", extendSteps, 1,
                             std::numeric_limits<int>::max(), 1, &ok);
    if (!ok) return;
    extendSteps = nsteps;

    QSettings settings;
    progress->setValue(0);
    dirstatus->hide();
    progress->show();
    cpuuse->show();
    lastCpuBucket = -1; // force the cpuuse stylesheet to be applied on the first poll
    status->setText(QString("Extending run by %1 steps ...").arg(nsteps));
    status->repaint();

    capturer->beginCapture();
    verifyLibraryCapture();

    // append to the windows of the extended run; create them only when missing
    // (e.g. when extending the state of an inspected restart file)
    if (!logwindow) createLogWindow(settings);
    if (!chartwindow) createChartWindow(settings);
    reportCaptureFailure();

    logwindow->moveCursor(QTextCursor::End);
    logwindow->insertPlainText(
        QString("\n========== Extending run by %1 steps ==========\n\n").arg(nsteps));
    logwindow->moveCursor(QTextCursor::End);

    // "timer timeout off" resets the expired walltime timer that the Stop button
    // leaves behind.  The setup phase must not be skipped with "pre no": each run
    // executes on a new runner thread with its own OpenMP thread pool, and only
    // the setup re-initializes the per-thread data of threaded accelerator
    // packages for that pool (e.g. FixOMP::init()), so "pre no" crashes such runs.
    launchRunner(QString("timer timeout off\nrun %1 start 0\n").arg(nsteps).toStdString(), {},
                 false);
}

void LammpsGui::plotDataFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Data File to Plot",
                                                    QDir::currentPath(), Cfg::FILTER_DATA);
    if (fileName.isEmpty()) return;
    plotFile(fileName);
}

// the same for a file that is already in hand -- the command window's "plot"
// hands one over after the shell has expanded it.  Returns false only when the
// user canceled the column dialog, which a caller with more files to plot takes
// as "stop" rather than "ask me again for each of them".
bool LammpsGui::plotFile(const QString &fileName)
{
    // the parsers report what they could not read, but a picture handed to the
    // plotter is a mistake to catch before that rather than an error to explain
    if (looksLikeBinaryFile(fileName) && !confirmUnexpectedFile(this, fileName, "data")) {
        // declining this one file is not declining the rest of them
        return true;
    }

    QString error;
    // the block-structured output of the fix ave/* styles is not a flat table
    // and gets the import dialog that can reduce it to one
    const PlotBlockData blocks = loadPlotBlockData(fileName);
    PlotData data;
    if (blocks.isEmpty()) {
        data = loadPlotData(fileName, &error);
        if (data.isEmpty()) {
            critical(this, "Plot Data File",
                     "Could not read data from file:", error.isEmpty() ? fileName : error);
            // the file was the problem, not the user, so a caller with more of
            // them carries on to the next
            return true;
        }
    }

    auto dialog = blocks.isEmpty() ? std::make_unique<PlotDataDialog>(data, this)
                                   : std::make_unique<PlotDataDialog>(blocks, this);
    if (dialog->exec() != QDialog::Accepted) return false;
    const QList<int> ycols = dialog->yColumns();
    if (ycols.isEmpty()) {
        warning(this, "Plot Data File", "No data columns were selected to plot.");
        return true;
    }

    const PlotData plotData = dialog->buildData();

    // standalone chart window (no live simulation); cleans itself up on close
    auto *win = new ChartWindow(fileName, nullptr);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->setWindowTitle(QString("Plot: %1 - LAMMPS-GUI").arg(QFileInfo(fileName).fileName()));
    win->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // a minimum size becomes a floor the dock area cannot get below
    if (!dockedLayout()) win->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
    win->loadData(plotData, dialog->xColumn(), ycols, dialog->buildErrors());
    // combined layout: the plot joins the tab group on the right, next to the
    // charts of the current run; adoption also chains its post-processing
    // result windows into the same group
    adoptChartResult(win, QString("Plot: %1").arg(QFileInfo(fileName).fileName()));
    return true;
}

void LammpsGui::adoptChartResult(ChartWindow *win, const QString &title)
{
    connect(win, &ChartWindow::resultWindowCreated, this, &LammpsGui::adoptChartResult);
    viewlayout->addAuxiliaryView(win, ViewSlot::Chart, title);
}

void LammpsGui::renderImage()
{
    // LAMMPS is not re-entrant, so we can only query LAMMPS when it is not running
    if (!lammps.isRunning()) {
        startLammps();
        if (!lammps.extractSetting("box_exist")) {
            // there is no current system defined yet.
            // so we select the input from the start to the first run or minimize command
            // add a run 0 and thus create the state of the initial system without running.
            // this will allow us to create a snapshot image.
            auto saved = textEdit->textCursor();
            textEdit->moveCursor(QTextCursor::Start);
            if (textEdit->find(QRegularExpression(QStringLiteral(R"(^\s*(run|minimize)\s+)")))) {
                auto cursor = textEdit->textCursor();
                cursor.movePosition(QTextCursor::PreviousBlock);
                cursor.movePosition(QTextCursor::EndOfLine);
                cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
                auto selection = cursor.selectedText().replace(QChar(0x2029), '\n');
                selection += "\nrun 0 pre yes post no";
                textEdit->setTextCursor(saved);
                {
                    StdoutSilencer guard;
                    lammps.command("clear");
                    clearVariables();
                    lammps.commandsString(selection);
                }

                const QString errmsg = lammps.lastErrorMessage();
                // ignore "Invalid LAMMPS handle", but report other errors
                if (!errmsg.isEmpty() && !errmsg.contains("Invalid LAMMPS handle")) {
                    warning(this, "Image Viewer File Creation Error",
                            "LAMMPS failed to create the image:",
                            QString("<br><code>%1</code>").arg(errmsg));
                    return;
                }
            }
            textEdit->setTextCursor(saved);
            // still no system box. bail out with a suitable message
            if (!lammps.extractSetting("box_exist")) {
                warning(this, "Image Viewer File Creation Error",
                        "Cannot create snapshot image from an input not creating a system box");
                return;
            }
        }

        // Purge the input deck's dump instances before opening the viewer: it
        // renders by creating its own dump and issuing "run 0", and leaving the
        // deck's dumps active would make that run re-trigger them and overwrite
        // their output files. (The walltime timeout the stop button leaves behind
        // is cleared per render in ImageViewer::createImage.)
        {
            StdoutSilencer guard;
            const int ndumps = lammps.idCount("dump");
            QStringList dumpids;
            for (int i = 0; i < ndumps; ++i)
                dumpids << lammps.idName("dump", i);
            for (const auto &id : dumpids)
                lammps.command("undump " + id);
        }

        // delete the old image window before opening the new one
        delete imagewindow;
        imagewindow = new ImageViewer(currentFile, &lammps, this);
        if (!dockedLayout()) imagewindow->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
        viewlayout->place(ViewSlot::Image, imagewindow);
    } else {
        warning(this, "Image Viewer File Creation Error",
                "Cannot create snapshot image while LAMMPS is running");
        return;
    }
    // an explicit request to look at the new image
    viewlayout->raise(ViewSlot::Image);
}

void LammpsGui::viewSlides()
{
    if (!slideshow) {
        slideshow = new SlideShow(currentFile, this);
        if (!dockedLayout()) slideshow->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
        viewlayout->place(ViewSlot::SlideShow, slideshow);
    }
    viewlayout->toggle(ViewSlot::SlideShow);
}

void LammpsGui::viewChart()
{
    viewlayout->toggle(ViewSlot::Chart);
}

void LammpsGui::viewLog()
{
    viewlayout->toggle(ViewSlot::Log);
}

void LammpsGui::viewImage()
{
    // on first use there is no viewer to toggle, so create the snapshot image
    // the way the "Create snapshot image" button does; that also raises it
    if (!imagewindow) {
        renderImage();
        return;
    }
    viewlayout->toggle(ViewSlot::Image);
}

void LammpsGui::createVariableWindow()
{
    varwindow = new QLabel(QString());
    varwindow->setWindowTitle(QString("LAMMPS-GUI - Current Variables"));
    varwindow->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    varwindow->setMinimumSize(100, 50);
    varwindow->setText("(none)");

    varwindow->setFont(monoFontFromSettings());

    varwindow->setFrameStyle(QFrame::Sunken);
    varwindow->setFrameShape(QFrame::Panel);
    varwindow->setAlignment(Qt::AlignVCenter);
    varwindow->setContentsMargins(5, 5, 5, 5);
    varwindow->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    // apply before hide(): applyWindowFlags() calls setWindowFlags(), which re-shows the widget
    applyWindowFlags(varwindow);
    viewlayout->place(ViewSlot::Variables, varwindow);
    viewlayout->hide(ViewSlot::Variables);
}

void LammpsGui::createCommandWindow()
{
    if (commandwindow) return;
    commandwindow = new CommandWindow(this);
    commandwindow->setWindowTitle("LAMMPS-GUI - Commands");
    commandwindow->setWindowIcon(QIcon(Cfg::MAIN_ICON));
    // start where the input file is, which is where a run leaves its output
    commandwindow->changeDirectory(currentDir);
    viewlayout->place(ViewSlot::Command, commandwindow);
}

void LammpsGui::openCommandWindow()
{
    createCommandWindow();
    viewlayout->raise(ViewSlot::Command);
}

void LammpsGui::viewCommand()
{
    // on first use there is nothing to hide, so the toggle opens the window
    if (!commandwindow) {
        openCommandWindow();
        return;
    }
    viewlayout->toggle(ViewSlot::Command);
}

void LammpsGui::viewVariables()
{
    // varwindow is destroyed when the editor is reset (newDocument()/openFile()),
    // so recreate it on demand here -- mirrors viewSlides()
    if (!varwindow) createVariableWindow();
    viewlayout->toggle(ViewSlot::Variables);
}

// Docked, the views are named by their tab and no longer carry the run number
// in a window title of their own, so the editor title takes it over.
void LammpsGui::updateEditorTitle(const QString &file)
{
    QString title = "LAMMPS-GUI - Editor - " + (file.isEmpty() ? QString("*unknown*") : file);
    if (viewlayout && viewlayout->mode() == LayoutMode::Docked && runCounter > 0)
        title += QString(" - Run %1").arg(runCounter);
    setWindowTitle(title);
}

void LammpsGui::setDocver()
{
    QString git_branch = static_cast<const char *>(lammps.extractGlobal("git_branch"));
    if ((git_branch == "stable") || (git_branch == "maintenance")) {
        docver = "/stable/";
    } else if (git_branch == "release") {
        docver = "/";
    } else {
        docver = "/latest/";
    }
}

void LammpsGui::autoSave()
{
    // no need to auto-save, if the document has no name or is not modified.
    QString fileName = currentFile;
    if (fileName.isEmpty()) return;
    if (!textEdit->document()->isModified()) return;

    // check preference
    bool autosave = false;
    QSettings settings;
    settings.beginGroup(Keys::GROUP_REFORMAT);
    autosave = settings.value(Keys::AUTOSAVE, false).toBool();
    settings.endGroup();

    if (autosave) writeFile(fileName);
}

void LammpsGui::setFont(const QFont &newFont)
{
    QMainWindow::setFont(newFont);
    if (textEdit) {
        textEdit->setFont(newFont);
        menubar->setFont(newFont);
    }
}

void LammpsGui::about()
{
    std::string version = "<b>This is LAMMPS-GUI version " LAMMPS_GUI_VERSION "</b>\n";
    version += "<ul><li> with Qt version " QT_VERSION_STR "</li>\n";
    if (isLightTheme())
        version += "<li>with light theme</li>\n";
    else
        version += "<li>with dark theme</li>\n";
    // name the layout the way the preferences dialog does
    if (dockedLayout())
        version += "<li>with combined main window layout</li>\n";
    else
        version += "<li>with individual windows layout</li>\n";
    version += "</ul>\n";
    if (lammps.hasPlugin()) {
        version += "LAMMPS library loaded as plugin";
        if (!pluginPath.isEmpty()) {
            version += "<br>\n from file ";
            version += pluginPath.toStdString();
        }
    } else {
        version += "LAMMPS library linked to executable";
    }

    QString to_clipboard(version.c_str());
    to_clipboard += "\n\n";

    std::string info    = "LAMMPS is currently running. LAMMPS config info not available.\n";
    std::string details = "";

    // LAMMPS is not re-entrant, so we can only query LAMMPS when it is not running
    if (!lammps.isRunning()) {
        startLammps();
        capturer->beginCapture();
        lammps.command("info config styles");
        capturer->endCapture();
        info       = capturer->getCapture();
        auto start = info.find("LAMMPS version");
        auto mid   = info.find("Styles information", start);
        auto end   = info.find("Info-Info-Info", start);

        // protect from a failed or incomplete capture
        if ((start != std::string::npos) && (mid != std::string::npos) &&
            (end != std::string::npos)) {
            details = std::string(info, mid, end - mid);
            info    = std::string(info, start, mid - start);

            // condense newlines and trailing whitespace in detailed styles info string
            auto loc = details.find("\n\n\n\n");
            while (loc != std::string::npos) {
                details.replace(loc, 4, "\n\n");
                loc = details.find("\n\n\n\n");
            }
            loc = details.find("\r\n\r\n\r\n\r\n");
            while (loc != std::string::npos) {
                details.replace(loc, 8, "\r\n\r\n");
                loc = details.find("\r\n\r\n\r\n\r\n");
            }
            loc = details.find("les:\n\n");
            while (loc != std::string::npos) {
                details.replace(loc, 6, "les:\n");
                loc = details.find("les:\n\n");
            }
            loc = details.find("les:\r\n\r\n");
            while (loc != std::string::npos) {
                details.replace(loc, 8, "les:\r\n");
                loc = details.find("les:\r\n\r\n");
            }
            loc = details.find(" \n");
            while (loc != std::string::npos) {
                details.replace(loc, 2, "\n");
                loc = details.find(" \n");
            }
            loc = details.find(" \r\n");
            while (loc != std::string::npos) {
                details.replace(loc, 3, "\r\n");
                loc = details.find(" \r\n");
            }
        }
    }

    info += citeme.toStdString();
    to_clipboard += info.c_str();
    to_clipboard += details.c_str();

#if QT_CONFIG(clipboard)
    if (auto *clip = QGuiApplication::clipboard()) clip->setText(to_clipboard);
#endif

    auto fsize = QFontMetrics(QApplication::font()).size(Qt::TextSingleLine, citeme);
    AboutDialog dialog(QString::fromStdString(version).trimmed(),
                       QString::fromStdString(info).trimmed(),
                       QString::fromStdString(details).trimmed(), fsize.width(), this);
    dialog.exec();
}

#if defined(LAMMPS_GUI_USE_PLUGIN)
void LammpsGui::checkUpdate()
{
    const auto libName   = getLammpsLibName();
    const auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    auto libPath         = configDir + QDir::separator() + libName;
    auto dlUrl           = getLammpsDownloadUrl();

    if (dlUrl.isEmpty()) {
        information(this, "Check for LAMMPS Update",
                    "The pre-compiled LAMMPS shared libraries from the LAMMPS webserver "
                    "are not compatible with this LAMMPS-GUI executable. Please compile "
                    "a matching LAMMPS shared library yourself and select it in the "
                    "preferences dialog.");
        return;
    }

    if (!QFile::exists(libPath)) {
        information(this, "Check for LAMMPS Update",
                    "No pre-compiled LAMMPS library found in the configuration folder. "
                    "Click on 'Download LAMMPS shared library' in the preferences dialog "
                    "to download one.");
        return;
    }

    URLDownloader downloader(this);
    QString expectedHash = downloader.getRemoteChecksum(dlUrl);
    if (expectedHash.isEmpty()) {
        critical(this, "Check for LAMMPS Update", "Failed to retrieve remote checksum.",
                 downloader.errorString());
        return;
    }

    QString actualHash = URLDownloader::getLocalChecksum(libPath);
    if (actualHash == expectedHash) {
        information(this, "Check for LAMMPS Update",
                    "Your downloaded LAMMPS shared library is up-to-date.");
        return;
    } else {
        QMessageBox mb(this);
        mb.setWindowTitle("Check for LAMMPS Shared Library Update");
        mb.setText("An updated pre-compiled LAMMPS shared library is available. ");
        mb.setInformativeText("Do you want to download it now?");
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
        mb.setIconPixmap(QPixmap(":/icons/lammps-plugin.png").scaled(96, 96));

        // customize button icons
        auto *button = mb.button(QMessageBox::Yes);
        button->setIcon(QIcon(":/icons/dialog-ok.svg"));
        button = mb.button(QMessageBox::No);
        button->setIcon(QIcon(":/icons/dialog-no.svg"));

        if (mb.exec() == QMessageBox::Yes) {
            if (downloader.download(dlUrl, libPath, true, true)) {
                warning(this, "LAMMPS Shared Library Updated",
                        "The latest LAMMPS library has been downloaded successfully. "
                        "LAMMPS-GUI must be relaunched to activate it.");
                relaunchApplication();
            } else {
                critical(this, "Check for LAMMPS Update",
                         "Failed to download LAMMPS shared library.", downloader.errorString());
            }
        }
        return;
    }
}
#endif

void LammpsGui::help()
{
    QMessageBox mb(this);
    mb.setWindowTitle("LAMMPS-GUI Quick Help");
    mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
    mb.setText("<div>This is LAMMPS-GUI version " LAMMPS_GUI_VERSION "</div>");
    mb.setInformativeText(
        "<p>LAMMPS-GUI is a graphical text editor that is customized for "
        "editing LAMMPS input files and linked to the LAMMPS "
        "library and thus can run LAMMPS directly using the contents of the "
        "text buffer as input. It can retrieve and display information from "
        "LAMMPS while it is running and display visualizations created "
        "with the dump image command.</p>"
        "<p>The main window of the LAMMPS-GUI is a text editor window with "
        "LAMMPS specific syntax highlighting. When typing <b>Ctrl-Enter</b> "
        "or clicking on 'Run LAMMPS from Editor Buffer' in the 'Run' menu, "
        "LAMMPS will be run "
        "with the contents of editor buffer as input. The output of the LAMMPS "
        "run is captured and displayed in an Output window. The thermodynamic data "
        "is displayed in a chart window. Both are updated regularly during the "
        "run, as is a progress bar in the main window. The running simulation "
        "can be stopped cleanly by typing <b>Ctrl-/</b> or by clicking on "
        "'Stop LAMMPS' in the 'Run' menu. While LAMMPS is not running, "
        "an image of the simulated system can be created and shown in an image "
        "viewer window by typing <b>Ctrl-i</b> or by clicking on 'Create Image' "
        "in the 'Run' menu. Multiple image settings can be changed through the "
        "buttons in the menu bar and the image will be re-rendered. In case "
        "an input file contains a dump image command, LAMMPS-GUI will load "
        "the images as they are created and display them in a slide show. </p>"
        "<p>When opening a file, the editor will determine the directory "
        "where the input file resides and switch its current working directory "
        "to that same folder and thus enabling the run to read other files in "
        "that folder, e.g. a data file. The GUI will show its current working "
        "directory in the status bar. In addition to using the menu, the "
        "editor window can also receive files as the first command line "
        "argument or via drag-n-drop from a graphical file manager or a "
        "desktop environment.</p>"
        "<p>Almost all commands are accessible via keyboard shortcuts. Which "
        "those shortcuts are, is typically shown next to their entries in the "
        "menus. "
        "In addition, the documentation for the command in the current line "
        "can be viewed by typing <b>Ctrl-?</b> or by choosing the respective "
        "entry in the context menu, available by right-clicking the mouse. "
        "Log, chart, slide show, and image windows can be closed with "
        "<b>Ctrl-W</b> and the application terminated with <b>Ctrl-Q</b>.</p>"
        "<p>The 'About LAMMPS-GUI' dialog will show the LAMMPS version and the "
        "features included into the LAMMPS library linked to the LAMMPS-GUI. "
        "A number of settings can be adjusted in the 'Preferences' dialog (in "
        "the 'Edit' menu or from <b>Ctrl-P</b>) which includes selecting "
        "accelerator packages and number of OpenMP threads. Due to its nature "
        "as a graphical application, it is <b>not</b> possible to use the "
        "LAMMPS-GUI in parallel with MPI.</p>");
    mb.setIconPixmap(QPixmap(Cfg::MAIN_ICON).scaled(64, 64));
    mb.setStandardButtons(QMessageBox::Close);
    auto *button = mb.button(QMessageBox::Close);
    button->setIcon(QIcon(":/icons/window-close.svg"));
    mb.setFont(font());
    mb.exec();
}

void LammpsGui::manual()
{
    if (docver.isEmpty()) setDocver();
    QDesktopServices::openUrl(QUrl(Cfg::DOCS_URL + docver));
}

void LammpsGui::tutorialWeb()
{
    QDesktopServices::openUrl(QUrl("https://lammpstutorials.github.io/"));
}

QWizardPage *LammpsGui::tutorialIntro(int collection, int ntutorial, const QString &infotext)
{
    const auto &coll = tutorialCollection(collection);
    auto *page       = new QWizardPage;
    page->setTitle(QString("Getting Started With %1 Tutorial %2").arg(coll.name).arg(ntutorial));
    page->setPixmap(QWizard::WatermarkPixmap, QPixmap(coll.logoFor(ntutorial)));

    QString text = QString("<p>This dialog will help you to select and populate a folder with "
                           "materials required to work through tutorial %1 from the LAMMPS %2 "
                           "tutorials by %3.</p>")
                       .arg(ntutorial)
                       .arg(coll.name, coll.author);
    if (!coll.filesRepoUrl.isEmpty())
        text += QString("<p>The materials for this tutorial are downloaded from:<br>"
                        "<b><a href=\"%1\">%1</a></b></p>")
                    .arg(coll.filesRepoUrl);
    auto *label = new QLabel(text + infotext);
    label->setWordWrap(true);

    auto *layout = new QVBoxLayout;
    layout->addWidget(label);
    page->setLayout(layout);
    return page;
}

QWizardPage *LammpsGui::tutorialDirectory(int collection, int ntutorial)
{
    const auto &coll = tutorialCollection(collection);
    QSettings settings;
    settings.beginGroup(Keys::GROUP_TUTORIAL);
    auto *page = new QWizardPage;
    page->setTitle(QString("Select Directory for %1 Tutorial %2").arg(coll.name).arg(ntutorial));
    page->setPixmap(QWizard::WatermarkPixmap, QPixmap(coll.logoFor(ntutorial)));

    auto *label = new QLabel(
        QString("<p>Select a directory to store the files for tutorial %1.  The directory will be "
                "created if necessary and LAMMPS-GUI will download the files required for the "
                "tutorial.  If selected, an existing directory may be cleared from old "
                "files.</p>\n<p>Available files of the tutorial solution may be downloaded to a "
                "sub-folder called \"solution\", if requested.</p>\n")
            .arg(ntutorial));
    label->setWordWrap(true);

    auto *layout = new QVBoxLayout;
    layout->addWidget(label);

    auto *dirlayout = new QHBoxLayout;
    auto *directory = new QLineEdit;

    // if we are already inside this collection's "<prefix><N>" folder, switch the
    // tutorial number in place; otherwise append a fresh folder (after redirecting
    // away from home/system locations that should not be written to directly)
    const QString folder = coll.dirPrefix + QString::number(ntutorial);
    const int idx        = currentDir.lastIndexOf("/" + coll.dirPrefix);
    bool inCollFolder    = false;
    if (idx >= 0) {
        bool ok        = false;
        QString digits = currentDir.mid(idx + 1 + coll.dirPrefix.size());
        digits.toInt(&ok);
        inCollFolder = ok && !digits.isEmpty();
    }
    if (inCollFolder) {
        currentDir.truncate(idx);
    } else if ((currentDir == QDir::homePath()) || currentDir.contains("AppData") ||
               currentDir.contains("system32") || currentDir.contains("Program Files")) {
        currentDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }
    currentDir.append("/" + folder);
    directory->setText(currentDir);

    auto *dirbutton = new QPushButton("&Choose");
    dirlayout->addWidget(directory);
    dirlayout->addWidget(dirbutton);
    directory->setObjectName("t_directory");
    connect(dirbutton, &QPushButton::released, this, &LammpsGui::getDirectory);
    layout->addLayout(dirlayout);

    auto *purgeval = new QCheckBox("&Remove existing files from directory");
    auto *solval   = new QCheckBox("&Download solutions");

    purgeval->setChecked(false);
    purgeval->setObjectName("t_dirpurge");
    layout->addWidget(purgeval, 0, Qt::AlignVCenter | Qt::AlignLeft);

    solval->setChecked(settings.value(Keys::SOLUTION, false).toBool());
    solval->setObjectName("t_getsolution");
    layout->addWidget(solval, 0, Qt::AlignVCenter | Qt::AlignLeft);

    // only offer the webpage checkbox for collections that have online pages
    QCheckBox *webval = nullptr;
    if (!coll.webUrl.isEmpty()) {
        webval = new QCheckBox("&Open tutorial webpage in web browser");
        webval->setChecked(settings.value(Keys::WEBPAGE, true).toBool());
        webval->setObjectName("t_webopen");
        layout->addWidget(webval, 0, Qt::AlignVCenter | Qt::AlignLeft);
    }

    auto *label2 = new QLabel(
        QString("<hr width=\"33%\">\n<p align=\"center\">Click on "
                "the \"Finish\" button to complete the setup and start the download.</p>"));
    label2->setWordWrap(false);

    layout->addWidget(label2);
    settings.endGroup();

    page->setLayout(layout);
    return page;
}

void LammpsGui::startTutorial(int collection, int tutno)
{
    const auto &coll = tutorialCollection(collection);
    if (tutno < 1 || tutno > coll.count()) return;
    // tutorials beyond the available count are shown in the menu but not launchable yet
    if (tutno > coll.available) return;

    delete wizard;
    wizard = new TutorialWizard(collection, tutno, this);
    const auto infotext =
        coll.blurbs.value(tutno - 1) +
        QString("<hr width=\"33%\">\n<p align=\"center\">Click on the \"Next\" button "
                "to select a folder.</p>");
    wizard->setFont(font());
    wizard->addPage(tutorialIntro(collection, tutno, infotext));
    wizard->addPage(tutorialDirectory(collection, tutno));
    wizard->setWindowTitle(QString("%1 Tutorial %2 Setup Wizard").arg(coll.name).arg(tutno));
    wizard->setWizardStyle(QWizard::ModernStyle);
    wizard->show();
}

void LammpsGui::howto()
{
    QDesktopServices::openUrl(QUrl("https://lammps-gui.lammps.org/"));
}

void LammpsGui::defaults()
{
    QSettings settings;
    settings.clear();
    settings.sync();

    // also delete a LAMMPS shared library that was downloaded into the
    // configuration folder; try the names for all platforms, not just the
    // current one, since the configuration folder may be shared between
    // different machines
    const auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!configDir.isEmpty()) {
        const QDir dir(configDir);
        for (const auto &lib :
             {Cfg::LAMMPS_LIB_MACOS, Cfg::LAMMPS_LIB_WINDOWS, Cfg::LAMMPS_LIB_LINUX}) {
            const auto path = dir.absoluteFilePath(lib);
            // a loaded library is locked against deletion on Windows, but it
            // can still be renamed; the backup is removed on the next launch
            if (QFileInfo::exists(path) && !QFile::remove(path)) renameToBackup(path);
        }
        // remove backups and partial downloads right away where possible
        purgeLibraryLeftovers();
    }
}

void LammpsGui::editVariables()
{
    // sync the dialog with the current state of the input script
    refreshVariables();

    QList<VariableEntry> newvars = variables;
    SetVariables vars(newvars);
    vars.setFont(font());
    if (vars.exec() == QDialog::Accepted) {
        variables = newvars;
        textEdit->setVariableOverrides(variables);
        if (lammps.isRunning()) {
            stopRun();
            runner->wait();
            runner->deleteLater();
            runner = nullptr;
        }
        {
            StdoutSilencer guard;
            lammps.close();
        }
        lammpsstatus->hide();
    }
}

void LammpsGui::findAndReplace()
{
    FindAndReplace find(textEdit, this);
    find.setFont(font());
    find.setObjectName("find");
    find.exec();
}

void LammpsGui::preferences()
{
    // default settings are committed to QSettings during initialization of LAMMPS-GUI
    QSettings settings;
    int oldthreads   = settings.value(Keys::NTHREADS, 1).toInt();
    int oldaccel     = settings.value(Keys::ACCELERATOR, AcceleratorTab::None).toInt();
    bool oldecho     = settings.value(Keys::ECHO, false).toBool();
    bool oldcite     = settings.value(Keys::CITE, false).toBool();
    int oldiprec     = settings.value(Keys::INTELPREC, AcceleratorTab::Mixed).toInt();
    bool oldgpuneigh = settings.value(Keys::GPUNEIGH, true).toBool();
    bool oldgpupair  = settings.value(Keys::GPUPAIRONLY, false).toBool();

    Preferences prefs(&lammps, this);
    prefs.setFont(font());
    prefs.setObjectName("preferences");
    if (prefs.exec() == QDialog::Accepted) {
        // must delete LAMMPS instance after preferences have changed that require
        // using different command line flags when creating the LAMMPS instance like
        // suffixes or package commands
        int newthreads = settings.value(Keys::NTHREADS, nthreads).toInt();
        int newaccel   = settings.value(Keys::ACCELERATOR, AcceleratorTab::None).toInt();
        int newiprec   = settings.value(Keys::INTELPREC, AcceleratorTab::Mixed).toInt();
        if ((oldaccel != newaccel) || (oldthreads != newthreads) || (oldiprec != newiprec) ||
            (oldecho != settings.value(Keys::ECHO, false).toBool()) ||
            (oldcite != settings.value(Keys::CITE, false).toBool()) ||
            (oldgpuneigh != settings.value(Keys::GPUNEIGH, true).toBool()) ||
            (oldgpupair != settings.value(Keys::GPUPAIRONLY, false).toBool())) {
            if (lammps.isRunning()) {
                stopRun();
                runner->wait();
                runner->deleteLater();
                runner = nullptr;
            }
            {
                StdoutSilencer guard;
                lammps.close();
            }
            lammpsstatus->hide();
            // reset nthreads if accelerator does not support threads
            if ((newaccel == AcceleratorTab::Opt) || (newaccel == AcceleratorTab::None))
                nthreads = 1;
            else
                nthreads = newthreads;

            qputenv("OMP_NUM_THREADS", QByteArray::number(nthreads));
        }
        // the settings change above may have torn down the LAMMPS instance;
        // re-rendering then would only produce a spurious error about the
        // missing simulation box
        if (imagewindow && hasSystemState()) imagewindow->createImage();
        settings.beginGroup(Keys::GROUP_REFORMAT);
        textEdit->setReformatOnReturn(settings.value(Keys::RETURN, false).toBool());
        textEdit->setAutoComplete(settings.value(Keys::AUTOMATIC, true).toBool());
        settings.endGroup();
    }
}

void LammpsGui::appendAcceleratorArgs(int accel, QSettings &settings)
{
    if (accel == AcceleratorTab::Opt) {
        lammpsArgs.push_back("-suffix");
        lammpsArgs.push_back("opt");
    } else if (accel == AcceleratorTab::OpenMP) {
        lammpsArgs.push_back("-suffix");
        lammpsArgs.push_back("omp");
        lammpsArgs.push_back("-pk");
        lammpsArgs.push_back("omp");
        lammpsArgs.push_back(std::to_string(nthreads));
    } else if (accel == AcceleratorTab::Intel) {
        lammpsArgs.push_back("-suffix");
        if (lammps.configHasPackage("OPENMP")) {
            lammpsArgs.push_back("hybrid");
            lammpsArgs.push_back("intel");
            lammpsArgs.push_back("omp");
            lammpsArgs.push_back("-pk");
            lammpsArgs.push_back("omp");
            lammpsArgs.push_back(std::to_string(nthreads));
        } else {
            lammpsArgs.push_back("intel");
        }
        lammpsArgs.push_back("-pk");
        lammpsArgs.push_back("intel");
        lammpsArgs.push_back("0");
        lammpsArgs.push_back("omp");
        lammpsArgs.push_back(std::to_string(nthreads));
        lammpsArgs.push_back("mode");
        int iprec = settings.value(Keys::INTELPREC, AcceleratorTab::Mixed).toInt();
        if (iprec == AcceleratorTab::Double)
            lammpsArgs.push_back("double");
        else if (iprec == AcceleratorTab::Mixed)
            lammpsArgs.push_back("mixed");
        else if (iprec == AcceleratorTab::Single)
            lammpsArgs.push_back("single");
        else // use mixed precision for invalid value so there is no syntax error crash
            lammpsArgs.push_back("mixed");
    } else if (accel == AcceleratorTab::Gpu) {
        lammpsArgs.push_back("-suffix");
        if ((nthreads > 1) && lammps.configHasPackage("OPENMP")) {
            lammpsArgs.push_back("hybrid");
            lammpsArgs.push_back("gpu");
            lammpsArgs.push_back("omp");
            lammpsArgs.push_back("-pk");
            lammpsArgs.push_back("omp");
            lammpsArgs.push_back(std::to_string(nthreads));
        } else {
            lammpsArgs.push_back("gpu");
        }
        lammpsArgs.push_back("-pk");
        lammpsArgs.push_back("gpu");
        lammpsArgs.push_back("1"); // can use only one GPU without MPI
        lammpsArgs.push_back("omp");
        lammpsArgs.push_back(std::to_string(nthreads));
        lammpsArgs.push_back("neigh");
        if (settings.value(Keys::GPUNEIGH, true).toBool())
            lammpsArgs.push_back("yes");
        else
            lammpsArgs.push_back("no");
        lammpsArgs.push_back("pair/only");
        if (settings.value(Keys::GPUPAIRONLY, false).toBool())
            lammpsArgs.push_back("on");
        else
            lammpsArgs.push_back("off");
    } else if (accel == AcceleratorTab::Kokkos) {
        lammpsArgs.push_back("-kokkos");
        lammpsArgs.push_back("on");
        lammpsArgs.push_back("t");
        lammpsArgs.push_back(std::to_string(nthreads));
        lammpsArgs.push_back("-suffix");
        lammpsArgs.push_back("kk");
    }
}

void LammpsGui::startLammps()
{
    // temporarily extend lammpsArgs with additional arguments
    int initial_narg = lammpsArgs.size();
    QSettings settings;
    int accel = settings.value(Keys::ACCELERATOR, AcceleratorTab::None).toInt();
    // if non-threaded accelerator selected reset threads
    if ((accel == AcceleratorTab::None) || (accel == AcceleratorTab::Opt)) {
        nthreads = 1;
    }
    qputenv("OMP_NUM_THREADS", QByteArray::number(nthreads));

    appendAcceleratorArgs(accel, settings);

    if (settings.value(Keys::ECHO, false).toBool()) {
        lammpsArgs.push_back("-echo");
        lammpsArgs.push_back("screen");
    }
    if (settings.value(Keys::CITE, false).toBool()) {
        lammpsArgs.push_back("-cite");
        lammpsArgs.push_back("screen");
    }

    // Build temporary char* array for the LAMMPS C API which takes char**
    // but does not modify the argument strings. The const_cast is safe here
    // because lammps.open() only reads the strings to copy them internally.
    std::vector<char *> cargs;
    cargs.reserve(lammpsArgs.size());
    for (auto &s : lammpsArgs)
        cargs.push_back(const_cast<char *>(s.c_str()));
    int narg = static_cast<int>(cargs.size());
    lammps.open(narg, cargs.data());
    lammpsstatus->show();

    if (lammps.version() < Cfg::MIN_LAMMPS_VERSION) {
        critical(this, "LAMMPS-GUI Error", "Incompatible LAMMPS Version:",
                 "LAMMPS-GUI version " LAMMPS_GUI_VERSION " requires\n"
                 "a LAMMPS version of at least " +
                     Cfg::MIN_LAMMPS_VERSION_STR);
        exit(1);
    }

    // remove additional arguments (3 were there initially)
    lammpsArgs.resize(initial_narg);

    const QString errmsg = lammps.lastErrorMessage();
    if (!errmsg.isEmpty()) critical(this, "LAMMPS-GUI Error", "Error launching LAMMPS:", errmsg);
}

void LammpsGui::populateSyntax()
{
    // without a LAMMPS instance the registry stays unpopulated, which keeps
    // the unknown-name marking of the highlighter disabled
    if (!lammps.isOpen()) return;

    // command names: input script commands from the bundled list plus the
    // registered command styles of the running LAMMPS instance
    QStringList names;
    QFile internal_commands(QStringLiteral(":/lammps_internal_commands.txt"));
    if (internal_commands.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!internal_commands.atEnd())
            names << QString(internal_commands.readLine()).trimmed();
        internal_commands.close();
    }
    const int ncmds = lammps.styleCount("command");
    for (int i = 0; i < ncmds; ++i) {
        const QString style = lammps.styleName("command", i);
        if (!style.isEmpty()) names << style;
    }
    syntax.setCommands(names);

    static const struct {
        const char *name;
        StyleCat cat;
    } categories[] = {{"fix", StyleCat::Fix},           {"compute", StyleCat::Compute},
                      {"dump", StyleCat::Dump},         {"atom", StyleCat::Atom},
                      {"pair", StyleCat::Pair},         {"bond", StyleCat::Bond},
                      {"angle", StyleCat::Angle},       {"dihedral", StyleCat::Dihedral},
                      {"improper", StyleCat::Improper}, {"kspace", StyleCat::Kspace},
                      {"region", StyleCat::Region},     {"integrate", StyleCat::Integrate},
                      {"minimize", StyleCat::Minimize}};
    for (const auto &category : categories) {
        names.clear();
        const int nstyles = lammps.styleCount(category.name);
        for (int i = 0; i < nstyles; ++i) {
            const QString style = lammps.styleName(category.name, i);
            if (!style.isEmpty()) names << style;
        }
        syntax.setStyles(category.cat, names);
    }

    // re-highlight with the now complete syntax data
    highlighter->rehighlight();
}

bool LammpsGui::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close) {
        quit(); // quit() runs autoSave() itself
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void LammpsGui::openTutorialWebpage(int collection, int tutno)
{
    const auto &coll = tutorialCollection(collection);
    QString weburl   = coll.siteUrl;
    if (!coll.webUrl.isEmpty() && (tutno >= 1) && (tutno <= coll.slugs.size()))
        weburl = coll.webUrl.arg(tutno).arg(coll.slugs.value(tutno - 1));
    if (!weburl.isEmpty()) QDesktopServices::openUrl(QUrl(weburl));
}

namespace {

// deliberately does not speculate about the cause of the failure: users tend to
// take such hints literally and then chase a problem they do not have
void tutorialDownloadFailed(QWidget *parent, const QString &detail)
{
    critical(parent, "LAMMPS-GUI Error",
             "<p>Download of the tutorial files over the network is currently failing. "
             "Please try again in a while.</p>"
             "<p>If the problem persists, please report it in the LAMMPS forum at "
             "<a href=\"https://matsci.org/lammps\">https://matsci.org/lammps</a> or by email "
             "to developers@lammps.org.</p>",
             detail);
}

// list the tutorial files that could not be downloaded and suggest reporting
// them; a button opens the issue tracker of the collection's file repository
// in the web browser, so the long URL does not need to be shown
void tutorialFilesMissing(QWidget *parent, const QString &issuesUrl, const QStringList &missing)
{
    const QString plural = (missing.size() > 1) ? "s" : "";
    QString files;
    for (const auto &file : missing)
        files += QString("<br><code>%1</code>").arg(file);

    QMessageBox mb(parent);
    mb.setWindowTitle("LAMMPS-GUI Warning");
    mb.setText(QString("<p>The following tutorial file%1 could not be downloaded:%2</p>")
                   .arg(plural, files));
    mb.setInformativeText(
        QString("<p>Please report the missing file%1 by opening an issue in the tutorial's "
                "file repository on GitHub or by sending an email to akohlmey@gmail.com.</p>")
            .arg(plural));
    setDialogIcons(mb, ":/icons/warning.svg");
    auto *report = mb.addButton("&Report Issue...", QMessageBox::ActionRole);
    report->setIcon(QIcon(":/icons/help-browser.svg"));
    mb.exec();
    if (mb.clickedButton() == report) QDesktopServices::openUrl(QUrl(issuesUrl));
}

} // namespace

bool LammpsGui::downloadTutorialFiles(const QString &dir, const QList<DownloadItem> &downloads,
                                      URLDownloader &downloader, const QString &baseUrl,
                                      DownloadProgress &dlg, const QString &issuesUrl)
{
    int i         = 0;
    const int num = downloads.size();
    QStringList missing;

    for (const auto &item : downloads) {
        ++i;
        dlg.setProgress(QString("File %1 of %2: %3").arg(i).arg(num).arg(item.fname), i, num);

        QString localPath = dir + QDir::separator() + item.fname;
        if (!downloader.download(baseUrl.arg(item.ntutorial).arg(item.fname), localPath)) {
            // only a download canceled by the user aborts the batch.  accept(),
            // not close(): closing implies reject() and would re-trigger the
            // caller's cancel connection
            if (downloader.wasAborted()) {
                dlg.accept();
                return false;
            }
            // otherwise record the file and continue with the remaining ones:
            // a single file missing from the server (e.g. from a stale manifest
            // entry) should not discard the rest of the tutorial
            missing.append(item.fname);
            continue;
        }

        // check if download is a placeholder for a symbolic link and make a copy instead.
        QFile dlfile(localPath);
        QFileInfo dlpath(localPath);
        if (dlfile.open(QIODevice::ReadOnly)) {
            QString line = QString::fromLocal8Bit(dlfile.readLine());
            line         = line.trimmed();
            dlfile.close();

            if (line == QString("../") + dlpath.fileName()) {
                // the file is a symbolic link placeholder: copy the referenced file instead
                QString srcFile = dir + QDir::separator() + dlpath.fileName();
                QFile::remove(localPath);
                QFile::copy(srcFile, localPath);
            }
        }
    }
    progress->setValue(Cfg::PROGRESS_MAXIMUM);
    status->setText(Cfg::STATUS_READY);
    progress->hide();
    dirstatus->show();
    status->repaint();

    if (!missing.isEmpty()) {
        dlg.accept();
        tutorialFilesMissing(this, issuesUrl, missing);
    }
    return true;
}

void LammpsGui::setupTutorial(int collection, int tutno, const QString &dir, bool purgedir,
                              bool getsolution, bool openwebpage)
{
    const auto &coll = tutorialCollection(collection);
    if (coll.filesUrl.isEmpty()) {
        critical(this, "LAMMPS-GUI Error", "Tutorial files are not available:",
                 QString("The \"%1\" tutorial collection is not yet published for download.")
                     .arg(coll.name));
        return;
    }
    const QString baseUrl = coll.filesUrl;

    QDir directory(dir);
    directory.cd(dir);

    // open web page of the corresponding online tutorial
    if (openwebpage) openTutorialWebpage(collection, tutno);

    if (purgedir) purgeDirectory(dir);
    if (getsolution && !directory.mkpath("solution"))
        warning(this, "LAMMPS-GUI Warning",
                "Could not create the \"solution\" subdirectory for the tutorial files.");

    URLDownloader downloader(this);

    // splash-style progress dialog, visible from the very first network round
    // trip: that request may stall until the transfer timeout and previously
    // had no feedback at all, making the download look like a silent no-op
    DownloadProgress dlg(QString("Downloading %1 Tutorial %2").arg(coll.name).arg(tutno),
                         QPixmap(coll.logoFor(tutno)), this);
    connect(&dlg, &QDialog::rejected, &dlg, [&downloader]() {
        downloader.abort();
    });
    dlg.setBusy("Retrieving the list of tutorial files ...");

    // download and process manifest for selected tutorial
    // must check for error after download, e.g. when there is no network.
    QString manifestPath = dir + QDir::separator() + ".manifest";
    if (!downloader.download(baseUrl.arg(tutno).arg(".manifest"), manifestPath)) {
        // accept(), not close(): closing implies reject() and would trigger
        // the cancel connection above, masking the failure as a cancellation
        dlg.accept();
        // no error dialog when the user canceled the download
        if (!downloader.wasAborted()) tutorialDownloadFailed(this, downloader.errorString());
        return;
    }

    QFile manifest(manifestPath);
    QString line, first;

    QList<DownloadItem> downloads;
    if (manifest.open(QIODevice::ReadOnly)) {
        while (!manifest.atEnd()) {
            line = QString::fromLocal8Bit(manifest.readLine());
            line = line.trimmed();

            // skip empty and comment lines
            if (line.isEmpty() || line.startsWith('#')) continue;

            // file in subfolder
            if (line.contains('/')) {
                if (getsolution && line.startsWith("solution")) {
                    downloads.append(DownloadItem(tutno, line));
                }
            } else {
                // first file is the initial template
                if (first.isEmpty()) first = line;
                downloads.append(DownloadItem(tutno, line));
            }
        }
        manifest.close();
        manifest.remove();
    }

    if (!downloadTutorialFiles(dir, downloads, downloader, baseUrl, dlg,
                               coll.filesRepoUrl + "/issues"))
        return;
    dlg.accept();

    // the initial template may itself be among the files that failed to download
    const QString firstFile = dir + QDir::separator() + first;
    if (!first.isEmpty() && QFileInfo::exists(firstFile)) openFile(firstFile);
}

// Local Variables:
// c-basic-offset: 4
// End:
