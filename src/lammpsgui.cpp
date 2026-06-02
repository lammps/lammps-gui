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
#include "fileviewer.h"
#include "findandreplace.h"
#include "helpers.h"
#include "highlighter.h"
#include "imageviewer.h"
#include "lammpsrunner.h"
#include "logwindow.h"
#include "preferences.h"
#include "setvariables.h"
#include "slideshow.h"
#include "stdcapture.h"
#include "tutorialwizard.h"
#include "urldownloader.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDataStream>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
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
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <process.h>
#define execl(exe, arg0, arg1) _execl(exe, arg0, arg1)
#else
#include <unistd.h>
#endif

#include "constants.h"

namespace {
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
    setWindowIcon(QIcon(GuiConstants::MAIN_ICON));

    // set up central widget
    textEdit = new CodeEditor(this);
    textEdit->setEnabled(true);
    textEdit->setAcceptDrops(true);
    textEdit->setStyleSheet(bannerstyle);
    textEdit->setMinimumSize(GuiConstants::MINIMUM_WIDTH, GuiConstants::MINIMUM_HEIGHT);

    // set up menu bar and menus with their actions and shortcuts
    menubar = new QMenuBar(this);
    createFileMenu();
    createEditMenu();
    createRunMenu();
    createViewMenu();
    createTutorialMenu();
    createAboutMenu();
    setMenuBar(menubar);

    // Status bar
    createStatusBar();

    // document settings
    auto *document = textEdit->document();
    document->setPlainText(citeme);
    document->setModified(false);
    highlighter = new Highlighter(document);
    connect(document, &QTextDocument::modificationChanged, this, &LammpsGui::modified);

    // apply font settings
    setFont(allFont);
    textEdit->setFont(monoFont);
    document->setDefaultFont(monoFont);
    setCentralWidget(textEdit);

    // set width and height of main window
    // use default so the background logo is fully shown
    // use last values unless overridden from command-line
    // do not accept a geometry smaller than minimum, revert to default instead
    if (mainx < GuiConstants::MINIMUM_WIDTH) mainx = settings.value("mainx", 1024).toInt();
    if (mainy < GuiConstants::MINIMUM_HEIGHT) mainy = settings.value("mainy", 512).toInt();
    resize(mainx, mainy);

    varwindow = new QLabel(QString());
    varwindow->setWindowTitle(QString("LAMMPS-GUI - Current Variables"));
    varwindow->setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
    varwindow->setMinimumSize(100, 50);
    varwindow->setText("(none)");
    varwindow->setFont(monoFont);
    varwindow->setFrameStyle(QFrame::Sunken);
    varwindow->setFrameShape(QFrame::Panel);
    varwindow->setAlignment(Qt::AlignVCenter);
    varwindow->setContentsMargins(5, 5, 5, 5);
    varwindow->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    varwindow->hide();

    // set window flags for window manager
    auto flags = windowFlags();
    flags &= ~Qt::Dialog;
    flags |= Qt::CustomizeWindowHint;
    flags |= Qt::WindowMinimizeButtonHint;
    flags |= Qt::WindowMaximizeButtonHint;
    setWindowFlags(flags);
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
    auto *menu = menubar->addMenu("&File");
    addMenuAction(menu, ":/icons/document-new.png", "&New", "Ctrl+N", &LammpsGui::newDocument);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/document-open.png", "&Open", "Ctrl+O", &LammpsGui::open);
    addMenuAction(menu, ":/icons/document-open.png", "&View", "Ctrl+Shift+F", &LammpsGui::view);
    addMenuAction(menu, ":/icons/document-open.png", "Inspect &Restart", "Ctrl+Shift+R",
                  &LammpsGui::inspect);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/document-save.png", "&Save", "Ctrl+S", &LammpsGui::save);
    addMenuAction(menu, ":/icons/document-save-as.png", "Save &As", "Ctrl+Shift+S",
                  &LammpsGui::saveAs);
    menu->addSeparator();

    recentActions.resize(GuiConstants::NUM_RECENT_FILES);
    for (int i = 0; i < GuiConstants::NUM_RECENT_FILES; ++i) {
        recentActions[i] = addMenuAction(menu, ":/icons/document-open-recent.png",
                                         QString("&%1.").arg(i + 1), "", &LammpsGui::openRecent);
    }
    menu->addSeparator();

    addMenuAction(menu, ":/icons/application-exit.png", "&Quit", "Ctrl+Q", &LammpsGui::quit);
}

void LammpsGui::createEditMenu()
{
    auto *menu = menubar->addMenu("&Edit");
    addMenuAction(menu, ":/icons/edit-undo.png", "&Undo", "Ctrl+Z", &LammpsGui::undo);
    addMenuAction(menu, ":/icons/edit-redo.png", "&Redo", "Ctrl+Shift+Z", &LammpsGui::redo);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/edit-copy.png", "&Copy", "Ctrl+C", &LammpsGui::copy)
        ->setEnabled(hasClipboard);
    addMenuAction(menu, ":/icons/edit-cut.png", "Cu&t", "Ctrl+X", &LammpsGui::cut)
        ->setEnabled(hasClipboard);
    addMenuAction(menu, ":/icons/edit-paste.png", "&Paste", "Ctrl+V", &LammpsGui::paste)
        ->setEnabled(hasClipboard);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/search.png", "&Find and Replace...", "Ctrl+F",
                  &LammpsGui::findAndReplace);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/preferences-desktop.png", "P&references...", "Ctrl+P",
                  &LammpsGui::preferences);
    addMenuAction(menu, ":/icons/document-revert.png", "Reset Preferences to &Defaults", "",
                  &LammpsGui::defaults);
}

void LammpsGui::createRunMenu()
{
    auto *menu = menubar->addMenu("&Run");
    addMenuAction(menu, ":/icons/system-run.png", "&Run LAMMPS from Editor Buffer", "Ctrl+Return",
                  &LammpsGui::runBuffer);
    addMenuAction(menu, ":/icons/run-file.png", "Run LAMMPS from &File", "Ctrl+Shift+Return",
                  &LammpsGui::runFile);
    addMenuAction(menu, ":/icons/process-stop.png", "&Stop LAMMPS", "Ctrl+/", &LammpsGui::stopRun);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/system-restart.png", "Relaunch &LAMMPS Instance", "",
                  &LammpsGui::restartLammps);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/preferences-desktop-personal.png", "Set &Variables...",
                  "Ctrl+Shift+V", &LammpsGui::editVariables);
    menu->addSeparator();

    addMenuAction(menu, ":/icons/emblem-photos.png", "Create &Image", "Ctrl+I",
                  &LammpsGui::renderImage);
    menu->addSeparator();

    auto *ovito = addMenuAction(menu, ":/icons/ovito.png", "View in &OVITO", "Ctrl+Shift+O",
                                &LammpsGui::startExe);
    ovito->setEnabled(hasExe("ovito"));
    ovito->setData("ovito");

    auto *vmd = addMenuAction(menu, ":/icons/vmd.png", "View in VM&D", "Ctrl+Shift+D",
                              &LammpsGui::startExe);
    vmd->setEnabled(hasExe("vmd"));
    vmd->setData("vmd");
}

void LammpsGui::createViewMenu()
{
    auto *menu = menubar->addMenu("&View");
    addMenuAction(menu, ":/icons/utilities-terminal.png", "&Output Window", "Ctrl+Shift+L",
                  &LammpsGui::viewLog);
    addMenuAction(menu, ":/icons/x-office-drawing.png", "&Charts Window", "Ctrl+Shift+C",
                  &LammpsGui::viewChart);
    addMenuAction(menu, ":/icons/emblem-photos.png", "&Image Window", "Ctrl+Shift+I",
                  &LammpsGui::viewImage);
    addMenuAction(menu, ":/icons/image-x-generic.png", "&Slide Show Window", "Ctrl+L",
                  &LammpsGui::viewSlides);
    addMenuAction(menu, ":/icons/preferences-desktop-personal.png", "&Variables Window",
                  "Ctrl+Shift+W", &LammpsGui::viewVariables);
}

void LammpsGui::createTutorialMenu()
{
    auto *menu = menubar->addMenu("&Tutorials");
    for (int i = 0; i < GuiConstants::NUM_TUTORIALS; ++i) {
        addMenuAction(menu, ":/icons/tutorial-logo.png",
                      QString("Start LAMMPS Tutorial &%1").arg(i + 1), "", [this, i]() {
                          startTutorial(i + 1);
                      });
    }
}

void LammpsGui::createAboutMenu()
{
    auto *menu = menubar->addMenu("&About");
    addMenuAction(menu, ":/icons/help-about.png", "&About LAMMPS-GUI", "Ctrl+Shift+A",
                  &LammpsGui::about);
    addMenuAction(menu, ":/icons/help-faq.png", "Quick &Help", "Ctrl+Shift+H", &LammpsGui::help);
    addMenuAction(menu, ":/icons/system-help.png", "LAMMPS-&GUI Documentation", "Ctrl+Shift+G",
                  &LammpsGui::howto);
    addMenuAction(menu, ":/icons/help-browser.png", "LAMMPS Online &Manual", "Ctrl+Shift+M",
                  &LammpsGui::manual);
    addMenuAction(menu, ":/icons/help-tutorial.png", "LAMMPS &Tutorial Website", "Ctrl+Shift+T",
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
    auto pix     = QPixmap(GuiConstants::LAMMPS_ICON);
    lammpsstatus->setPixmap(
        pix.scaled(GuiConstants::ICON_SCALE, GuiConstants::ICON_SCALE, Qt::KeepAspectRatio));
    lammpsstatus->setToolTip("LAMMPS instance is active");
    lammpsstatus->hide();
    statusbar->addWidget(lammpsstatus);

    auto *button = new QPushButton(QIcon(":/icons/document-save.png"), "");
    button->setToolTip("Save edit buffer to file");
    auto buttonhint = button->minimumSizeHint();
    buttonhint.setWidth(buttonhint.height() * 4 / 3);
    button->setMinimumSize(buttonhint);
    button->setMaximumSize(buttonhint);
    connect(button, &QPushButton::released, this, &LammpsGui::save);
    statusbar->addWidget(button);

    button = new QPushButton(QIcon(":/icons/system-run.png"), "");
    button->setToolTip("Run LAMMPS on input");
    button->setMinimumSize(buttonhint);
    button->setMaximumSize(buttonhint);
    connect(button, &QPushButton::released, this, &LammpsGui::runBuffer);
    statusbar->addWidget(button);

    button = new QPushButton(QIcon(":/icons/process-stop.png"), "");
    button->setToolTip("Stop LAMMPS");
    button->setMinimumSize(buttonhint);
    button->setMaximumSize(buttonhint);
    connect(button, &QPushButton::released, this, &LammpsGui::stopRun);
    statusbar->addWidget(button);

    button = new QPushButton(QIcon(":/icons/emblem-photos.png"), "");
    button->setToolTip("Create snapshot image");
    button->setMinimumSize(buttonhint);
    button->setMaximumSize(buttonhint);
    connect(button, &QPushButton::released, this, &LammpsGui::renderImage);
    statusbar->addWidget(button);

    cpuuse = new QLabel("   0%CPU");
    cpuuse->setFixedWidth(90);
    statusbar->addWidget(cpuuse);
    cpuuse->hide();

    status = new QLabel("Ready.");
    status->setFixedWidth(300);
    statusbar->addWidget(status);

    dirstatus = new QLabel(QString(" Directory: (unknown)"));
    dirstatus->setMinimumWidth(GuiConstants::MINIMUM_WIDTH);
    dirstatus->show();
    statusbar->addWidget(dirstatus);

    progress = new QProgressBar();
    progress->setRange(0, GuiConstants::PROGRESS_MAXIMUM);
    progress->setMinimumWidth(GuiConstants::MINIMUM_WIDTH);
    progress->hide();
    statusbar->addWidget(progress);
}

void LammpsGui::setupPlugin(QSettings &settings)
{
#if defined(LAMMPS_GUI_USE_PLUGIN)
    // first try to load from existing setting
    pluginPath = settings.value("plugin_path", "").toString();
    if (!pluginPath.isEmpty()) {
        // make canonical and try loading; reset to empty string if loading failed
        pluginPath = QFileInfo(pluginPath).canonicalFilePath();
        if (!lammps.loadLib(pluginPath)) {
            pluginPath.clear();
            // could not load successfully -> remove any existing setting.
            settings.remove("plugin_path");
        }
    }

    // set platform specific paths, library file name,config directory, and filename patterns
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
        // check some more system paths on Linux or unix-like systems
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
                settings.setValue("plugin_path", pluginPath);
                settings.sync();
                break;
            }
        }

        // No suitable plugin was found automatically.  Show a dialog with three choices:
        // 1) Download a pre-compiled shared library from the LAMMPS webserver
        // 2) Exit LAMMPS-GUI
        // 2) Browse the filesystem for a suitable shared library file
        while (pluginPath.isEmpty()) {
            // remove key for path to the plugin so we won't get stuck in a loop reading a bad file
            settings.remove("plugin_path");

            QMessageBox mb(this);
            mb.setWindowTitle("LAMMPS-GUI - No LAMMPS Shared Library");
            mb.setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
            mb.setIconPixmap(QPixmap(":/icons/lammps-plugin.png").scaled(96, 96));
            mb.setText("No suitable LAMMPS shared library found.");
            mb.setInformativeText(
                "<p align=\"justify\">Either the shared library path has been reset, the "
                "configured or default library file was not found, or the selected library failed "
                "to load.</p><p align=\"justify\">You may now either download a pre-compiled LAMMPS"
                " shared library file for your platform from the LAMMPS webserver, browse the "
                "filesystem for a suitable LAMMPS library file, or exit LAMMPS-GUI.</p>");

            auto *downloadBtn = mb.addButton("Download Library...", QMessageBox::ApplyRole);
            downloadBtn->setIcon(QIcon(":/icons/download-file.png"));
            auto *browseBtn = mb.addButton("Browse Filesystem...", QMessageBox::AcceptRole);
            browseBtn->setIcon(QIcon(":/icons/document-open.png"));
            auto *exitBtn = mb.addButton("Exit", QMessageBox::NoRole);
            exitBtn->setIcon(QIcon(":/icons/application-exit.png"));

            mb.setDefaultButton(downloadBtn);
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
                    settings.setValue("plugin_path", canonical);
                    settings.sync();
                    // must re-launch LAMMPS-GUI to cleanly load the selected new plugin
                    const auto path = QCoreApplication::applicationFilePath().toStdString();
                    const auto arg0 = QCoreApplication::arguments().at(0).toStdString();
                    execl(path.c_str(), arg0.c_str(), (char *)nullptr);
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
                auto dlUrl   = QString("https://download.lammps.org/lammps-gui/%1").arg(libName);

                URLDownloader downloader(this);
                if (downloader.download(dlUrl, libPath, true)) {
                    // try loading the downloaded library
                    if (lammps.loadLib(libPath)) {
                        pluginPath = libPath;
                        settings.setValue("plugin_path", pluginPath);
                        settings.sync();
                        // must re-launch LAMMPS-GUI to cleanly load the selected new plugin
                        const auto path = QCoreApplication::applicationFilePath().toStdString();
                        const auto arg0 = QCoreApplication::arguments().at(0).toStdString();
                        execl(path.c_str(), arg0.c_str(), (char *)nullptr);
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
#endif
}

void LammpsGui::setupAccelerators(QSettings &settings)
{
    // default accelerator package is OPENMP, but we switch the configured accelerator to
    // "none" if the selected package is not available to have an option that always works
    int accel = settings.value("accelerator", AcceleratorTab::OpenMP).toInt();
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
    settings.setValue("accelerator", accel);

    // Check and initialize some settings for individual accelerator packages and commit
    // GPU neighbor list on GPU versus host
    bool gpuneigh = settings.value("gpuneigh", true).toBool();
    settings.setValue("gpuneigh", gpuneigh);
    // accelerate only pair style (i.e. run PPPM completely on host)
    bool gpupaironly = settings.value("gpupaironly", false).toBool();
    settings.setValue("gpupaironly", gpupaironly);
    // INTEL package precision
    int intelprec = settings.value("intelprec", AcceleratorTab::Mixed).toInt();
    settings.setValue("intelprec", intelprec);

    // Check and initialize nthreads setting for when OpenMP support is compiled in.
    // Default is to use OMP_NUM_THREADS setting, if that is not available, thenhalf of max
    // (assuming hyper-threading is enabled) and no more than GuiConstants::MAX_DEFAULT_THREADS
    // (=16). This is only if there is no preference set but do not override OMP_NUM_THREADS
    int default_threads =
        std::min(QThread::idealThreadCount() / 2, GuiConstants::MAX_DEFAULT_THREADS);
    default_threads = std::max(default_threads, 1);
    if (qEnvironmentVariableIsSet("OMP_NUM_THREADS"))
        default_threads = qEnvironmentVariable("OMP_NUM_THREADS").toInt();
    nthreads = settings.value("nthreads", default_threads).toInt();

    // reset nthreads if accelerator does not support threads
    if ((accel == AcceleratorTab::Opt) || (accel == AcceleratorTab::None)) nthreads = 1;

    // set OMP_NUM_THREADS environment variable, if not set
    if (!qEnvironmentVariableIsSet("OMP_NUM_THREADS"))
        qputenv("OMP_NUM_THREADS", QByteArray::number(nthreads));
}

/* -------------------------------------------------------------------- */

LammpsGui::LammpsGui(QWidget *parent, const QString &filename, int width, int height) :
    QMainWindow(parent), textEdit(nullptr), menubar(nullptr), highlighter(nullptr),
    capturer(new StdCapture), status(nullptr), cpuuse(nullptr), logwindow(nullptr),
    imagewindow(nullptr), chartwindow(nullptr), slideshow(nullptr), logupdater(nullptr),
    dirstatus(nullptr), progress(nullptr), prefdialog(nullptr), lammpsstatus(nullptr),
    varwindow(nullptr), wizard(nullptr), runner(nullptr), isRunning(false), runCounter(0),
    nthreads(1), mainx(width), mainy(height)
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
    allFont.setFamily(settings.value("allfamily", allInfo.family()).toString());
    allFont.setPointSize(settings.value("allsize", allInfo.pointSize()).toInt());
    allFont.setStyleHint(GUI_ALLFONT->styleHint());
    settings.setValue("allfamily", allFont.family());
    settings.setValue("allsize", allFont.pointSize());

    QFont monoFont;
    QFontInfo monoInfo(*GUI_MONOFONT);
    monoFont.setFamily(settings.value("monofamily", monoInfo.family()).toString());
    monoFont.setPointSize(settings.value("monosize", monoInfo.pointSize()).toInt());
    monoFont.setStyleHint(GUI_MONOFONT->styleHint());
    monoFont.setFixedPitch(true);
    settings.setValue("monofamily", monoFont.family());
    settings.setValue("monosize", monoFont.pointSize());

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

    inspectList.clear();
    setAutoFillBackground(true);

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
        setWindowTitle("LAMMPS-GUI - Editor - *unknown*");
    }

    // start LAMMPS and initialize command completion
    startLammps();
    QStringList style_list;
    char buf[GuiConstants::DEFAULT_BUFLEN];
    QFile internal_commands(":/lammps_internal_commands.txt");
    if (internal_commands.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!internal_commands.atEnd()) {
            style_list << QString(internal_commands.readLine()).trimmed();
        }
    }
    internal_commands.close();
    int ncmds = lammps.styleCount("command");
    for (int i = 0; i < ncmds; ++i) {
        if (lammps.styleName("command", i, buf, GuiConstants::DEFAULT_BUFLEN)) {
            // skip suffixed names
            const QString style(buf);
            if (style.endsWith("/kk/host") || style.endsWith("/kk/device") || style.endsWith("/kk"))
                continue;
            style_list << style;
        }
    }
    style_list.sort();
    textEdit->setCommandList(style_list);

    style_list.clear();
    const char *varstyles[] = {"delete",   "atomfile", "file",   "format", "getenv", "index",
                               "internal", "loop",     "python", "string", "timer",  "uloop",
                               "universe", "world",    "equal",  "vector", "atom"};
    for (const auto *const var : varstyles)
        style_list << var;
    style_list.sort();
    textEdit->setVariableList(style_list);

    style_list.clear();
    const char *unitstyles[] = {"lj", "real", "metal", "si", "cgs", "electron", "micro", "nano"};
    for (const auto *const unit : unitstyles)
        style_list << unit;
    style_list.sort();
    textEdit->setUnitsList(style_list);

    style_list.clear();
    const char *extraargs[] = {"extra/atom/types",        "extra/bond/types",
                               "extra/angle/types",       "extra/dihedral/types",
                               "extra/improper/types",    "extra/bond/per/atom",
                               "extra/angle/per/atom",    "extra/dihedral/per/atom",
                               "extra/improper/per/atom", "extra/special/per/atom"};
    for (const auto *const extra : extraargs)
        style_list << extra;
    textEdit->setExtraList(style_list);

    textEdit->setFileList();

#define ADD_STYLES(keyword, Type)                                                              \
    style_list.clear();                                                                        \
    if ((std::string(#keyword) == "pair") || (std::string(#keyword) == "bond") ||              \
        (std::string(#keyword) == "angle") || (std::string(#keyword) == "dihedral") ||         \
        (std::string(#keyword) == "improper") || (std::string(#keyword) == "kspace"))          \
        style_list << QString("none");                                                         \
    ncmds = lammps.styleCount(#keyword);                                                       \
    for (int i = 0; i < ncmds; ++i) {                                                          \
        if (lammps.styleName(#keyword, i, buf, GuiConstants::DEFAULT_BUFLEN)) {                \
            const QString style(buf);                                                          \
            if (style.endsWith("/gpu") || style.endsWith("/intel") || style.endsWith("/kk") || \
                style.endsWith("/kk/device") || style.endsWith("/kk/host") ||                  \
                style.endsWith("/omp") || style.endsWith("/opt"))                              \
                continue;                                                                      \
            style_list << style;                                                               \
        }                                                                                      \
    }                                                                                          \
    style_list.sort();                                                                         \
    textEdit->set##Type##List(style_list)

    ADD_STYLES(fix, Fix);
    ADD_STYLES(compute, Compute);
    ADD_STYLES(dump, Dump);
    ADD_STYLES(atom, Atom);
    ADD_STYLES(pair, Pair);
    ADD_STYLES(bond, Bond);
    ADD_STYLES(angle, Angle);
    ADD_STYLES(dihedral, Dihedral);
    ADD_STYLES(improper, Improper);
    ADD_STYLES(kspace, Kspace);
    ADD_STYLES(region, Region);
    ADD_STYLES(integrate, Integrate);
    ADD_STYLES(minimize, Minimize);
#undef ADD_STYLES

    settings.beginGroup("reformat");
    textEdit->setReformatOnReturn(settings.value("return", false).toBool());
    textEdit->setAutoComplete(settings.value("automatic", true).toBool());
    settings.endGroup();

    // apply https proxy setting: prefer environment variable or fall back to preferences value
    auto https_proxy = QString::fromLocal8Bit(qgetenv("https_proxy"));
    if (https_proxy.isEmpty()) https_proxy = settings.value("https_proxy", "").toString();
    if (!https_proxy.isEmpty()) lammps.command(QString("shell putenv https_proxy=") + https_proxy);

    // finally show the window
    showNormal();
}

LammpsGui::~LammpsGui()
{
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
}

void LammpsGui::newDocument()
{
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
    setWindowTitle("LAMMPS-GUI - Editor - *unknown*");
    runCounter = 0;
}

void LammpsGui::open()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open the file");
    openFile(fileName);
}

void LammpsGui::view()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open the file");
    viewFile(fileName);
}

void LammpsGui::inspect()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open the restart file");
    inspectFile(fileName);
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
                    vmd->start(exe, args);
                } else {
                    warning(this, "LAMMPS-GUI Error",
                            "Cannot create temporary file for loading system in VMD",
                            vmdfile.errorString());
                }
            }
            if (exe == "ovito") {
                QStringList args;
                args << datafile.fileName();
                {
                    StdoutSilencer guard;
                    lammps.command(datacmd);
                }
                auto *ovito = new QProcess(this);
                ovito->start(exe, args);
            }
        } else {
            // launch program without arguments when no system exists (yet)
            auto *proc = new QProcess(this);
            proc->start(exe, args);
        }
    }
}

void LammpsGui::updateRecents(const QString &filename)
{
    QSettings settings;
    if (settings.contains("recent")) recent = settings.value("recent").value<QList<QString>>();

    for (int i = 0; i < recent.size(); ++i) {
        QFileInfo fi(recent[i]);
        if (!fi.isReadable()) {
            recent.removeAt(i);
            i = 0;
        }
    }

    if (!filename.isEmpty() && !recent.contains(filename)) recent.prepend(filename);
    if (recent.size() > 5) recent.removeLast();
    if (!recent.empty())
        settings.setValue("recent", QVariant::fromValue(recent));
    else
        settings.remove("recent");

    for (int i = 0; i < GuiConstants::NUM_RECENT_FILES; ++i) {
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
    char buffer[GuiConstants::DEFAULT_BUFLEN];

    // delete from back so they are not re-indexed
    for (int i = nvar - 1; i >= 0; --i) {
        memset(buffer, 0, GuiConstants::DEFAULT_BUFLEN);
        if (lammps.idName("variable", i, buffer, GuiConstants::DEFAULT_BUFLEN))
            lammps.command(QString("variable %1 delete").arg(buffer));
    }
}

void LammpsGui::updateVariables()
{
    const auto doc = textEdit->toPlainText().replace('\t', ' ').split('\n');
    QStringList known;
    QRegularExpression indexvar(R"(^\s*variable\s+(\w+)\s+index\s+(.*))");
    QRegularExpression anyvar(R"(^\s*variable\s+(\w+)\s+(\w+)\s+(.*))");
    QRegularExpression usevar(R"((\$(\w)|\${(\w+)}))");
    QRegularExpression refvar(R"(v_(\w+))");

    // forget previously listed variables
    variables.clear();

    for (const auto &line : doc) {

        if (line.isEmpty()) continue;

        // first find variable definitions.
        // index variables are special since they can be overridden from the command line
        auto index = indexvar.match(line);
        auto any   = anyvar.match(line);

        if (index.hasMatch()) {
            if (index.lastCapturedIndex() >= 2) {
                auto name = index.captured(1);
                if (!known.contains(name)) {
                    variables.append(qMakePair(name, index.captured(2)));
                    known.append(name);
                }
            }
        } else if (any.hasMatch()) {
            if (any.lastCapturedIndex() >= 3) {
                auto name = any.captured(1);
                if (!known.contains(name)) known.append(name);
            }
        }

        // now split line into words and search for use of undefined variables
        auto words = line.split(' ', Qt::SkipEmptyParts);
        for (const auto &word : words) {
            auto use = usevar.match(word);
            auto ref = refvar.match(word);
            if (use.hasMatch()) {
                auto name = use.captured(use.lastCapturedIndex());
                if (!known.contains(name)) {
                    known.append(name);
                    variables.append(qMakePair(name, QString()));
                }
            }
            if (ref.hasMatch()) {
                auto name = ref.captured(use.lastCapturedIndex());
                if (!known.contains(name)) known.append(name);
            }
        }
    }
}

// open file and switch CWD to path of file
void LammpsGui::openFile(const QString &fileName)
{
    // do nothing, if no file name provided
    if (fileName.isEmpty()) return;

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
                break;
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
    setWindowTitle(QString("LAMMPS-GUI - Editor - " + currentFile));
    runCounter = 0;
    textEdit->document()->setModified(false);
    textEdit->setGroupList();
    textEdit->setVarNameList();
    textEdit->setComputeIDList();
    textEdit->setFixIDList();
    textEdit->setFileList();
    dirstatus->setText(QString(" Directory: ") + currentDir);
    status->setText(GuiConstants::STATUS_READY);
    cpuuse->hide();

    updateVariables();
}

// open file in read-only mode for viewing in separate window
void LammpsGui::viewFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QFile::Text)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot open file " + fileName + ":",
                file.errorString());
    } else {
        file.close();
        auto *viewer = new FileViewer(fileName, this);
        viewer->show();
    }
}

void LammpsGui::purgeInspectList()
{
    for (auto *item : inspectList) {
        if (item->info) {
            if (!item->info->isVisible()) {
                delete item->info;
                item->info = nullptr;
            }
        }
        if (item->data) {
            if (!item->data->isVisible()) {
                delete item->data;
                item->data = nullptr;
            }
        }
        if (item->image) {
            if (!item->image->isVisible()) {
                delete item->image;
                item->image = nullptr;
            }
        }
        if (!item->image && !item->data && !item->info) inspectList.removeOne(item);
    }
}

// read restart file into LAMMPS instance and launch image viewer
void LammpsGui::inspectFile(const QString &fileName)
{
    QFile file(fileName);
    auto shortName = QFileInfo(fileName).fileName();

    purgeInspectList();
    auto *ilist  = new InspectData;
    ilist->info  = nullptr;
    ilist->data  = nullptr;
    ilist->image = nullptr;
    inspectList.append(ilist);

    if (file.size() > 262144000L) {
        QMessageBox mb;
        mb.setWindowTitle("  Warning:  Large Restart File  ");
        mb.setWindowIcon(windowIcon());
        mb.setText(QString("<center>The restart file ") + shortName + " is large</center>");
        QString details = "Inspecting the restart file %1 with LAMMPS-GUI may need an additional "
                          "%2 GB of free RAM (or more) to proceed";
        mb.setDetailedText(details.arg(shortName).arg(file.size() / 134217728.0));
        mb.setInformativeText("Do you want to continue?");
        mb.setIcon(QMessageBox::Question);
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setDefaultButton(QMessageBox::No);
        mb.setEscapeButton(QMessageBox::No);
        mb.setFont(font());

        auto *button = mb.button(QMessageBox::Yes);
        button->setIcon(QIcon(":/icons/dialog-ok.png"));
        button = mb.button(QMessageBox::No);
        button->setIcon(QIcon(":/icons/dialog-no.png"));

        int rv = mb.exec();
        switch (rv) {
            case QMessageBox::No:
                return;
                break;
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

    char magic[16] = "               ";
    QDataStream in(&file);
    in.readRawData(magic, 16);
    file.close();
    if (strcmp(magic, LAMMPS_MAGIC) != 0) {
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
            infoviewer->show();
            ilist->info = infoviewer;
            dumpinfo.remove();
            {
                StdoutSilencer guard;
                lammps.command(QString("write_data %1 pair ij noinit").arg(infodata));
            }
            auto *dataviewer = new FileViewer(
                infodata, this, QString("LAMMPS-GUI: data file for %1").arg(shortName));
            dataviewer->show();
            ilist->data = dataviewer;
            QFile(infodata).remove();
            auto *inspect_image = new ImageViewer(fileName, &lammps, this);
            inspect_image->setFont(font());
            inspect_image->setMinimumSize(GuiConstants::MINIMUM_WIDTH,
                                          GuiConstants::MINIMUM_HEIGHT);
            inspect_image->show();
            ilist->image = inspect_image;
        }
    }
}

// write file and update CWD to its folder

void LammpsGui::writeFile(const QString &fileName)
{
    QFileInfo path(fileName);
    currentFile = path.fileName();
    currentDir  = path.absolutePath();
    QFile file(path.absoluteFilePath());

    if (!file.open(QIODevice::WriteOnly | QFile::Text)) {
        warning(this, "LAMMPS-GUI Warning", "Cannot save to file " + fileName + ":",
                file.errorString());
        return;
    }
    setWindowTitle(QString("LAMMPS-GUI - Editor - " + currentFile));
    QDir::setCurrent(currentDir);

    updateRecents(path.absoluteFilePath());

    QTextStream out(&file);
    QString text = textEdit->toPlainText();
    out << text;
    if (text.back().toLatin1() != '\n') out << "\n"; // add final newline if missing
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
    if (fileName.isEmpty()) fileName = QFileDialog::getSaveFileName(this, "Save");

    writeFile(fileName);
}

void LammpsGui::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save as");
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
                break;
            case QMessageBox::No: // fallthrough
            default:
                // do nothing
                break;
        }
    }

    // store some global settings
    QSettings settings;
    if (!isMaximized()) {
        settings.setValue("mainx", width());
        settings.setValue("mainy", height());
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
    double t_elapsed, t_remain, t_total;
    int completed = 1000;

    // estimate completion percentage
    if (lammps.isRunning()) {
        t_elapsed = lammps.getThermo("cpu");
        t_remain  = lammps.getThermo("cpuremain");
        t_total   = t_elapsed + t_remain + 1.0e-10;
        completed = t_elapsed / t_total * 1000.0;
        // update cpu usage
        int percent_cpu = (int)lammps.getThermo("cpuuse");
        // clear any pending error messages from polling those thermo keywords
        lammps.getLastErrorMessage(nullptr, 0);

        cpuuse->setText(QString("%1%CPU").arg(percent_cpu, 4));
        if (percent_cpu < 25.0 * nthreads) {
            cpuuse->setStyleSheet("QLabel {background-color: black; color: white;}");
        } else if (percent_cpu < 50.0 * nthreads) {
            cpuuse->setStyleSheet("QLabel {background-color: darkblue; color: white;}");
        } else if (percent_cpu > 100.0 * nthreads + 50.0) {
            cpuuse->setStyleSheet("QLabel {background-color: firebrick; color: white;}");
        } else if (percent_cpu < 100.0 * nthreads - 50.0) {
            cpuuse->setStyleSheet("QLabel {background-color: firebrick; color: white;}");
        } else if (percent_cpu > 100.0 * nthreads + 20.0) {
            cpuuse->setStyleSheet("QLabel {background-color: gold; color: black;}");
        } else if (percent_cpu < 100.0 * nthreads - 20.0) {
            cpuuse->setStyleSheet("QLabel {background-color: gold; color: black;}");
        } else {
            cpuuse->setStyleSheet("QLabel {background-color: forestgreen; color: white;}");
        }

        int nline = -1;
        void *ptr = lammps.lastThermo("line", 0);
        if (ptr) {
            nline = *((int *)ptr);
            textEdit->setHighlight(nline, false);
        }

        if (varwindow) {
            int nvar = lammps.idCount("variable");
            char buffer[GuiConstants::DEFAULT_BUFLEN];
            QString varinfo("\n");
            for (int i = 0; i < nvar; ++i) {
                memset(buffer, 0, GuiConstants::DEFAULT_BUFLEN);
                if (lammps.variableInfo(i, buffer, GuiConstants::DEFAULT_BUFLEN)) varinfo += buffer;
            }
            if (nvar == 0) varinfo += "  (none)  ";

            varwindow->setText(varinfo);
            varwindow->adjustSize();
        }
    }

    progress->setValue(completed);
    if (logwindow) {
        const auto text = capturer->getChunk();
        if (!text.empty()) {
            logwindow->moveCursor(QTextCursor::End);
            logwindow->insertPlainText(text.c_str());
            logwindow->moveCursor(QTextCursor::End);
            logwindow->textCursor().deleteChar();
        }
    }

    // get timestep
    int step  = 0;
    void *ptr = lammps.lastThermo("step", 0);
    if (ptr) {
        if (lammps.extractSetting("bigint") == 4)
            step = *(int *)ptr;
        else
            step = (int)*(int64_t *)ptr;
    }

    // extract cached thermo data when LAMMPS is executing a minimize or run command
    if (chartwindow && lammps.isRunning()) {
        // thermo data is not yet valid during setup
        void *ptr = lammps.lastThermo("setup", 0);
        if (ptr && *(int *)ptr) return;

        lammps.lastThermo("lock", 0);
        ptr = lammps.lastThermo("num", 0);
        if (ptr) {
            int ncols = *(int *)ptr;

            // check if the column assignment has changed
            // if yes, delete charts and start over
            if (chartwindow->numCharts() > 0) {
                int count     = 0;
                bool do_reset = false;
                if (step < chartwindow->getStep()) do_reset = true;
                for (int i = 0, idx = 0; i < ncols; ++i) {
                    QString label = (const char *)lammps.lastThermo("keyword", i);
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
                    QString label = (const char *)lammps.lastThermo("keyword", i);
                    // no need to store the timestep column
                    if (label == "Step") continue;
                    chartwindow->addChart(label, i);
                }
            }

            for (int i = 0; i < ncols; ++i) {
                int datatype = -1;
                double data  = 0.0;
                void *ptr    = lammps.lastThermo("type", i);
                if (ptr) datatype = *(int *)ptr;
                ptr = lammps.lastThermo("data", i);
                if (ptr) {
                    if (datatype == 0) // int
                        data = *(int *)ptr;
                    else if (datatype == 2) // double
                        data = *(double *)ptr;
                    else if (datatype == 4) // bigint
                        data = (double)*(int64_t *)ptr;
                }
                chartwindow->addData(step, data, i);
            }
        }
        lammps.lastThermo("unlock", 0);
    }

    // update list of available image file names

    QString imagefile = (const char *)lammps.lastThermo("imagename", 0);
    if (!imagefile.isEmpty()) {
        if (!slideshow) {
            slideshow = new SlideShow(currentFile, this);
            if (QSettings().value("viewslide", true).toBool())
                slideshow->show();
            else
                slideshow->hide();
        } else {
            slideshow->setWindowTitle(
                QString("LAMMPS-GUI - Slide Show - %1 - Run %2").arg(currentFile).arg(runCounter));
            if (QSettings().value("viewslide", true).toBool()) slideshow->show();
        }
        slideshow->addImage(imagefile);
    }
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

void LammpsGui::runDone()
{
    if (logupdater) {
        logupdater->stop();
        delete logupdater;
        logupdater = nullptr;
    }
    progress->setValue(GuiConstants::PROGRESS_MAXIMUM);
    textEdit->setHighlight(CodeEditor::NO_HIGHLIGHT, false);

    capturer->endCapture();

    if (logwindow) {
        auto log = capturer->getCapture();
        logwindow->insertPlainText(log.c_str());
        logwindow->moveCursor(QTextCursor::End);
    }

    // check stdout capture buffer utilization and print warning message if large

    double bufferuse = capturer->getBufferUse();
    if (bufferuse > GuiConstants::BUFFER_WARNING_THRESHOLD) {
        int thermo_val = lammps.extractSetting("thermo_every");
        int thermo_suggest =
            GuiConstants::THERMO_SUGGEST_MULTIPLIER * (int)round(bufferuse * thermo_val);
        int update_val     = QSettings().value("updfreq", 100).toInt();
        int update_suggest = std::max(1, update_val / 5);

        QString mesg1("<p align=\"justified\">The I/O buffer for capturing the LAMMPS screen "
                      "output was used by up to %1%.</p>"
                      "<p align=\"justified\"><b>This can slow down the simulation.</b></p>");
        QString mesg2("<p align=\"justified\">Please consider reducing the amount of output "
                      "to the screen, for example by increasing the thermo interval in the "
                      "input from %1 to %2, or reducing the data update interval in the "
                      "preferences from %3 to %4, or something similar.</p>");

        critical(this, "LAMMPS-GUI Warning: High I/O Buffer Usage",
                 mesg1.arg((int)(100.0 * bufferuse)),
                 mesg2.arg(thermo_val).arg(thermo_suggest).arg(update_val).arg(update_suggest));
    }

    if (chartwindow) {
        void *ptr = lammps.lastThermo("step", 0);
        if (ptr) {
            int step = 0;
            if (lammps.extractSetting("bigint") == 4)
                step = *(int *)ptr;
            else
                step = (int)*(int64_t *)ptr;
            int ncols = *(int *)lammps.lastThermo("num", 0);
            for (int i = 0; i < ncols; ++i) {
                if (chartwindow->numCharts() == 0) {
                    QString label = (const char *)lammps.lastThermo("keyword", i);
                    // no need to store the timestep column
                    if (label == "Step") continue;
                    chartwindow->addChart(label, i);
                }
                int datatype = *(int *)lammps.lastThermo("type", i);
                double data  = 0.0;
                if (datatype == 0) // int
                    data = *(int *)lammps.lastThermo("data", i);
                else if (datatype == 2) // double
                    data = *(double *)lammps.lastThermo("data", i);
                else if (datatype == 4) // bigint
                    data = (double)*(int64_t *)lammps.lastThermo("data", i);
                chartwindow->addData(step, data, i);
            }
        }
        chartwindow->resetZoom();
        chartwindow->setRangeEnabled(true);
    }

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
        void *ptr = lammps.lastThermo("line", 0);
        if (ptr) nline = *((int *)ptr);
    }

    if (success) {
        status->setText(GuiConstants::STATUS_READY);
        cpuuse->setText("   0%CPU");
    } else {
        status->setText("Failed.");
        textEdit->setHighlight(nline, true);
        critical(this, "LAMMPS-GUI Error", "<p>Error running LAMMPS:</p>",
                 QString("<p><pre>%1</pre></p>").arg(errmsg));
    }
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
};

void LammpsGui::doRun(bool use_buffer)
{
    if (lammps.isRunning()) {
        warning(this, "LAMMPS-GUI Warning", "Must stop current run before starting a new run");
        return;
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
            case QMessageBox::Cancel: // falthrough
            default:
                return;
                break;
        }
    }

    QSettings settings;
    progress->setValue(0);
    dirstatus->hide();
    progress->show();
    cpuuse->show();

    int numthreads = nthreads;
    int accel      = settings.value("accelerator", AcceleratorTab::OpenMP).toInt();
    if ((accel != AcceleratorTab::OpenMP) && (accel != AcceleratorTab::Intel) &&
        (accel != AcceleratorTab::Kokkos) && (accel != AcceleratorTab::Gpu))
        numthreads = 1;
    if (numthreads > 1)
        status->setText(QString("Running LAMMPS with %1 thread(s)...").arg(numthreads));
    else
        status->setText(QString("Running LAMMPS ..."));
    status->repaint();
    startLammps();
    if (!lammps.isOpen()) return;
    capturer->beginCapture();

    runner    = new LammpsRunner(this);
    isRunning = true;
    ++runCounter;

    // must delete all variables since clear does not delete them
    clearVariables();

    // define "gui_run" variable set to runCounter value
    lammps.command(std::string("variable gui_run index " + std::to_string(runCounter)));
    if (use_buffer) {
        // always add final newline since the text edit widget does not do it
        runner->setupRun(&lammps, (textEdit->toPlainText() + "\n").toStdString());
    } else {
        runner->setupRun(&lammps, {}, currentFile.toStdString());
    }

    // apply https proxy setting: prefer environment variable or fall back to preferences value
    auto https_proxy = QString::fromLocal8Bit(qgetenv("https_proxy"));
    if (https_proxy.isEmpty()) https_proxy = settings.value("https_proxy", "").toString();
    if (!https_proxy.isEmpty()) lammps.command(QString("shell putenv https_proxy=") + https_proxy);

    connect(runner, &LammpsRunner::resultReady, this, &LammpsGui::runDone);
    connect(runner, &LammpsRunner::finished, runner, &QObject::deleteLater);
    runner->start();

    // if configured, delete old log window before opening new one
    if (settings.value("logreplace", true).toBool()) delete logwindow;
    logwindow = new LogWindow(currentFile, this);
    logwindow->setReadOnly(true);
    logwindow->setCenterOnScroll(true);
    logwindow->moveCursor(QTextCursor::End);
    logwindow->setLineWrapMode(LogWindow::NoWrap);
    logwindow->setWindowTitle(
        QString("LAMMPS-GUI - Output - %1 - Run %2").arg(currentFile).arg(runCounter));
    logwindow->setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
    logwindow->setMinimumSize(GuiConstants::MINIMUM_WIDTH, GuiConstants::MINIMUM_HEIGHT);
    if (settings.value("viewlog", true).toBool())
        logwindow->show();
    else
        logwindow->hide();

    // if configured, delete old chart window before opening new one
    if (settings.value("chartreplace", true).toBool()) delete chartwindow;
    chartwindow = new ChartWindow(currentFile, this);
    chartwindow->setWindowTitle(
        QString("LAMMPS-GUI - Charts - %1 - Run %2").arg(currentFile).arg(runCounter));
    chartwindow->setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
    chartwindow->setMinimumSize(GuiConstants::MINIMUM_WIDTH, GuiConstants::MINIMUM_HEIGHT);

    const auto *unitptr = (const char *)lammps.extractGlobal("units");
    if (unitptr) chartwindow->setUnits(QString("%1").arg(unitptr));
    auto normflag = lammps.extractSetting("thermo_norm");
    chartwindow->setNorm(normflag != 0);
    chartwindow->setRangeEnabled(false);

    if (settings.value("viewchart", true).toBool())
        chartwindow->show();
    else
        chartwindow->hide();

    if (slideshow) {
        slideshow->setWindowTitle(QString("LAMMPS-GUI - Slide Show - " + currentFile));
        slideshow->clear();
        slideshow->hide();
    }

    logupdater = new QTimer(this);
    connect(logupdater, &QTimer::timeout, this, &LammpsGui::logUpdate);
    logupdater->start(settings.value("updfreq", "10").toInt());
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
                warning(this, "ImageViewer File Creation Error",
                        "Cannot create snapshot image from an input not creating a system box");
                return;
            }
        }
        // if configured, delete old image window before opening new one
        if (QSettings().value("imagereplace", true).toBool()) delete imagewindow;
        imagewindow = new ImageViewer(currentFile, &lammps, this);
        imagewindow->setMinimumSize(GuiConstants::MINIMUM_WIDTH, GuiConstants::MINIMUM_HEIGHT);
    } else {
        warning(this, "ImageViewer File Creation Error",
                "Cannot create snapshot image while LAMMPS is running");
        return;
    }
    imagewindow->show();
}

void LammpsGui::viewSlides()
{
    if (!slideshow) {
        slideshow = new SlideShow(currentFile, this);
        slideshow->setMinimumSize(GuiConstants::MINIMUM_WIDTH, GuiConstants::MINIMUM_HEIGHT);
    }
    if (slideshow->isVisible())
        slideshow->hide();
    else
        slideshow->show();
}

void LammpsGui::viewChart()
{
    QSettings settings;
    if (chartwindow) {
        if (chartwindow->isVisible()) {
            chartwindow->hide();
            settings.setValue("viewchart", false);
        } else {
            chartwindow->show();
            settings.setValue("viewchart", true);
        }
    }
}

void LammpsGui::viewLog()
{
    QSettings settings;
    if (logwindow) {
        if (logwindow->isVisible()) {
            logwindow->hide();
            settings.setValue("viewlog", false);
        } else {
            logwindow->show();
            settings.setValue("viewlog", true);
        }
    }
}

void LammpsGui::viewImage()
{
    if (imagewindow) {
        if (imagewindow->isVisible()) {
            imagewindow->hide();
        } else {
            imagewindow->show();
        }
    }
}

void LammpsGui::viewVariables()
{
    if (varwindow) {
        if (varwindow->isVisible()) {
            varwindow->hide();
        } else {
            varwindow->show();
        }
    }
}

void LammpsGui::setDocver()
{
    QString git_branch = (const char *)lammps.extractGlobal("git_branch");
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
    settings.beginGroup("reformat");
    autosave = settings.value("autosave", false).toBool();
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
    std::string version = "<b>This is LAMMPS-GUI version " LAMMPS_GUI_VERSION;
    version += " using Qt version " QT_VERSION_STR;
#ifdef LAMMPS_GUI_USE_QTGRAPHS
    version += " with QtGraphs";
#else
    version += " with QtCharts";
#endif
    if (isLightTheme())
        version += " using light theme";
    else
        version += " using dark theme";
    version += "</b><br><br>\n";
    if (lammps.hasPlugin()) {
        version += "LAMMPS library loaded as plugin";
        if (!pluginPath.isEmpty()) {
            version += " from file ";
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

    if (!QFile::exists(libPath)) {
        information(this, "Check for LAMMPS Update",
                    "No pre-compiled LAMMPS library found in the configuration folder. "
                    "Click on 'Download Library' in the preferences dialog to download one.");
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
        mb.setIcon(QMessageBox::Question);
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        mb.setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
        mb.setIconPixmap(QPixmap(":/icons/lammps-plugin.png").scaled(96, 96));

        // customize button icons
        auto *button = mb.button(QMessageBox::Yes);
        button->setIcon(QIcon(":/icons/dialog-ok.png"));
        button = mb.button(QMessageBox::No);
        button->setIcon(QIcon(":/icons/dialog-no.png"));

        if (mb.exec() == QMessageBox::Yes) {
            if (downloader.download(dlUrl, libPath, true)) {
                warning(this, "LAMMPS Shared Library Updated",
                        "The latest LAMMPS library has been downloaded successfully. "
                        "LAMMPS-GUI must be relaunched to activate it.");
                const auto path = QCoreApplication::applicationFilePath().toStdString();
                const auto arg0 = QCoreApplication::arguments().at(0).toStdString();
                execl(path.c_str(), arg0.c_str(), (char *)nullptr);
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
    mb.setWindowIcon(QIcon(GuiConstants::MAIN_ICON));
    mb.setText("<div>This is LAMMPS-GUI version " LAMMPS_GUI_VERSION "</div>");
    mb.setInformativeText(
        "<p>LAMMPS-GUI is a graphical text editor that is customized for "
        "editing LAMMPS input files and linked to the LAMMPS "
        "library and thus can run LAMMPS directly using the contents of the "
        "text buffer as input. It can retrieve and display information from "
        "LAMMPS while it is running and  display visualizations created "
        "with the dump image command.</p>"
        "<p>The main window of the LAMMPS-GUI is a text editor window with "
        "LAMMPS specific syntax highlighting. When typing <b>Ctrl-Enter</b> "
        "or clicking on 'Run LAMMMPS' in the 'Run' menu, LAMMPS will be run "
        "with the contents of editor buffer as input. The output of the LAMMPS "
        "run is captured and displayed in an Output window. The thermodynamic data "
        "is displayed in a chart window. Both are updated regularly during the "
        "run, as is a progress bar in the main window. The running simulation "
        "can be stopped cleanly by typing <b>Ctrl-/</b> or by clicking on "
        "'Stop LAMMPS' in the 'Run' menu. While LAMMPS is not running, "
        "an image of the simulated system can be created and shown in an image "
        "viewer window by typing <b>Ctrl-i</b> or by clicking on 'View Image' "
        "in the 'Run' menu. Multiple image settings can be changed through the "
        "buttons in the menu bar and the image will be re-renderd.  In case "
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
    mb.setIconPixmap(QPixmap(":/icons/lammps-gui-icon-128x128.png").scaled(64, 64));
    mb.setStandardButtons(QMessageBox::Close);
    auto *button = mb.button(QMessageBox::Close);
    button->setIcon(QIcon(":/icons/window-close.png"));
    mb.setFont(font());
    mb.exec();
}

void LammpsGui::manual()
{
    if (docver.isEmpty()) setDocver();
    QDesktopServices::openUrl(QUrl(QString("https://docs.lammps.org%1").arg(docver)));
}

void LammpsGui::tutorialWeb()
{
    QDesktopServices::openUrl(QUrl("https://lammpstutorials.github.io/"));
}

QWizardPage *LammpsGui::tutorialIntro(const int ntutorial, const QString &infotext)
{
    auto *page = new QWizardPage;
    page->setTitle(QString("Getting Started With Tutorial %1").arg(ntutorial));
    page->setPixmap(QWizard::WatermarkPixmap,
                    QPixmap(QString(":/icons/tutorial%1-logo.png").arg(ntutorial)));

    // TBD: TODO: update URL to published tutorial DOI
    auto *label = new QLabel(
        QString("<p>This dialog will help you to select and populate a folder with materials "
                "required to work through tutorial ") +
        QString::number(ntutorial) +
        QString(" from the LAMMPS tutorials article by Simon Gravelle, Cecilia Alvares, "
                "Jake Gissinger, and Axel Kohlmeyer.</p>"
                "<p>The materials for this tutorial are downloaded from:<br><b><a "
                "href=\"https://github.com/lammpstutorials/lammpstutorials-article\">https://"
                "github.com/lammpstutorials/lammpstutorials-article</a></b></p>") +
        infotext);
    label->setWordWrap(true);

    auto *layout = new QVBoxLayout;
    layout->addWidget(label);
    page->setLayout(layout);
    return page;
}

QWizardPage *LammpsGui::tutorialDirectory(const int ntutorial)
{
    QSettings settings;
    settings.beginGroup("tutorial");
    auto *page = new QWizardPage;
    page->setTitle(QString("Select Directory for Tutorial %1").arg(ntutorial));
    page->setPixmap(QWizard::WatermarkPixmap,
                    QPixmap(QString(":/icons/tutorial%1-logo.png").arg(ntutorial)));

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

    // if we are already in a tutorial folder, stay there or pick folder in same parent dir
    bool haveDir = false;
    for (int i = 1; i < 99; ++i) {
        if (currentDir.endsWith(QString("tutorial%1").arg(i))) {
            if (i > 9) { // We assume there are no more than 99 tutorials
                currentDir.chop(2);
            } else {
                currentDir.chop(1);
            }
            currentDir.append(QString::number(ntutorial));
            haveDir = true;
        }
    }

    // if current dir is home, or application folder, switch to desktop path
    if ((currentDir == QDir::homePath()) || currentDir.contains("AppData") ||
        currentDir.contains("system32") || currentDir.contains("Program Files")) {
        currentDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }
    if (!haveDir) currentDir.append(QString("/tutorial%1").arg(ntutorial));
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
    layout->addWidget(purgeval, Qt::AlignVCenter | Qt::AlignLeft);

    solval->setChecked(settings.value("solution", false).toBool());
    solval->setObjectName("t_getsolution");
    layout->addWidget(solval, Qt::AlignVCenter | Qt::AlignLeft);

    // we have tutorials 1 to 8 currently available online

    QCheckBox *webval = nullptr;
    if ((ntutorial > 0) && (ntutorial < 9)) {
        webval = new QCheckBox("&Open tutorial webpage in web browser");
        webval->setChecked(settings.value("webpage", true).toBool());
        webval->setObjectName("t_webopen");
        layout->addWidget(webval, Qt::AlignVCenter | Qt::AlignLeft);
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

void LammpsGui::startTutorial(int tutno)
{
    static const char *descriptions[] = {
        "<p>In tutorial 1 you will learn about LAMMPS input files, their syntax and "
        "structure, how to create and set up models and their interactions, how to run a "
        "minimization and a molecular dynamics trajectory, how to plot thermodynamic data "
        "and how to create visualizations of your system</p>",
        "<p>In tutorial 2 you will learn about setting up a simulation for a molecular "
        "system with bonds.  The target is to simulate a carbon nanotube with a "
        "conventional molecular force field under growing strain and observe the response "
        "to it.  Since bonds are represented by a harmonic potential, they cannot break.  "
        "This is then compared to simulating the same system with a reactive force field "
        "(AIREBO) where bonds may be broken and formed.</p>",
        "<p>In tutorial 3 you will learn setting up a multi-component, a polymer molecule embedded "
        "in liquid water.  The model employs a long-range Coulomb solver and a stretching force is "
        "applied to the polymer. This is used to demonstrate how to use the type label facility in "
        "LAMMPS to make components more generic.</p>",
        "<p>In tutorial 4 an electrolyte is simulated while confined between two walls and "
        "thus illustrating the specifics of simulating systems with fluid-solid "
        "interfaces.  The water model is more complex than in tutorial 3 and also a "
        "non-equilibrium MD simulation is performed by imposing shearing forces on the "
        "electrolyte through moving the walls.</p>",
        "<p>Tutorial 5 demonstrates the use of the ReaxFF reactive force field which "
        "includes a dynamic bond topology based on determining the bond order.  ReaxFF "
        "includes charge equilibration (QEq) and thus the atoms can change their partial "
        "charges according to the local environment.</p>",
        "<p>In tutorial 6 an MD simulation is combined with Monte Carlo (MC) steps to implement "
        "a Grand Canonical ensemble.  This represents an open system where atoms or "
        "molecules may be exchanged with a reservoir.</p>",
        "<p>In tutorial 7 you will determine the height of a free energy barrier through "
        "using umbrella sampling.  This is one of many advanced methods using specific "
        "reaction coordinates or so-called collective variables to map out relevant parts "
        "of free energy landscapes, where unbiased MD or MC simulation may take too "
        "long.</p>",
        "<p>In tutorial 8 a CNT embedded in a Nylon-6,6 polymer melt is simulated.  The "
        "REACTER protocol is used to model the polymerization of Nylon without having to "
        "employ far more computationally demanding models like ReaxFF.  Also, the "
        "formation of water molecules is tracked over time.</p>",
    };

    if (tutno < 1 || tutno > 8) return;

    delete wizard;
    wizard = new TutorialWizard(tutno, this);
    const auto infotext =
        QString(descriptions[tutno - 1]) +
        QString("<hr width=\"33%\"\\>\n<p align=\"center\">Click on the \"Next\" button "
                "to select a folder.</p>");
    wizard->setFont(font());
    wizard->addPage(tutorialIntro(tutno, infotext));
    wizard->addPage(tutorialDirectory(tutno));
    wizard->setWindowTitle(QString("Tutorial %1 Setup Wizard").arg(tutno));
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
}

void LammpsGui::editVariables()
{
    QList<QPair<QString, QString>> newvars = variables;
    SetVariables vars(newvars);
    vars.setFont(font());
    if (vars.exec() == QDialog::Accepted) {
        variables = newvars;
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
    int oldthreads   = settings.value("nthreads", 1).toInt();
    int oldaccel     = settings.value("accelerator", AcceleratorTab::None).toInt();
    bool oldecho     = settings.value("echo", false).toBool();
    bool oldcite     = settings.value("cite", false).toBool();
    int oldiprec     = settings.value("intelprec", AcceleratorTab::Mixed).toInt();
    bool oldgpuneigh = settings.value("gpuneigh", true).toBool();
    bool oldgpupair  = settings.value("gpupaironly", false).toBool();

    Preferences prefs(&lammps, this);
    prefs.setFont(font());
    prefs.setObjectName("preferences");
    if (prefs.exec() == QDialog::Accepted) {
        // must delete LAMMPS instance after preferences have changed that require
        // using different command line flags when creating the LAMMPS instance like
        // suffixes or package commands
        int newthreads = settings.value("nthreads", nthreads).toInt();
        int newaccel   = settings.value("accelerator", AcceleratorTab::None).toInt();
        int newiprec   = settings.value("intelprec", AcceleratorTab::Mixed).toInt();
        if ((oldaccel != newaccel) || (oldthreads != newthreads) || (oldiprec != newiprec) ||
            (oldecho != settings.value("echo", false).toBool()) ||
            (oldcite != settings.value("cite", false).toBool()) ||
            (oldgpuneigh != settings.value("gpuneigh", true).toBool()) ||
            (oldgpupair != settings.value("gpupaironly", false).toBool())) {
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
        if (imagewindow) imagewindow->createImage();
        settings.beginGroup("reformat");
        textEdit->setReformatOnReturn(settings.value("return", false).toBool());
        textEdit->setAutoComplete(settings.value("automatic", true).toBool());
        settings.endGroup();
    }
}

void LammpsGui::startLammps()
{
    // temporary extend lammpsArgs with additional arguments
    int initial_narg = lammpsArgs.size();
    QSettings settings;
    int accel = settings.value("accelerator", AcceleratorTab::None).toInt();
    // if non-threaded accelerator selected reset threads
    if ((accel == AcceleratorTab::None) || (accel == AcceleratorTab::Opt)) {
        nthreads = 1;
    }
    qputenv("OMP_NUM_THREADS", QByteArray::number(nthreads));

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
        int iprec = settings.value("intelprec", AcceleratorTab::Mixed).toInt();
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
        if (settings.value("gpuneigh", true).toBool())
            lammpsArgs.push_back("yes");
        else
            lammpsArgs.push_back("no");
        lammpsArgs.push_back("pair/only");
        if (settings.value("gpupaironly", false).toBool())
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
    if (settings.value("echo", false).toBool()) {
        lammpsArgs.push_back("-echo");
        lammpsArgs.push_back("screen");
    }
    if (settings.value("cite", false).toBool()) {
        lammpsArgs.push_back("-cite");
        lammpsArgs.push_back("screen");
    }

    // add variables, if defined
    for (auto &var : variables) {
        QString name  = var.first;
        QString value = var.second;
        if (!name.isEmpty() && !value.isEmpty()) {
            lammpsArgs.push_back("-var");
            lammpsArgs.push_back(name.toStdString());
            for (const auto &v : value.split(' ', Qt::SkipEmptyParts))
                lammpsArgs.push_back(v.toStdString());
        }
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

    // Must have at least LAMMPS version 30 March 2026
    if (lammps.version() < GuiConstants::MIN_LAMMPS_VERSION) {
        critical(this, "LAMMPS-GUI Error", "Incompatible LAMMPS Version:",
                 "LAMMPS-GUI version " LAMMPS_GUI_VERSION " requires\n"
                 "a LAMMPS version of at least " +
                     GuiConstants::MIN_LAMMPS_VERSION_STR);
        exit(1);
    }

    // remove additional arguments (3 were there initially)
    lammpsArgs.resize(initial_narg);

    const QString errmsg = lammps.lastErrorMessage();
    if (!errmsg.isEmpty()) critical(this, "LAMMPS-GUI Error", "Error launching LAMMPS:", errmsg);
}

bool LammpsGui::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close) {
        autoSave();
        quit();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// Base URL for tutorial input and solution files on the web
static const QString tutorialBaseUrl =
    "https://raw.githubusercontent.com/lammpstutorials/"
    "lammpstutorials-article/refs/heads/main/files/tutorial%1/%2";

void LammpsGui::setupTutorial(int tutno, const QString &dir, bool purgedir, bool getsolution,
                              bool openwebpage)
{
    QDir directory(dir);
    directory.cd(dir);

    // open web page of the corresponding online tutorial
    if (openwebpage) {
        QString weburl = "https://lammpstutorials.github.io/sphinx/build/html/tutorial%1/%2.html";
        switch (tutno) {
            case 1:
                weburl = weburl.arg(tutno).arg("lennard-jones-fluid");
                break;
            case 2:
                weburl = weburl.arg(tutno).arg("breaking-a-carbon-nanotube");
                break;
            case 3:
                weburl = weburl.arg(tutno).arg("polymer-in-water");
                break;
            case 4:
                weburl = weburl.arg(tutno).arg("nanosheared-electrolyte");
                break;
            case 5:
                weburl = weburl.arg(tutno).arg("reactive-silicon-dioxide");
                break;
            case 6:
                weburl = weburl.arg(tutno).arg("water-adsorption-in-silica");
                break;
            case 7:
                weburl = weburl.arg(tutno).arg("free-energy-calculation");
                break;
            case 8:
                weburl = weburl.arg(tutno).arg("reactive-molecular-dynamics");
                break;
            default:
                weburl = "https://lammpstutorials.github.io/";
        }
        QDesktopServices::openUrl(QUrl(weburl));
    }

    if (purgedir) purgeDirectory(dir);
    if (getsolution) directory.mkpath("solution");

    URLDownloader downloader(this);

    // download and process manifest for selected tutorial
    // must check for error after download, e.g. when there is no network.
    QString manifestPath = dir + QDir::separator() + ".manifest";
    if (!downloader.download(tutorialBaseUrl.arg(tutno).arg(".manifest"), manifestPath)) {
        critical(this, "LAMMPS-GUI Error",
                 "Tutorial files download error:", downloader.errorString());
        return;
    }

    QFile manifest(manifestPath);
    QString line, first;
    struct DownloadItem {
        DownloadItem(int _n, const QString &_f) : ntutorial(_n), fname(_f) {}

        int ntutorial;
        QString fname;
    };

    QList<DownloadItem> downloads;
    if (manifest.open(QIODevice::ReadOnly)) {
        while (!manifest.atEnd()) {
            line = (const char *)manifest.readLine();
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

    int i   = 0;
    int num = downloads.size();
    if (!num) num = 1;

    progress->setValue(0);
    progress->show();
    dirstatus->hide();

    for (const auto &item : downloads) {
        ++i;
        status->setText(QString("Downloading file %1 of %2").arg(i).arg(num));
        progress->setValue((int)((double)i / ((double)num) * 1000.0));
        status->repaint();

        QString localPath = dir + QDir::separator() + item.fname;
        if (!downloader.download(tutorialBaseUrl.arg(item.ntutorial).arg(item.fname), localPath)) {
            // download failed. abort, restore status line, and launch error dialog
            status->setText("Error.");
            progress->hide();
            dirstatus->show();
            status->repaint();
            critical(this, "LAMMPS-GUI Error",
                     "Tutorial files download error:", downloader.errorString());
            return;
        }

        // check if download is a placeholder for a symbolic link and make a copy instead.
        QFile dlfile(localPath);
        QFileInfo dlpath(localPath);
        if (dlfile.open(QIODevice::ReadOnly)) {
            line = (const char *)dlfile.readLine();
            line = line.trimmed();
            dlfile.close();

            if (line == QString("../") + dlpath.fileName()) {
                // the file is a symbolic link placeholder: copy the referenced file instead
                QString srcFile = dir + QDir::separator() + dlpath.fileName();
                QFile::remove(localPath);
                QFile::copy(srcFile, localPath);
            }
        }
    }
    progress->setValue(GuiConstants::PROGRESS_MAXIMUM);
    status->setText(GuiConstants::STATUS_READY);
    progress->hide();
    dirstatus->show();
    status->repaint();
    if (!first.isEmpty()) openFile(dir + QDir::separator() + first);
}

// Local Variables:
// c-basic-offset: 4
// End:
