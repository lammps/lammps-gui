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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

/**
 * @brief Application-wide constants for LAMMPS-GUI
 *
 * Centralizes magic numbers and repeated string literals that were previously
 * scattered across the codebase.  Grouping by category makes maintenance easier
 * and reduces the risk of typos from duplicated literals.
 *
 * The namespace name is deliberately short: these constants are internal, not
 * an exported interface, so call sites read @c Cfg::NAME directly without a
 * @c using directive or alias.
 */
namespace Cfg {

// ---- UI dimensions -------------------------------------------------------
constexpr int DEFAULT_BUFLEN =
    1024; ///< Default length for C-string buffers (error messages, names)
constexpr int MAX_DEFAULT_THREADS   = 16;  ///< Maximum default thread count
constexpr int MINIMUM_WIDTH         = 400; ///< Minimum window width in pixels
constexpr int MINIMUM_HEIGHT        = 300; ///< Minimum window height in pixels
constexpr int ICON_SCALE            = 22;  ///< Status bar icon dimension in pixels
constexpr int TOOLBAR_ICON_SIZE     = 24;  ///< Icon size in pixels for tool/status-bar buttons
constexpr int TOOLBAR_BUTTON_MARGIN = 6; ///< Pixels added to the size hint for square tool buttons
constexpr int PROGRESS_MAXIMUM      = 1000; ///< Maximum value for QProgressBar

// ---- File limits ---------------------------------------------------------
constexpr int NUM_RECENT_FILES = 5; ///< Number of entries in the recent files list

// ---- Run control ---------------------------------------------------------
constexpr int EXTEND_STEPS_DEFAULT = 1000; ///< Default number of steps in the Extend Run dialog

// ---- Downloadable LAMMPS shared library ----------------------------------
// all platform variants are listed so they can be cleaned up from a
// configuration folder that is shared between different machines
inline const QString LAMMPS_LIB_MACOS =
    QStringLiteral("liblammps.0.dylib"); ///< Downloaded library name on macOS
inline const QString LAMMPS_LIB_WINDOWS =
    QStringLiteral("liblammps.dll"); ///< Downloaded library name on Windows
inline const QString LAMMPS_LIB_LINUX =
    QStringLiteral("liblammps.so.0"); ///< Downloaded library name on Linux
inline const QString BACKUP_SUFFIX =
    QStringLiteral(".bak"); ///< Suffix for the backup name of a replaced file

// ---- LAMMPS version requirement ------------------------------------------
constexpr int MIN_LAMMPS_VERSION =
    20260902; ///< Minimum LAMMPS version (2 September 2026) as YYYYMMDD format number
inline const QString MIN_LAMMPS_VERSION_STR =
    QStringLiteral("2 September 2026"); ///< Minimum LAMMPS version (2 September 2026) as string

// ---- Buffer thresholds ---------------------------------------------------
constexpr double BUFFER_WARNING_THRESHOLD = 0.333; ///< Warn when capture buffer exceeds this
constexpr int THERMO_SUGGEST_MULTIPLIER   = 5;     ///< Multiplier for thermo interval suggestion

// ---- Preferences dialog --------------------------------------------------
constexpr int PREFERENCES_WIDTH  = 700; ///< Preferences dialog default width in pixels
constexpr int PREFERENCES_HEIGHT = 500; ///< Preferences dialog default height in pixels

// ---- Update intervals (milliseconds) -------------------------------------
constexpr int DATA_UPDATE_INTERVAL_MIN      = 1;    ///< Min log/data update interval
constexpr int DATA_UPDATE_INTERVAL_MAX      = 1000; ///< Max log/data update interval
constexpr int DATA_UPDATE_INTERVAL_DEFAULT  = 10;   ///< Default log/data update interval
constexpr int CHART_UPDATE_INTERVAL_MIN     = 1;    ///< Min chart update interval
constexpr int CHART_UPDATE_INTERVAL_MAX     = 5000; ///< Max chart update interval
constexpr int CHART_UPDATE_INTERVAL_DEFAULT = 500;  ///< Default chart update interval

// ---- Chart dimension ranges and defaults (pixels) ------------------------
constexpr int CHART_WIDTH_MIN        = 400;   ///< Min configurable chart width
constexpr int CHART_WIDTH_MAX        = 40000; ///< Max configurable chart width
constexpr int CHART_HEIGHT_MIN       = 300;   ///< Min configurable chart height
constexpr int CHART_HEIGHT_MAX       = 30000; ///< Max configurable chart height
constexpr int CHART_DEFAULT_WIDTH    = 640;   ///< Default chart width
constexpr int CHART_DEFAULT_HEIGHT   = 480;   ///< Default chart height
constexpr double CHART_YPAD_FRACTION = 0.05;  ///< Relative y-axis margin around the data range

// ---- Chart series style defaults -----------------------------------------
// The color defaults are indices into the fixed palette of the color combo
// boxes in the charts preferences tab (black, blue, red, green, gray).
constexpr int RAWBRUSH_DEFAULT      = 1;    ///< Default raw series color (blue)
constexpr int SMOOTHBRUSH_DEFAULT   = 2;    ///< Default processed series color (red)
constexpr int ERRBRUSH_DEFAULT      = 0;    ///< Default error bar color (black)
constexpr double LINE_WIDTH_MIN     = 0.5;  ///< Min configurable line width
constexpr double LINE_WIDTH_MAX     = 20.0; ///< Max configurable line width
constexpr double LINE_WIDTH_DEFAULT = 3.0;  ///< Default data series line width
constexpr double ERR_WIDTH_DEFAULT  = 1.5;  ///< Default error bar line width
constexpr double POINT_SIZE_MIN     = 1.0;  ///< Min configurable marker diameter
constexpr double POINT_SIZE_MAX     = 40.0; ///< Max configurable marker diameter
constexpr double POINT_SIZE_DEFAULT = 8.0;  ///< Default marker diameter
constexpr int PROC_LABEL_MAXLEN =
    12; ///< Longest post-process name shown as the processed-slot label as is

// ---- Chart post-processing dialog ----------------------------------------
constexpr int POSTPROCESS_EXPR_WIDTH  = 260; ///< Min width of the custom-function expression field
constexpr int POSTPROCESS_GRID_POINTS = 200; ///< Default number of Fourier output grid points

// ---- Plot data import dialog ---------------------------------------------
constexpr int PLOTDIALOG_COLUMN_LIST_HEIGHT = 180; ///< Min height of the column-role list
constexpr int PLOTDIALOG_PREVIEW_ROWS       = 8;   ///< Data rows shown in the preview table

// ---- Chart smoothing (Savitzky-Golay) ------------------------------------
constexpr int SMOOTH_WINDOW_MIN     = 5;   ///< Min smoothing window size
constexpr int SMOOTH_WINDOW_MAX     = 999; ///< Max smoothing window size
constexpr int SMOOTH_WINDOW_DEFAULT = 10;  ///< Default smoothing window size
constexpr int SMOOTH_ORDER_MIN      = 1;   ///< Min smoothing polynomial order
constexpr int SMOOTH_ORDER_MAX      = 20;  ///< Max smoothing polynomial order
constexpr int SMOOTH_ORDER_DEFAULT  = 4;   ///< Default smoothing polynomial order

// ---- Auto-completion -----------------------------------------------------
constexpr int COMPLETION_CHARS_MIN = 1;  ///< Min characters before auto-completion triggers
constexpr int COMPLETION_CHARS_MAX = 32; ///< Max characters before auto-completion triggers

// ---- Inactive (grayed out) icons -----------------------------------------
/** Gray level that the pixels of an inactive icon are faded towards */
constexpr int GRAYSCALE_MIDPOINT = 145;
/** Fraction of its contrast that an inactive icon keeps; 1.0 desaturates only */
constexpr double GRAYSCALE_CONTRAST = 0.4;

// ---- Network downloads ---------------------------------------------------
// a download is aborted when no data arrives for the number of seconds set in
// the Keys::DOWNLOAD_TIMEOUT preference; a stalled connection otherwise blocks
// the download event loop indefinitely
constexpr int DOWNLOAD_TIMEOUT_MIN      = 5;   ///< Min download stall timeout in seconds
constexpr int DOWNLOAD_TIMEOUT_MAX      = 300; ///< Max download stall timeout in seconds
constexpr int DOWNLOAD_TIMEOUT_DEFAULT  = 10;  ///< Default download stall timeout in seconds
constexpr int DOWNLOAD_DIALOG_WIDTH     = 500; ///< Min width of the download progress dialog
constexpr int DOWNLOAD_DIALOG_LOGO_SIZE = 96;  ///< Logo size in the download progress dialog

// ---- Movie frame import --------------------------------------------------
constexpr int MOVIE_PROBE_TIMEOUT = 15000; ///< Timeout in milliseconds for an ffprobe run
constexpr int MOVIE_WARN_FRAMES   = 1000;  ///< Warn when extracting more frames than this
/** Warn when the extracted frames are estimated to need more than this many bytes */
constexpr qint64 MOVIE_WARN_BYTES = 1024LL * 1024LL * 1024LL;
/** Warn when the estimated size exceeds this fraction of the free space on the temporary volume */
constexpr double MOVIE_WARN_DISKFRAC = 0.9;

// ---- Docked window layout ------------------------------------------------
/// Version tag written into the saved dock arrangement.  Bump it whenever the
/// set of docks or their default arrangement changes, so QMainWindow discards a
/// saved state that no longer matches instead of restoring something stale.
/// Bumped when ViewSlot::Command was added: a saved arrangement from before
/// does not know that panel and must not be restored over the new default.
constexpr int DOCK_STATE_VERSION  = 2;
constexpr int MAIN_DEFAULT_WIDTH  = 1024; ///< Default main window width, individual windows
constexpr int MAIN_DEFAULT_HEIGHT = 512;  ///< Default main window height, individual windows
// The combined main window defaults to twice the individual-window size,
// since it holds the editor plus two dock groups.  Clamped to the available
// screen area where it is applied.
constexpr int DOCK_MAIN_DEFAULT_WIDTH  = 2 * MAIN_DEFAULT_WIDTH;  ///< Docked main window width
constexpr int DOCK_MAIN_DEFAULT_HEIGHT = 2 * MAIN_DEFAULT_HEIGHT; ///< Docked main window height
/** Fraction of the combined window width given to the right hand dock group */
constexpr double DOCK_SPLIT_HORIZONTAL = 0.5;
/** Fraction of the combined window height given to the bottom dock group */
constexpr double DOCK_SPLIT_VERTICAL = 0.25;

/// Object name a view gives its own File menu, so the combined layout can put
/// it at the front of the shared menu bar without knowing the view's class
inline const QString VIEW_FILE_MENU = QStringLiteral("viewFileMenu");

// ---- Command window ------------------------------------------------------
constexpr int COMMAND_SCROLLBACK_LINES = 5000; ///< Lines of transcript kept
constexpr int COMMAND_HISTORY_MAX      = 200;  ///< Command lines remembered between sessions
constexpr int COMMAND_START_TIMEOUT    = 5000; ///< Milliseconds to wait for the shell to start
constexpr int COMMAND_EXIT_TIMEOUT     = 2000; ///< Milliseconds to wait for it to end
constexpr int COMMAND_KILL_GRACE       = 500;  ///< Milliseconds between SIGTERM and SIGKILL
constexpr int COMMAND_PGREP_TIMEOUT    = 1000; ///< Milliseconds to wait for pgrep
constexpr int COMMAND_DEFAULT_WIDTH    = 800;  ///< Default window width
constexpr int COMMAND_DEFAULT_HEIGHT   = 400;  ///< Default window height
constexpr int COMMAND_RESYNC_DELAY     = 300;  ///< Milliseconds after an interrupt before resyncing
constexpr int COMMAND_MIN_COLUMNS      = 20;   ///< Narrowest width worth reporting as COLUMNS
constexpr int COMMAND_MIN_CWD_WIDTH    = 120; ///< Pixels the directory in front of the prompt keeps
constexpr int COMMAND_MIN_PROMPT_WIDTH =
    240;                                    ///< Pixels the input line keeps whatever the directory
constexpr int ALIASES_DEFAULT_WIDTH  = 520; ///< Default width of the alias dialog
constexpr int ALIASES_DEFAULT_HEIGHT = 420; ///< Default height of the alias dialog

// ---- Resource paths ------------------------------------------------------
/** path to LAMMPS-GUI Window Icon resource */
inline const QString MAIN_ICON = QStringLiteral(":/icons/lammps-gui-icon-128x128.png");
/** path to LAMMPS Icon resource */
inline const QString LAMMPS_ICON = QStringLiteral(":/icons/lammps-icon-128x128.png");
/** path to the command spec table resource for the syntax engine */
inline const QString SYNTAX_SPEC_TABLE = QStringLiteral(":/command_specs.table");

// ---- Restart file inspection ----------------------------------------------
/** restart files larger than this (bytes) prompt a memory-use warning */
constexpr qint64 INSPECT_WARN_SIZE = 262144000LL;
/** divisor turning a restart file size into an estimated RAM demand in GB */
constexpr double INSPECT_GB_PER_BYTE = 134217728.0;

// ---- Fixed RNG seeds for LAMMPS commands ----------------------------------
/** seed for the create_atoms command placing the temporary molecule */
constexpr int CREATE_ATOMS_SEED = 312944;
/** seed for the dump image ssao keyword */
constexpr int SSAO_SEED = 453983;
/** fixed SSAO sample count for interactive Image Viewer renders when the
 *  SSAO samples setting is "auto" (speed over quality); an explicitly
 *  configured count is used as-is */
constexpr int SSAO_VIEW_SAMPLES = 8;

// ---- Documentation ---------------------------------------------------------
/** base URL of the LAMMPS online documentation */
inline const QString DOCS_URL = QStringLiteral("https://docs.lammps.org");

// ---- Charts ----------------------------------------------------------------
/** default chart title template; %f is replaced with the input file name */
inline const QString CHART_TITLE_DEFAULT = QStringLiteral("Thermo: %f");

// ---- Status messages -----------------------------------------------------
/** status string when LAMMPS-GUI is ready */
inline const QString STATUS_READY = QStringLiteral("Ready.");
/** CPU utilization status label text when no simulation is running */
inline const QString STATUS_ZERO_CPU = QStringLiteral("   0%CPU");

// ---- File dialog name filters ---------------------------------------------
/** name filter for LAMMPS input files */
inline const QString FILTER_INPUT = QStringLiteral("LAMMPS input files (in.* *.lmp *.txt)"
                                                   ";;All files (*)");
/** name filter for LAMMPS binary restart files */
inline const QString FILTER_RESTART = QStringLiteral("LAMMPS restart files (*.restart *.rst)"
                                                     ";;All files (*)");
/** name filter for captured log output */
inline const QString FILTER_LOG = QStringLiteral("Log files (*.log *.out *.txt);;All files (*)");
/** name filter for YAML data */
inline const QString FILTER_YAML = QStringLiteral("YAML files (*.yaml *.yml);;All files (*)");
/** name filter for CSV data */
inline const QString FILTER_CSV = QStringLiteral("CSV data (*.csv);;All files (*)");
/** name filter for gnuplot data */
inline const QString FILTER_GNUPLOT = QStringLiteral("Gnuplot data (*.dat);;All files (*)");
/** name filter for JSON settings files */
inline const QString FILTER_JSON = QStringLiteral("JSON files (*.json);;All files (*)");
/** name filter for the plottable data file formats */
inline const QString FILTER_DATA = QStringLiteral("Data files (*.dat *.csv *.yaml *.yml "
                                                  "*.json *.txt);;All files (*)");
/** name filter for the image formats supported when saving (Qt or ImageMagick writable) */
inline const QString FILTER_IMAGE = QStringLiteral("Image files (*.png *.jpg *.jpeg *.gif *.bmp "
                                                   "*.tga *.ppm *.tiff *.webp *.pgm *.xpm *.xbm)"
                                                   ";;All files (*)");
/** name filter for the movie formats supported when exporting with FFmpeg */
inline const QString FILTER_MOVIE = QStringLiteral("Movie files (*.mp4 *.m4v *.mkv *.mov *.webm "
                                                   "*.avi *.mpg *.mpeg *.gif);;All files (*)");

} // namespace Cfg

/**
 * @brief Centralized QSettings key and group names
 *
 * One named constant per persisted QSettings key so a typo becomes a compile
 * error instead of a silently mismatched (and therefore lost) setting.  The
 * string value of each constant must match the original literal exactly.
 * Like @ref Cfg, the namespace name is kept short for direct @c Keys::NAME use.
 */
namespace Keys {

// The settings-key constants below are intentionally self-describing -- each
// constant name mirrors its string value -- so they are excluded from the
// generated API docs by the conditional section below instead of carrying
// redundant per-member documentation comments. See the namespace brief above.
/// @cond

// ---- groups (QSettings::beginGroup) --------------------------------------
inline const QString GROUP_CHARTS   = QStringLiteral("charts");
inline const QString GROUP_REFORMAT = QStringLiteral("reformat");
inline const QString GROUP_SNAPSHOT = QStringLiteral("snapshot");
inline const QString GROUP_TUTORIAL = QStringLiteral("tutorial");

// ---- keys ----------------------------------------------------------------
inline const QString ACCELERATOR  = QStringLiteral("accelerator");
inline const QString ALIASES      = QStringLiteral("aliases");
inline const QString ALLFAMILY    = QStringLiteral("allfamily");
inline const QString ALLSIZE      = QStringLiteral("allsize");
inline const QString ANTIALIAS    = QStringLiteral("antialias");
inline const QString AUTOBOND     = QStringLiteral("autobond");
inline const QString AUTOMATIC    = QStringLiteral("automatic");
inline const QString AUTOSAVE     = QStringLiteral("autosave");
inline const QString AXES         = QStringLiteral("axes");
inline const QString AXESDIAM     = QStringLiteral("axesdiam");
inline const QString AXESLEN      = QStringLiteral("axeslen");
inline const QString BACKCOLOR    = QStringLiteral("backcolor");
inline const QString BACKCOLOR2   = QStringLiteral("backcolor2");
inline const QString BONDCOLOR    = QStringLiteral("bondcolor");
inline const QString BONDCOLORMAP = QStringLiteral("bondcolormap");
inline const QString BONDCUT      = QStringLiteral("bondcut");
inline const QString BONDDIAM     = QStringLiteral("bonddiam");
inline const QString BOX          = QStringLiteral("box");
inline const QString BOXCOLOR     = QStringLiteral("boxcolor");
inline const QString BOXDIAM      = QStringLiteral("boxdiam");
inline const QString CHARTX       = QStringLiteral("chartx");
inline const QString CHARTY       = QStringLiteral("charty");
inline const QString CITE         = QStringLiteral("cite");
inline const QString CMDHISTORY   = QStringLiteral("cmdhistory");
inline const QString COLOR        = QStringLiteral("color");
inline const QString COLORMAP     = QStringLiteral("colormap");
inline const QString COMMAND      = QStringLiteral("command");
inline const QString DIAMETER     = QStringLiteral("diameter");
inline const QString DOCKED       = QStringLiteral("docked");
inline const QString DOCKMAINX    = QStringLiteral("dockmainx");
inline const QString DOCKMAINY    = QStringLiteral("dockmainy");
inline const QString DOCKSPLITH   = QStringLiteral("docksplith");
inline const QString DOCKSPLITV   = QStringLiteral("docksplitv");
// The dock arrangement is a QMainWindow::saveState() byte array, and that format
// belongs to the Qt version that wrote it: restoreState() does not reliably
// reject a blob from a different feature release, it can crash on one.  So the
// key is qualified by the Qt feature version and every installed Qt keeps its
// own arrangement.  The patch level is deliberately left out, so an update
// within a feature release (6.9.0 -> 6.9.1) keeps the layout the user set up.
//
// This is the *runtime* version, not QT_VERSION_MAJOR/MINOR: the format is the
// business of the Qt that reads the blob back, and Qt stays binary compatible
// across feature releases, so a shared-library update from 6.9 to 6.10 puts a
// different Qt under an unchanged executable.  Keying on the compile-time
// version would hand that new Qt the old one's arrangement -- the very case
// this guards against.
inline const QString DOCKSTATE =
    QStringLiteral("dockstate_%1").arg(QString::fromLatin1(qVersion()).section('.', 0, 1));
// the unqualified key from before that; unlike PLUGIN_PATH_LEGACY it is dropped
// rather than kept, because there is no way to tell which Qt version wrote it
inline const QString DOCKSTATE_LEGACY = QStringLiteral("dockstate");
inline const QString DOWNLOAD_TIMEOUT = QStringLiteral("download_timeout");
inline const QString ECHO             = QStringLiteral("echo");
inline const QString ERRBRUSH         = QStringLiteral("errbrush");
inline const QString ERRWIDTH         = QStringLiteral("errwidth");
inline const QString GPUNEIGH         = QStringLiteral("gpuneigh");
inline const QString GPUPAIRONLY      = QStringLiteral("gpupaironly");
inline const QString GRID             = QStringLiteral("grid");
inline const QString HROT             = QStringLiteral("hrot");
inline const QString HTTPS_PROXY      = QStringLiteral("https_proxy");
inline const QString ID               = QStringLiteral("id");
inline const QString INTELPREC        = QStringLiteral("intelprec");
inline const QString LEGEND           = QStringLiteral("legend");
inline const QString LINTCHECK        = QStringLiteral("lintcheck");
inline const QString LOGX             = QStringLiteral("logx");
inline const QString LOGY             = QStringLiteral("logy");
inline const QString MAINX            = QStringLiteral("mainx");
inline const QString MAINY            = QStringLiteral("mainy");
inline const QString MAXIMIZED        = QStringLiteral("maximized");
inline const QString MINORGRID        = QStringLiteral("minorgrid");
inline const QString MONOFAMILY       = QStringLiteral("monofamily");
inline const QString MONOSIZE         = QStringLiteral("monosize");
inline const QString NAME             = QStringLiteral("name");
inline const QString NTHREADS         = QStringLiteral("nthreads");
// The library path is the one setting that must not be shared between builds
// from different compilers: a library built against a different C runtime
// loads and runs fine -- the plugin uses only the C API -- but its output
// silently bypasses the stdout capture, whose redirect only covers the
// runtime of this executable.  Qualifying the key by the toolchain that built
// the executable keeps an MSVC build and a MinGW build on the same machine
// from silently using each other's library.  (_MSC_VER first: clang-cl
// defines both and uses the MSVC runtime.)
#if defined(_MSC_VER)
inline const QString PLUGIN_PATH = QStringLiteral("plugin_path_msvc");
#elif defined(__clang__)
inline const QString PLUGIN_PATH = QStringLiteral("plugin_path_clang");
#else
inline const QString PLUGIN_PATH = QStringLiteral("plugin_path_gcc");
#endif
// the unqualified pre-3.1 key; read once at start-up to seed the qualified
// one, and left in place for older versions that still read it
inline const QString PLUGIN_PATH_LEGACY = QStringLiteral("plugin_path");
inline const QString RAWBRUSH           = QStringLiteral("rawbrush");
inline const QString RAWMODE            = QStringLiteral("rawmode");
inline const QString RAWPOINTSIZE       = QStringLiteral("rawpointsize");
inline const QString RAWWIDTH           = QStringLiteral("rawwidth");
inline const QString RECENT             = QStringLiteral("recent");
inline const QString REFLABELBOX        = QStringLiteral("reflabelbox");
inline const QString REFLABELDIST       = QStringLiteral("reflabeldist");
inline const QString REFLABELSIZE       = QStringLiteral("reflabelsize");
inline const QString RETURN             = QStringLiteral("return");
inline const QString SHELL              = QStringLiteral("shell");
inline const QString SHINYSTYLE         = QStringLiteral("shinystyle");
inline const QString SMOOTHBRUSH        = QStringLiteral("smoothbrush");
inline const QString SMOOTHCHOICE       = QStringLiteral("smoothchoice");
inline const QString SMOOTHMODE         = QStringLiteral("smoothmode");
inline const QString SMOOTHORDER        = QStringLiteral("smoothorder");
inline const QString SMOOTHPOINTSIZE    = QStringLiteral("smoothpointsize");
inline const QString SMOOTHWIDTH        = QStringLiteral("smoothwidth");
inline const QString SMOOTHWINDOW       = QStringLiteral("smoothwindow");
inline const QString SOLUTION           = QStringLiteral("solution");
inline const QString SSAO               = QStringLiteral("ssao");
inline const QString TITLE              = QStringLiteral("title");
inline const QString TYPE               = QStringLiteral("type");
inline const QString UPDCHART           = QStringLiteral("updchart");
inline const QString UPDFREQ            = QStringLiteral("updfreq");
inline const QString USEGRADIENT        = QStringLiteral("usegradient");
inline const QString VALUE              = QStringLiteral("value");
inline const QString VDWSTYLE           = QStringLiteral("vdwstyle");
inline const QString VIEWCHART          = QStringLiteral("viewchart");
inline const QString VIEWLOG            = QStringLiteral("viewlog");
inline const QString VIEWSLIDE          = QStringLiteral("viewslide");
inline const QString VROT               = QStringLiteral("vrot");
inline const QString WEBPAGE            = QStringLiteral("webpage");
inline const QString XSIZE              = QStringLiteral("xsize");
inline const QString YSIZE              = QStringLiteral("ysize");
inline const QString ZOOM               = QStringLiteral("zoom");
/// @endcond

} // namespace Keys

#endif // CONSTANTS_H

// Local Variables:
// c-basic-offset: 4
// End:
