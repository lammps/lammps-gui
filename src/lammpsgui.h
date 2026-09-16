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

#ifndef LAMMPSGUI_H
#define LAMMPSGUI_H

#include <QMainWindow>

#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringDecoder>
#include <string>
#include <vector>

#include "inputvariables.h"
#include "lammpssyntax.h"
#include "lammpswrapper.h"

// forward declarations

class QAction;
class QEvent;
class QFont;
class QLabel;
class QMenu;
class QMenuBar;
class QProgressBar;
class QSettings;
class QStatusBar;
class QTimer;
class QWidget;
class QWizardPage;

class ChartWindow;
class CodeEditor;
class CommandWindow;
class DownloadProgress;
class GeneralTab;
class Highlighter;
class ImageViewer;
class LammpsRunner;
class LogWindow;
class Preferences;
class SlideShow;
class StdCapture;
class TutorialWizard;
class URLDownloader;
class WindowLayout;

/**
 * @brief Main application window for LAMMPS-GUI
 *
 * LammpsGui is the central component of the LAMMPS-GUI application, serving as the main
 * window that coordinates all other components. It manages:
 * - The code editor for LAMMPS input scripts with syntax highlighting
 * - File operations (open, save, recent files)
 * - LAMMPS simulation execution and control
 * - Visualization windows (images, charts, log output)
 * - Application preferences and settings
 * - Tutorial wizard for interactive LAMMPS learning
 *
 * The class integrates with Qt's main window framework and provides menu actions,
 * toolbars, and status bar components. It uses a LammpsWrapper to interface with
 * the LAMMPS library and LammpsRunner to execute simulations in a separate thread.
 *
 * @see CodeEditor for the text editor component
 * @see ChartWindow for the charts window component
 * @see LogWindow for the log output window component
 * @see ImageViewer for the snapshot image window component
 * @see SlideShow for the slide show viewer window component
 * @see Preferences for the preferences window component
 * @see LammpsRunner for simulation execution in a separate thread
 * @see LammpsWrapper for LAMMPS library interface
 */
class LammpsGui : public QMainWindow {
    Q_OBJECT

    friend class CodeEditor;
    friend class Preferences;
    friend class AcceleratorTab;
    friend class GeneralTab;
    friend class TutorialWizard;

public:
    /**
     * @brief Construct the main application window
     * @param parent    Parent widget (typically nullptr for main window)
     * @param filename  Optional file to open on startup
     * @param width     Optional main editor window width override
     * @param height    Optional main editor window height override
     *
     * Initializes the main window, sets up the UI components, loads preferences,
     * initializes the LAMMPS library, and optionally opens a file if provided.
     */
    LammpsGui(QWidget *parent = nullptr, const QString &filename = QString(), int width = 0,
              int height = 0);

    /**
     * @brief Destructor
     *
     * Cleans up resources including dynamically created widgets and LAMMPS instances.
     */
    ~LammpsGui() override;

    LammpsGui()                             = delete;
    LammpsGui(const LammpsGui &)            = delete;
    LammpsGui(LammpsGui &&)                 = delete;
    LammpsGui &operator=(const LammpsGui &) = delete;
    LammpsGui &operator=(LammpsGui &&)      = delete;

protected:
    /** @brief Read a restart file into LAMMPS and open the inspection windows */
    void inspectFile(const QString &filename);

    /** @brief Write current editor content to a file */
    void writeFile(const QString &filename);

public:
    /**
     * @brief Load a file into the editor
     * @param filename File to edit; nothing happens if it is empty
     *
     * Ends a running simulation and closes the output windows, and offers to
     * save the current buffer first if it was changed.  Also reachable from the
     * command window's "edit".
     */
    void openFile(const QString &filename);

    /**
     * @brief Plot the columns of a data file, asking which ones
     * @param fileName Data file to read
     * @return false only if the user canceled the column dialog
     *
     * The return value lets a caller with several files to plot stop at the
     * first cancel rather than ask again for each of the rest.  Also reachable
     * from the command window's "plot".
     */
    bool plotFile(const QString &fileName);

    /**
     * @brief Open a file in a read-only text viewer
     * @param filename File to show; nothing happens if it is empty
     *
     * Refuses an image or a movie file, which belong in openImageFiles(), and a
     * binary one, which belongs nowhere.  Also reachable from the command
     * window's "open".
     */
    void viewFile(const QString &filename);

    /**
     * @brief Open image or movie files in a slide show viewer
     * @param files Images and movies to show together in one viewer
     *
     * The list is taken as given: openImages() collects it from a file dialog,
     * the command window's "open" from a shell that has expanded it.  Movie
     * files are offered for frame extraction as they are added, which is why
     * the viewer is shown before they are.
     */
    void openImageFiles(const QStringList &files);

    /**
     * @brief The menus that act on the application rather than on one view
     * @return Run, View, Tutorials and About, in the order they should appear
     *
     * These are owned by the main window but shown by every window that has a
     * menu bar: a QMenu can be added to more than one QMenuBar, which puts the
     * *same* actions there rather than duplicates.  That is what lets a run be
     * started or stopped from any window, and it is also why the accelerators
     * stay unambiguous -- one action matches once, however many menus show it.
     */
    QList<QMenu *> sharedMenus() const;

protected:
    /** @brief Set the editor window title from the current file and run number
     *
     * In the docked layout the views are named by their dock tab and no longer
     * carry a window title with the run number, so the editor title shows it. */
    void updateEditorTitle(const QString &file);

    /** @brief Update the recent files list */
    void updateRecents(const QString &filename = "");

    /** @brief Delete all variables defined in the LAMMPS instance */
    void clearVariables();

    /** @brief Rebuild the variables list from the editor buffer */
    void updateVariables();

    /**
     * @brief Fold input script edits into the variables list
     *
     * Re-parses the editor buffer and merges the result into the current
     * variables list: a changed index variable definition in the input wins
     * over a previous value, an unchanged one keeps the value set in the
     * Set Variables dialog.  Also updates the override markers shown in the
     * editor.  Called before the variables list is consumed (run setup,
     * input check, Set Variables dialog).
     */
    void refreshVariables();

    /**
     * @brief Execute a LAMMPS simulation
     * @param use_buffer If true, runs from editor buffer; if false, saves and runs from file
     * @param dryrun If true, checks the input via a dry run: LAMMPS executes
     *        the setup of every command but no timesteps (the equivalent of
     *        the -skiprun command line flag)
     */
    void doRun(bool use_buffer, bool dryrun = false);

    /**
     * @brief Check whether the LAMMPS instance holds a usable system state
     * @return true if LAMMPS is open, idle, and a simulation box is defined
     *
     * Guard for operations that act on the current system state outside of a
     * run, like writing a restart file or extending the previous run.
     */
    bool hasSystemState();

    /**
     * @brief Create and start a LammpsRunner thread and the log update timer
     * @param input      String of LAMMPS commands to execute (can be empty)
     * @param file       Input file path to execute (can be empty)
     * @param clearfirst If true, wipe the current LAMMPS system state first
     *
     * Shared tail of doRun() and extendRun(): dispatches the given input to a
     * new runner thread, connects its completion to runDone(), and starts the
     * periodic polling of captured output and thermo data.
     */
    void launchRunner(std::string input, std::string file, bool clearfirst);

    /** @brief Initialize and start a new LAMMPS instance */
    void startLammps();

    /** @brief Fill the syntax registry from LAMMPS library introspection
     *
     * Queries the command and style name lists from the running LAMMPS
     * instance into @c syntax and re-highlights the editor.  Does nothing
     * when no LAMMPS instance is available (the registry then stays
     * unpopulated and unknown-name marking is disabled). */
    void populateSyntax();

    /** @brief Variable names that are defined before the input runs
     *
     * The names from the Set Variables dialog plus the always-present
     * @c gui_run variable; passed to the input checker as preset names. */
    QStringList presetVariableNames() const;

    /** @brief Run the pre-run input lint and ask about found errors
     *
     * Called from doRun() when the pre-run check preference is enabled.
     * Warning-level findings never block; they are noted in the status bar.
     * Error-level findings pop up a "run anyway?" dialog.
     * @return true when the run should proceed */
    bool confirmLintIssues();

    /** @brief Handle completion of a LAMMPS run */
    void runDone();

    /** @brief Set the documentation version string for help links */
    void setDocver();

    /** @brief Perform an auto-save of the current file */
    void autoSave();

    /**
     * @brief Update the editor font
     * @param newfont The font to apply to the editor
     */
    void setFont(const QFont &newfont);

    /**
     * @brief Create an introduction page for a tutorial
     * @param collection Tutorial collection index
     * @param ntutorial Tutorial number within the collection
     * @param infotext Information text to display
     * @return Wizard page with tutorial introduction
     */
    QWizardPage *tutorialIntro(int collection, int ntutorial, const QString &infotext);

    /**
     * @brief Create a directory selection page for a tutorial
     * @param collection Tutorial collection index
     * @param ntutorial Tutorial number within the collection
     * @return Wizard page for tutorial directory selection
     */
    QWizardPage *tutorialDirectory(int collection, int ntutorial);

    /**
     * @brief Set up files and resources for a tutorial
     * @param collection Tutorial collection index
     * @param tutno Tutorial number within the collection
     * @param dir Directory to create files in
     * @param purgedir Whether to clean the directory first
     * @param getsolution Whether to include solution files
     * @param openwebpage Whether to open the tutorial web page
     */
    void setupTutorial(int collection, int tutno, const QString &dir, bool purgedir,
                       bool getsolution, bool openwebpage);

    /** @brief Clean up the inspect file dialog list */
    void purgeInspectList();

    /**
     * @brief Event filter for handling special events
     * @param watched Object being watched
     * @param event Event to filter
     * @return true if event was handled
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    /** @brief Quit the application */
    void quit();

    /** @brief Stop a running LAMMPS simulation */
    void stopRun();

    /** @brief Run LAMMPS with content from editor buffer */
    void runBuffer() { doRun(true); }

private slots:
    /** @brief Create a new document */
    void newDocument();

    /** @brief Open an existing file */
    void open();

    /** @brief View a file in read-only mode */
    void view();

    /** @brief Open one or more image files in a standalone snapshot viewer */
    void openImages();

    /** @brief Select and inspect a restart file */
    void inspect();

    /** @brief Write a restart file with the current state of the system */
    void writeRestart();

    /** @brief Open a file from the recent files list */
    void openRecent();

    /** @brief Change the working directory */
    void getDirectory();

    /** @brief Start external executable */
    void startExe();

    /** @brief Save the current file */
    void save();

    /** @brief Save the current file with a new name */
    void saveAs();

    /** @brief Copy selected text to clipboard */
    void copy();

    /** @brief Cut selected text to clipboard */
    void cut();

    /** @brief Paste text from clipboard */
    void paste();

    /** @brief Undo last edit action */
    void undo();

    /** @brief Redo previously undone action */
    void redo();

    /** @brief Open find and replace dialog */
    void findAndReplace();

    /** @brief Run LAMMPS from saved file */
    void runFile() { doRun(false); }

    /** @brief Extend the previous run by a number of steps queried in a dialog */
    void extendRun();

    /** @brief Run the static input check and show the findings in a dialog */
    void checkInput();

    /** @brief Check the editor buffer via a dry run (setup only, no timesteps) */
    void dryRunBuffer() { doRun(true, true); }

    /** @brief Restart LAMMPS with a new instance */
    void restartLammps();

    /** @brief Open dialog to edit index-style variables */
    void editVariables();

    /** @brief Render an image from a dump file */
    void renderImage();

    /** @brief Open an external data file and plot selected columns */
    void plotDataFile();

    /**
     * @brief Adopt a chart window a post-processing analysis created
     * @param win   The new chart window (self-deleting on close)
     * @param title Short label for its tab in the combined layout
     *
     * Hands the window to the WindowLayout, so in the docked layout it joins
     * the Charts tab group instead of opening as a free window, and connects
     * the window's own resultWindowCreated() signal here, so results of
     * results (a transform of an autocorrelation) join the same group.
     */
    void adoptChartResult(ChartWindow *win, const QString &title);

    /** @brief View a slideshow of images */
    void viewSlides();

    /** @brief View a single image */
    void viewImage();

    /** @brief View a chart from thermodynamic data */
    void viewChart();

    /** @brief View the log window */
    void viewLog();

    /** @brief Toggle the shell prompt window from the View menu
     *
     * On first use there is nothing to hide, so it opens the window. */
    void viewCommand();

    /** @brief Open and raise the shell prompt window (Run menu) */
    void openCommandWindow();

    /** @brief View current variable definitions */
    void viewVariables();

    /** @brief Show about dialog */
    void about();

#if defined(LAMMPS_GUI_USE_PLUGIN)
    /** @brief Check for LAMMPS library updates */
    void checkUpdate();
#endif

    /** @brief Show context-sensitive help */
    void help();

    /** @brief Open LAMMPS manual */
    void manual();

    /** @brief Open tutorial web page */
    void tutorialWeb();

    /**
     * @brief Start a tutorial wizard
     * @param collection Tutorial collection index
     * @param tutno Tutorial number within the collection
     */
    void startTutorial(int collection, int tutno);

    /** @brief Open HOWTO documentation */
    void howto();

    /** @brief Update log window with new output */
    void logUpdate();

    /** @brief Handle document modification */
    void modified();

    /** @brief Open preferences dialog */
    void preferences();

    /** @brief Reset settings to defaults */
    void defaults();

private:
    /** @brief Update CPU/progress/line/variable status while a run is active
     *  @return run completion in permille (1000 when not running) */
    int updateRunStatus();

    /** @brief Append the cached thermo columns for the current step to the charts */
    void updateChartData(int step, int ncols);

    /** @brief Append any newly rendered dump image to the slideshow */
    void updateSlideShow();

    /** @brief Decode the next chunk of captured output as part of one UTF-8 stream */
    QString decodeLog(const std::string &bytes);

    /** @brief End a running simulation and dispose of its runner thread */
    void abortRun();

    /** @brief Close the LAMMPS instance quietly and clear its status indicator */
    void closeLammpsInstance();

    /** @brief Delete the output windows of the current input (charts, log, slide show, image,
     * variables) */
    void closeOutputWindows();

    /** @brief Show the run progress widgets in the status bar with the given message */
    void beginRunStatus(const QString &message);

    /** @brief The current time step, whatever the width of the library's bigint */
    int currentStep();

    /**
     * @brief Put the focused view's own menu at the front of the menu bar
     * @param focused Widget that just took the keyboard focus
     *
     * Combined layout only; does nothing with individual windows, where each
     * window carries its own menu bar.
     */
    void updateMenuBarForFocus(QWidget *focused);

    /** @brief Append accelerator-package command-line arguments to lammpsArgs */
    void appendAcceleratorArgs(int accel, QSettings &settings);

    /** @brief Open the online web page for the given collection and tutorial number */
    void openTutorialWebpage(int collection, int tutno);

    /** @brief One file to fetch for a tutorial: tutorial number and relative filename */
    struct DownloadItem {
        DownloadItem(int _n, const QString &_f) : ntutorial(_n), fname(_f) {}
        int ntutorial;
        QString fname;
    };

    /**
     * @brief Download the listed tutorial files from @p baseUrl; false on error
     *
     * Per-file progress is shown in @p dlg.  A file that fails to download is
     * recorded and skipped so one missing file (e.g. from a stale manifest
     * entry) does not discard the rest of the tutorial; after the batch a
     * dialog lists the missing files and offers to report them at
     * @p issuesUrl.  Only a download canceled by the user aborts the batch
     * (the dialog is closed and false is returned).  On success the dialog
     * stays open (the caller closes it when the whole setup is complete).
     */
    bool downloadTutorialFiles(const QString &dir, const QList<DownloadItem> &downloads,
                               URLDownloader &downloader, const QString &baseUrl,
                               DownloadProgress &dlg, const QString &issuesUrl);

    /** @brief Create and show/hide the output log window for a run */
    void createLogWindow(QSettings &settings);

    /** @brief Create and show/hide the thermo chart window for a run */
    void createChartWindow(QSettings &settings);

    /** @brief Say in the log window when the output capture cannot work
     *
     * The failure is otherwise silent: printf() reports success and the
     * runtime drops the bytes.  Must be called after createLogWindow(). */
    void reportCaptureFailure();

    /** @brief Prove that the library's own output reaches the capture
     *
     * Pushes a marker line through the LAMMPS library and drains it from the
     * capture again; a marker that does not return sets a warning naming the
     * loaded library file, shown by reportCaptureFailure().  Must run after
     * beginCapture() and before the runner thread starts. */
    void verifyLibraryCapture();

    /** @brief Create the shell prompt window if it does not exist yet
     *
     * The shell starts in the directory of the current input file. */
    void createCommandWindow();

    /** @brief Warn (modal) if the stdout capture buffer usage was high */
    void warnHighBufferUsage();

    /** @brief Append the final thermo data point to the charts at run end */
    void finalizeChartData();

    /**
     * @brief Create all menu actions, menus, and status bar
     * @param settings application settings class instance
     * @param allFont global proportional font selection
     * @param monoFont global monospace font selection
     */
    void setupUi(QSettings &settings, QFont &allFont, QFont &monoFont);

    /**
     * @brief Configure, check, and download the LAMMPS shared library
     * @param settings application settings class instance
     */
    void setupPlugin(QSettings &settings);

    /**
     * @brief Configure, check, and assign LAMMPS accelerator package settings
     * @param settings application settings class instance
     */
    void setupAccelerators(QSettings &settings);

    /**
     * @brief Create a menu action with optional icon and shortcut and append it to a menu
     * @param menu     Menu to append the new action to
     * @param iconpath Resource path for the action icon (empty for no icon)
     * @param text     Action label text
     * @param shortcut Keyboard shortcut sequence (empty for none)
     * @param slot     Member function pointer or callable invoked on trigger
     * @return The created action, for any further configuration by the caller
     */
    template <typename Func>
    QAction *addMenuAction(QMenu *menu, const QString &iconpath, const QString &text,
                           const QString &shortcut, Func slot);

    /** @brief Create File menu actions and add them to the menu bar */
    void createFileMenu();

    /** @brief Create Edit menu actions and add them to the menu bar */
    void createEditMenu();

    /** @brief Create Run menu actions and add them to the menu bar */
    void createRunMenu();

    /** @brief Create View menu actions and add them to the menu bar */
    void createViewMenu();

    /** @brief Create Tutorials menu with tutorial actions */
    void createTutorialMenu();

    /** @brief Create About/Help menu actions and add them to the menu bar */
    void createAboutMenu();

    /** @brief Create the status bar and its widgets */
    void createStatusBar();

    /** @brief Create (or recreate) the floating "Current Variables" window.
     *
     * @c varwindow is torn down together with the other output windows when the
     * editor is reset (newDocument()/openFile()), so it must be recreated on
     * demand -- see viewVariables(). */
    void createVariableWindow();

    // Central GUI elements
    CodeEditor *textEdit;           ///< Custom code editor widget
    QMenuBar *menubar;              ///< Menu bar with menus and actions
    QMenu *filemenu;                ///< Editor File menu, swapped out for a view's own when docked
    QMenu *editmenu;                ///< Editor Edit menu, shown only for the editor
    QMenu *currentviewmenu;         ///< View menu currently at the front of the bar
    QMenu *runmenu;                 ///< Run menu, shared with the other windows
    QMenu *viewmenu;                ///< View menu, shared with the other windows
    QMenu *tutorialmenu;            ///< Tutorials menu, shared with the other windows
    QMenu *aboutmenu;               ///< About menu, shared with the other windows
    QStatusBar *statusbar;          ///< status bar
    QList<QAction *> recentActions; ///< list of actions for recent files

    LammpsSyntax syntax;          ///< Syntax registry for highlighting and input checking
    bool dryRunActive = false;    ///< current run is an input check dry run
    Highlighter *highlighter;     ///< Syntax highlighter for LAMMPS input
    StdCapture *capturer;         ///< Captures stdout/stderr from LAMMPS
    QLabel *status;               ///< Status bar label for general status
    QLabel *cpuuse;               ///< Status bar label for CPU usage
    int lastCpuBucket;            ///< Last applied cpuuse color bucket (-1 = none yet)
    LogWindow *logwindow;         ///< Window displaying LAMMPS output log
    ImageViewer *imagewindow;     ///< Window for viewing single images
    ChartWindow *chartwindow;     ///< Window for displaying charts
    SlideShow *slideshow;         ///< Window for image slideshow
    CommandWindow *commandwindow; ///< Window with a shell prompt
    QTimer *logupdater;           ///< Timer for periodic log updates
    QLabel *dirstatus;            ///< Status bar label showing current directory
    QProgressBar *progress;       ///< Progress bar for long operations
    Preferences *prefdialog;      ///< Preferences dialog
    QLabel *lammpsstatus;         ///< Status bar label for LAMMPS state
    QLabel *varwindow;            ///< Window showing variable definitions
    TutorialWizard *wizard;       ///< Tutorial wizard dialog
    WindowLayout *viewlayout;     ///< Presentation policy for the output windows above

    /**
     * @brief Container for inspect dialog widgets
     *
     * Holds references to the three tabs (info, data, image) in an inspect dialog
     */
    struct InspectData {
        /// Held weakly: a view that is closed deletes itself, and purgeInspectList()
        /// reaps the entry rather than the widget in that case
        QPointer<QWidget> info;  ///< Information tab widget
        QPointer<QWidget> data;  ///< Data viewing tab widget
        QPointer<QWidget> image; ///< Image rendering tab widget
    };
    QList<InspectData *> inspectList; ///< List of open inspect dialogs

    QString currentFile;            ///< Path to currently opened file
    QString currentDir;             ///< Current working directory
    QList<QString> recent;          ///< List of recently opened files
    QList<VariableEntry> variables; ///< Index-style variable definitions

    LammpsWrapper lammps;   ///< Interface to LAMMPS library
    LammpsRunner *runner;   ///< Thread for running LAMMPS simulations
    QString docver;         ///< LAMMPS documentation version string
    QString pluginPath;     ///< Path to LAMMPS shared library (plugin mode)
    QString capturewarning; ///< Library-side capture check result for this run
    int runCounter;         ///< Counter for simulation runs
    bool showSlides = true; ///< Show the slide show when a run writes images
    QStringDecoder logDecoder{QStringDecoder::Utf8}; ///< Decodes the captured output stream
    int extendSteps;                     ///< Last used step count of the Extend Run dialog
    std::vector<std::string> lammpsArgs; ///< Command-line arguments for LAMMPS

protected:
    int nthreads;      ///< Number of threads for parallel execution
    int mainx;         ///< Override value for main editor window width or 0
    int mainy;         ///< Override value for main editor window height or 0
    bool hasClipboard; ///< true if Qt was configured with Clipboard support, otherwise false
};

#endif // LAMMPSGUI_H

// Local Variables:
// c-basic-offset: 4
// End:
