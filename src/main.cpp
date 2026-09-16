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

#include "chartviewer.h"
#include "constants.h"
#include "fileviewer.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "plotdata.h"
#include "plotdatadialog.h"
#include "slideshow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QtGlobal>

#include <cstdio>

#if defined(Q_OS_WIN32)
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// In a GUI-subsystem (WIN32_EXECUTABLE) build the process starts without a console and
// the CRT leaves the standard streams without valid file descriptors.  That silently
// discards all LAMMPS output before the dup2()-based capture in StdCapture can grab it.
// If launched from a terminal, attach to that console; otherwise rebind the streams to
// the NUL device so they get valid descriptors.  Streams whose descriptor is already
// valid (e.g. redirected to a file by the parent process) are left untouched.
static void initConsoleIO()
{
    bool has_console = AttachConsole(ATTACH_PARENT_PROCESS);
    auto fd_invalid  = [](FILE *fp) {
        int fd = _fileno(fp);
        return (fd < 0) || (_get_osfhandle(fd) < 0);
    };
    // "NUL" without a colon: that is the spelling verified to work with the
    // MinGW runtime on an affected system; the "NUL:" form was never proven
    if (fd_invalid(stdin)) freopen(has_console ? "CONIN$" : "NUL", "r", stdin);
    if (fd_invalid(stdout)) freopen(has_console ? "CONOUT$" : "NUL", "w", stdout);
    if (fd_invalid(stderr)) freopen(has_console ? "CONOUT$" : "NUL", "w", stderr);
}
#else
// nothing to do
static void initConsoleIO() {}
#endif

#define stringify(x) myxstr(x)
#define myxstr(x) #x

int main(int argc, char *argv[])
{
    initConsoleIO();
#if defined(Q_OS_MACOS)
    // macOS does not support the "C" locale with UTF-8 encoding,
    // Since Qt requires UTF-8 we use "en_US" instead.
    qputenv("LC_ALL", "en_US.UTF-8");
#else
    // enforce using the plain ASCII C locale with UTF-8 encoding within the GUI.
    qputenv("LC_ALL", "C.UTF-8");
#endif

    // disable processor affinity for threads by default
    qputenv("OMP_PROC_BIND", "false");

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("The LAMMPS Developers");
    QCoreApplication::setOrganizationDomain("lammps.org");
    QCoreApplication::setApplicationName("LAMMPS-GUI (QT" stringify(QT_VERSION_MAJOR) ")");
    QCoreApplication::setApplicationVersion(LAMMPS_GUI_VERSION);
#if defined(LAMMPS_GUI_USE_PLUGIN)
    {
        // the library path is stored under a toolchain-qualified key (see
        // constants.h); carry the value of an installation from before the
        // split over once, and leave the legacy key for older versions
        QSettings settings;
        if (!settings.contains(Keys::PLUGIN_PATH) && settings.contains(Keys::PLUGIN_PATH_LEGACY))
            settings.setValue(Keys::PLUGIN_PATH, settings.value(Keys::PLUGIN_PATH_LEGACY));
    }
#endif
    QCommandLineParser parser;
    QString description(
        "\nThis is LAMMPS-GUI v" LAMMPS_GUI_VERSION "\n"
        "\nA graphical editor for LAMMPS input files with syntax highlighting and\n"
        "auto-completion that can run LAMMPS directly. It has built-in capabilities\n"
        "for monitoring, visualization, plotting, and capturing console output.");
#if defined(LAMMPS_GUI_USE_PLUGIN)
    description += QString("\n\nCurrent LAMMPS plugin path setting:\n  %1")
                       .arg(QSettings().value(Keys::PLUGIN_PATH, "").toString());
#endif
    parser.setApplicationDescription(description);

#if defined(LAMMPS_GUI_USE_PLUGIN)
    QCommandLineOption plugindir(QStringList() << "p"
                                               << "pluginpath",
                                 "Set path to LAMMPS shared library", "path");
    parser.addOption(plugindir);
#endif

    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions(
        {{{"x", "width"}, "Override LAMMPS-GUI editor window width", "width"},
         {{"y", "height"}, "Override LAMMPS-GUI editor window height", "height"},
         {{"s", "style"}, "Set LAMMPS-GUI's visual style (default: Fusion)", "style", "Fusion"},
         {{"c", "chart"}, "Open FILE directly in the chart/plot viewer", "file"},
         {{"i", "image"}, "Open FILE in the snapshot viewer (may be given multiple times)", "file"},
         {{"t", "text"}, "Open FILE in the text file viewer", "file"},
         {{"j", "joined"}, "Dock the output views into the main window for this run"},
         {{"w", "windows"}, "Show the output views as individual windows for this run"}});
    parser.addPositionalArgument("file", "The LAMMPS input file to open (optional).");
    parser.process(app);

#if defined(LAMMPS_GUI_USE_PLUGIN)
    if (parser.isSet(plugindir)) {
        QStringList pluginPath = parser.values(plugindir);
        QSettings settings;
        if (pluginPath.length() > 0) {
            settings.setValue(Keys::PLUGIN_PATH, QFileInfo(pluginPath.at(0)).canonicalFilePath());
            settings.sync();
        } else {
            // empty string provided -> delete any old setting
            settings.remove(Keys::PLUGIN_PATH);
        }
    }
#endif

    // -j/--joined and -w/--windows pick the layout for this run and leave the
    // preference alone.  This has to happen before any window is built, since
    // that is when the choice is read, and before the standalone viewers below
    // as well: they consult it too, for the chrome a dock panel does without.
    if (parser.isSet("joined") && parser.isSet("windows")) {
        fputs("Options -j/--joined and -w/--windows cannot be combined.\n", stderr);
        return 1;
    }
    if (parser.isSet("joined")) forceLayout(true);
    if (parser.isSet("windows")) forceLayout(false);

    // A standalone viewer is opened without a main window, so there is nothing
    // for it to dock into and it is an individual window whatever the
    // preference or the flags say.  It has to be told, because it asks the same
    // question the docked panels do and answers it the same way: a viewer that
    // took itself for a panel would come up with no menu bar of its own, since
    // in the combined window the main window shows that menu on its behalf.
    if (parser.isSet("chart") || parser.isSet("image") || parser.isSet("text")) forceLayout(false);

    int width  = parser.value("width").toInt();
    int height = parser.value("height").toInt();

    auto *usestyle = QStyleFactory::create(parser.value("style"));
    if (usestyle) QApplication::setStyle(usestyle);

#if defined(Q_OS_MACOS)
    GUI_MONOFONT = std::make_unique<QFont>("Menlo", -1, QFont::Normal);
    GUI_ALLFONT  = std::make_unique<QFont>("Arial", -1, QFont::Normal);
#elif defined(Q_OS_WIN32)
    GUI_MONOFONT = std::make_unique<QFont>("Consolas", -1, QFont::Normal);
    GUI_ALLFONT  = std::make_unique<QFont>("Arial", -1, QFont::Normal);
#else
    GUI_MONOFONT = std::make_unique<QFont>("Monospace", -1, QFont::Normal);
    GUI_ALLFONT  = std::make_unique<QFont>("Arial", -1, QFont::Normal);
#endif
    GUI_MONOFONT->setStyleHint(QFont::Monospace, QFont::PreferQuality);
    GUI_MONOFONT->setFixedPitch(true);
    GUI_ALLFONT->setStyleHint(QFont::SansSerif, QFont::PreferQuality);

    Q_INIT_RESOURCE(lammpsgui);

    // use our bundled icon theme so the standard text-edit context-menu actions
    // (undo/redo/cut/copy/paste/delete/select-all), which Qt fetches via
    // QIcon::fromTheme(), match our icon set instead of the desktop theme
    QIcon::setThemeSearchPaths(QStringList() << ":/icons");
    QIcon::setThemeName("lammpsgui");

    // -c/--chart: open a data file directly in a standalone chart window
    if (parser.isSet("chart")) {
        const QString fileName = parser.value("chart");
        QString error;
        auto dialog = PlotDataDialog::fromFile(fileName, nullptr, &error);
        if (!dialog) {
            critical(nullptr, "Plot Data File", "Could not read data from file:", error);
            return 1;
        }
        if (dialog->exec() != QDialog::Accepted) return 0;
        const PlotData plotData  = dialog->buildData();
        const PlotErrors plotErr = dialog->buildErrors();
        const int xcol           = dialog->xColumn();
        QList<int> ycols         = dialog->yColumns();
        if (ycols.isEmpty()) ycols.append(xcol);
        auto *win = new ChartWindow(fileName, nullptr);
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->setWindowTitle(QString("Plot: %1 - LAMMPS-GUI").arg(QFileInfo(fileName).fileName()));
        win->setWindowIcon(QIcon(Cfg::MAIN_ICON));
        win->setMinimumSize(Cfg::MINIMUM_WIDTH, Cfg::MINIMUM_HEIGHT);
        win->loadData(plotData, xcol, ycols, plotErr);
        win->show();
        return app.exec();
    }

    // -i/--image: open one or more image or movie files in a standalone snapshot viewer
    if (parser.isSet("image")) {
        const QStringList imageFiles = parser.values("image");
        if (imageFiles.isEmpty()) return 1;
        auto *viewer = new SlideShow(imageFiles.first());
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->setWindowIcon(QIcon(Cfg::MAIN_ICON));
        viewer->show();

        // the movie import dialog is modal to the (already visible) viewer
        for (const QString &f : imageFiles) {
            if (isMovieFile(f))
                viewer->addMovie(f);
            else
                viewer->addImage(f);
        }
        // every movie import was canceled or failed: delete the viewer directly
        // so its image cache is removed without a running event loop
        if (viewer->imageCount() == 0) {
            delete viewer;
            return 0;
        }
        return app.exec();
    }

    // -t/--text: open a file in a standalone text viewer
    if (parser.isSet("text")) {
        const QString fileName = parser.value("text");
        if (isMovieFile(fileName)) {
            critical(nullptr, "Cannot View Movie as Text",
                     "\"" + QFileInfo(fileName).fileName() +
                         "\" is a movie file. Use -i/--image to open it.");
            return 1;
        }
        if (isImageFile(fileName)) {
            critical(nullptr, "Cannot View Image as Text",
                     "\"" + QFileInfo(fileName).fileName() +
                         "\" is an image file. Use -i/--image to open it.");
            return 1;
        }
        if (looksLikeBinaryFile(fileName)) {
            critical(nullptr, "Cannot View Binary File as Text",
                     "\"" + QFileInfo(fileName).fileName() + "\" appears to be a binary file.");
            return 1;
        }
        auto *viewer = new FileViewer(fileName, nullptr);
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
        return app.exec();
    }

    // default: open the full LAMMPS-GUI main window
    QString infile;
    const QStringList args = parser.positionalArguments();
    if (!args.empty()) infile = args[0];

    LammpsGui w(nullptr, infile, width, height);
    w.show();

    return app.exec();
}

// Local Variables:
// c-basic-offset: 4
// End:
