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

#ifndef HELPERS_H
#define HELPERS_H

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QShortcut>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <initializer_list>
#include <memory>

class QMenuBar;
class QWidget;
class QImage;
class QPixmap;
class QFont;
class QAbstractButton;
class QDialogButtonBox;
class QMessageBox;
class QScrollArea;
class QTextDocument;

// OS specific default fonts (managed via unique_ptr for automatic cleanup)
extern std::unique_ptr<QFont> GUI_MONOFONT;
extern std::unique_ptr<QFont> GUI_ALLFONT;

/**
 * @brief Build the configured fixed-width font from the application settings
 * @return Fixed-pitch QFont with the family and point size stored in the
 *         settings, falling back to the platform default GUI_MONOFONT
 */
QFont monoFontFromSettings();

/**
 * @brief Re-assert the configured fixed-width font on a text view's document
 *
 * Docked, a text view is a child of the main window and inherits its
 * proportional font; QPlainTextEdit reacts to the resulting FontChange event
 * by adopting the widget font as the document font, and the view loses its
 * fixed pitch.  A view that must keep it calls this from its changeEvent()
 * override on QEvent::FontChange.  Setting only the document font does not
 * feed back into the widget font, so this cannot recurse.
 *
 * @param document The view's text document (no-op if null)
 */
extern void reassertMonoFont(QTextDocument *document);

/**
 * @brief Compare two date strings in LAMMPS "DD MMM YYYY" format (e.g. "22 Jul 2025")
 * @param one First date string
 * @param two Second date string
 * @return -1 if one < two, 0 if equal, 1 if one > two
 */
extern int dateCompare(const QString &one, const QString &two);

/**
 * @brief Split a string into words while respecting quotes
 * @param text The string to split
 * @return List of words extracted from the string
 */
extern QStringList splitLine(const QString &text);

/**
 * @brief Apply the bundled SVG icons to a QMessageBox
 *
 * Replaces the standard large icon of the message box with the given bundled
 * SVG icon and sets the window icon and a styled standard "Ok" button
 * consistently.  Custom buttons can be added after this call.
 *
 * @param mb       Message box to style
 * @param iconPath Resource path of the SVG icon used as the large dialog icon
 */
extern void setDialogIcons(QMessageBox &mb, const QString &iconPath);

/**
 * @brief Provide standardized information dialog
 * @param parent  Pointer to parent widget
 * @param title   Information dialog title
 * @param text1   Information message part 1
 * @param text2   Information message part 2 (optional)
 */
extern void information(QWidget *parent, const QString &title, const QString &text1,
                        const QString &text2 = QString());

/**
 * @brief Provide standardized critical error dialog
 * @param parent  Pointer to parent widget
 * @param title   Error dialog title
 * @param text1   Error message summary
 * @param text2   Detailed error message (optional)
 */
extern void critical(QWidget *parent, const QString &title, const QString &text1,
                     const QString &text2 = QString());

/**
 * @brief Provide standardized custom warning dialog
 * @param parent  Pointer to parent widget
 * @param title   Warning dialog title
 * @param text1   Warning message summary
 * @param text2   Detailed warning message (optional)
 */
extern void warning(QWidget *parent, const QString &title, const QString &text1,
                    const QString &text2 = QString());

/**
 * @brief Provide platform specific name of a LAMMPS shared library
 * @return String with the filename or an empty string if compiled without plugin support
 */
extern QString getLammpsLibName();

/**
 * @brief Provide platform specific URL for downloading a LAMMPS shared library
 * @return String with the URL or an empty string if compiled without plugin support
 *         or with a compiler that is incompatible with the pre-compiled libraries (MSVC)
 */
extern QString getLammpsDownloadUrl();

/**
 * @brief Save image directly or convert with ImageMagick
 * @param parent       Pointer to parent widget
 * @param image        Pointer to image class
 * @param title        Warning dialog title if failed
 * @param defaultname  Default file name offered by the save dialog (resolved
 *                     relative to the current working directory)
 */
extern void exportImage(QWidget *parent, QImage *image, const QString &title,
                        const QString &defaultname);

/**
 * @brief Derive the default save-file name stem from an input or data file name
 * @param filename Name of the file the stem is derived from (may include a path)
 * @return the stem to build default save-file names from
 *
 * Strips any directory part, a leading "in." prefix, and any trailing known
 * file extensions (input, plottable data, log, restart, image, and movie
 * formats), so "in.melt", "melt.lmp", or "melt.lmp.txt" all yield "melt".
 * Falls back to "lammps" when nothing remains.
 */
[[nodiscard]] extern QString defaultFileStem(const QString &filename);

/**
 * @brief Append a default suffix to a file name that has no suffix
 * @param filename File name selected in a save dialog
 * @param suffix   Default suffix (without the leading dot)
 * @return the file name with the default suffix appended if it had none
 */
[[nodiscard]] extern QString ensureFileSuffix(const QString &filename, const QString &suffix);

/**
 * @brief Check if an executable is in the executable search path
 * @param exe The executable name to search for
 * @return true if executable is found, false otherwise
 *
 * Uses findExe(), so the macOS package manager fallback locations apply.
 */
[[nodiscard]] extern bool hasExe(const QString &exe);

/**
 * @brief Find an executable in the executable search path
 * @param exe The executable name to search for
 * @return Full path to the executable or an empty string when not found
 *
 * On macOS, an application launched from the Finder inherits a minimal PATH
 * without the common package manager locations, so the Homebrew (Arm and
 * Intel macs) and MacPorts binary folders are searched as a fallback.  Launch
 * external helper programs with the path returned by this function rather
 * than the bare executable name, so they are also found in that case.
 */
[[nodiscard]] extern QString findExe(const QString &exe);

/**
 * @brief Rename a file to a backup name with the Cfg::BACKUP_SUFFIX suffix
 * @param file Path of the file to rename
 * @return Path of the backup file or an empty string on failure
 *
 * An existing backup file is replaced.  When it cannot be removed (on Windows
 * a loaded shared library is locked against deletion), a numbered backup name
 * is used instead.  Windows does permit renaming a locked file, so this is
 * the way to move a loaded shared library out of the way before an update.
 * The caller is responsible for removing the backup file eventually.
 */
extern QString renameToBackup(const QString &file);

/**
 * @brief Check whether a file is (likely) an image
 * @param filename Path to the file
 * @return true if the extension is a known image type, or the file exists and
 *         QImageReader recognizes its contents as an image
 *
 * Recognizes the formats Qt can decode plus common ImageMagick-only formats
 * (e.g. tga, eps, sgi) so callers can route them through a conversion step.
 */
[[nodiscard]] extern bool isImageFile(const QString &filename);

/**
 * @brief Check whether a file is a movie (video) file
 * @param filename Path to the file
 * @return true if the extension is a known movie type, or the file is an
 *         animated GIF with more than one frame
 *
 * An animated GIF is both an image and a movie, and isImageFile() also claims
 * it.  Callers that route a file to either destination must therefore test
 * isMovieFile() first.
 */
[[nodiscard]] extern bool isMovieFile(const QString &filename);

/**
 * @brief Check whether a file is a LAMMPS binary restart file
 * @param filename Path to the file
 * @return true if the file exists and begins with the LAMMPS restart magic string
 */
[[nodiscard]] extern bool isRestartFile(const QString &filename);

/**
 * @brief Heuristic check whether a file is binary rather than text
 * @param filename Path to the file
 * @return true if the first 8 KB of the file contains a null byte
 *
 * Null bytes are essentially absent from text (ASCII, UTF-8, Latin-1) but
 * ubiquitous in binary formats (IEEE floats, packed integers, padding).
 * This is the same heuristic used by git, grep, and the POSIX file utility.
 * Returns false for files that cannot be opened (callers handle that separately).
 */
[[nodiscard]] extern bool looksLikeBinaryFile(const QString &filename);

/**
 * @brief Re-exec the current LAMMPS-GUI process in place (e.g. to reload the plugin)
 *
 * Replaces the running process with a fresh launch of the same executable.
 * On success it does not return; it returns only if the re-exec failed, so
 * callers must handle that case (typically warn and/or exit).
 */
extern void relaunchApplication();

/**
 * @brief Relaunch LAMMPS-GUI, or report the failure and exit
 *
 * Wraps `relaunchApplication()` with the handling every caller needs: when the
 * re-exec fails, the user is told that the saved settings take effect on the
 * next start and the application exits, since continuing in a state that no
 * longer matches the settings is worse than stopping.
 *
 * @param parent  Parent widget for the error dialog
 */
[[noreturn]] extern void relaunchOrExit(QWidget *parent);

/**
 * @brief Recursively delete all files in a directory
 * @param dir The directory to purge
 */
extern void purgeDirectory(const QString &dir);

/**
 * @brief Determine if the current Qt theme is light or dark
 * @return true if light theme, false if dark theme
 */
[[nodiscard]] extern bool isLightTheme();

/**
 * @brief Show a standardized "unsaved changes" confirmation dialog
 * @param parent    Pointer to the parent widget
 * @param filename  Name of the file with unsaved changes
 * @param question  Informative text explaining the context (e.g. "save before opening?")
 * @return QMessageBox::Yes, QMessageBox::No, or QMessageBox::Cancel
 *
 * Provides a consistent confirmation dialog used whenever the user may lose
 * unsaved changes (opening a new file, quitting, running LAMMPS, etc.).
 */
extern int showUnsavedChangesDialog(QWidget *parent, const QString &filename,
                                    const QString &question);

/**
 * @brief Ask before opening a file that is not what it is being opened as
 * @param parent   Pointer to the parent widget
 * @param filename File about to be opened; only its name is shown
 * @param kind     What it was expected to be, worded to follow "a": "text",
 *                 "data", "image or movie"
 * @return true if the user wants to go ahead
 *
 * For the cases where opening the wrong file is a mistake rather than an error:
 * a binary in the editor, a picture handed to the plotter.  Answering No is
 * what Return and Escape do, because the usual reason to see this is a name
 * that was mistyped or a file that was mis-picked.  Ask only when the file
 * fails the test for its kind -- a file that looks right must never produce a
 * dialog.
 */
[[nodiscard]] extern bool confirmUnexpectedFile(QWidget *parent, const QString &filename,
                                                const QString &kind);

/**
 * @brief Apply the bundled SVG icons to a dialog button box's standard buttons
 * @param box The button box whose standard buttons should be re-iconed
 *
 * Qt fetches @c QDialogButtonBox standard-button icons (Ok, Cancel, ...) via
 * @c QIcon::fromTheme(), which falls back to the desktop theme because the
 * bundled @c lammpsgui icon theme only carries the editor context-menu actions.
 * This applies our own @c dialog-ok / @c dialog-cancel / @c dialog-no /
 * @c window-close SVGs to whichever standard buttons the box contains, so every
 * dialog button looks the same regardless of platform or desktop theme.
 */
extern void styleDialogButtons(QDialogButtonBox *box);

/**
 * @brief Apply the bundled SVG icons to the standard buttons of a message box
 *
 * The `QMessageBox` counterpart of `styleDialogButtons()`: the same icons for the
 * same buttons, so every dialog of the application looks alike.  Buttons the
 * box does not have are skipped, so this is called once the standard buttons
 * are set.
 *
 * @param mb Message box with its standard buttons already set
 */
extern void styleMessageBoxButtons(QMessageBox &mb);

/**
 * @brief Silence stdout by redirecting it to the null device
 *
 * Redirects stdout to /dev/null (Unix) or NUL: (Windows) to suppress
 * all output.  Does nothing if StdCapture is currently active or if
 * stdout is already silenced.
 *
 * @note Not thread-safe.  Must only be called from the main thread.
 */
extern void silenceStdout();

/**
 * @brief Restore stdout after it was silenced
 *
 * Restores the original stdout file descriptor that was saved by
 * silenceStdout().  Does nothing if stdout is not currently silenced.
 *
 * @note Not thread-safe.  Must only be called from the main thread.
 */
extern void restoreStdout();

/**
 * @brief Check if stdout is currently silenced
 * @return true if silenceStdout() is active, false otherwise
 */
extern bool isStdoutSilenced();

/**
 * @brief Notify the silence/restore system about StdCapture state changes
 *
 * Called by StdCapture to indicate whether it is actively capturing output.
 * While capture is active, silenceStdout() becomes a no-op to avoid
 * interfering with the capture pipe.
 *
 * @param active true when StdCapture starts capturing, false when it stops
 */
extern void notifyCaptureState(bool active);

/**
 * @brief RAII guard that silences stdout for the duration of its scope
 *
 * Calls silenceStdout() on construction and restoreStdout() on destruction,
 * so stdout is restored on every exit path from the enclosing scope, including
 * early returns and exceptions. Use in place of manual silenceStdout() /
 * restoreStdout() pairs.
 *
 * @note Not thread-safe. Must only be used from the main thread.
 */
class StdoutSilencer {
public:
    StdoutSilencer() { silenceStdout(); }
    ~StdoutSilencer() { restoreStdout(); }
    StdoutSilencer(const StdoutSilencer &)            = delete;
    StdoutSilencer(StdoutSilencer &&)                 = delete;
    StdoutSilencer &operator=(const StdoutSilencer &) = delete;
    StdoutSilencer &operator=(StdoutSilencer &&)      = delete;
};

/**
 * @brief RAII guard that collects Qt log messages instead of printing them
 *
 * Installs a Qt message handler on construction and restores the previous one
 * on destruction.  Debug, info, and warning messages emitted while the guard is
 * alive are collected and can be retrieved with messages(); they are never
 * printed.  Critical and fatal messages are passed on to the previous handler,
 * since those must not be swallowed.
 *
 * The intended use is a scope whose failure is expected and handled, such as
 * asking Qt to decode an image in a format it may not support: some of Qt's
 * image format plugins print a warning for every file they reject, which would
 * otherwise be repeated for each file and each attempt.  Retrieve the collected
 * text with messages() and report it once if the operation fails for good.
 *
 * @note The Qt message handler is process-wide, so the guard also captures
 *       messages emitted by other threads while it is alive.  Keep the guarded
 *       scope short and use it only from the main thread.
 */
class QtMessageSilencer {
public:
    QtMessageSilencer();
    ~QtMessageSilencer();
    QtMessageSilencer(const QtMessageSilencer &)            = delete;
    QtMessageSilencer(QtMessageSilencer &&)                 = delete;
    QtMessageSilencer &operator=(const QtMessageSilencer &) = delete;
    QtMessageSilencer &operator=(QtMessageSilencer &&)      = delete;

    /**
     * @brief The messages collected so far, one per line
     * @return Collected text, or an empty string if nothing was captured
     */
    [[nodiscard]] QString messages() const;

private:
    static void collect(QtMsgType type, const QMessageLogContext &context, const QString &message);

    static QtMessageSilencer *active; ///< Innermost live guard, nullptr if none
    QtMessageSilencer *outer;         ///< Guard this one replaced, for nesting
    QtMessageHandler previous;        ///< Message handler to restore, may be nullptr
    QStringList collected;            ///< Captured debug, info, and warning messages
};

/**
 * @brief Fade an image into an unmistakably inactive version of itself
 * @param src Image to convert
 * @return Grayscale, low-contrast copy of @p src with the original transparency
 *
 * Removes the color and, in addition, pulls the gray levels towards
 * Cfg::GRAYSCALE_MIDPOINT, keeping only Cfg::GRAYSCALE_CONTRAST of the original
 * contrast: a merely desaturated icon retains all of its structure and still
 * reads as active next to its colored counterpart.
 */
[[nodiscard]] extern QImage grayscaleImage(const QImage &src);

/**
 * @brief Fade a pixmap into an unmistakably inactive version of itself
 * @param src Pixmap to convert
 * @return Grayscale, low-contrast copy of @p src with the original transparency
 *
 * Applies grayscaleImage() to the pixmap.  Used for the "inactive" state of a
 * status icon, so the widget does not depend on the disabled-widget visual,
 * which does not refresh reliably on all platforms (e.g. macOS 12) and cannot
 * be applied to an enabled widget at all.
 */
[[nodiscard]] extern QPixmap grayscalePixmap(const QPixmap &src);

/**
 * @brief Square size for a toolbar/status-bar button from a sample's size hint
 *
 * Implements the shared tool-button sizing policy: take the sample button's
 * minimum size hint height, enlarge it by Cfg::TOOLBAR_BUTTON_MARGIN, and
 * return a square of that side. Compute this once per button row and reuse it
 * for the row's buttons (via styleToolButtons()) and any adjacent widgets
 * (line edits, labels, spin boxes) that should share the button height.
 *
 * @param sample Representative button (typically the first in the row)
 * @return Square button size in logical pixels
 */
extern QSize toolButtonSize(const QAbstractButton *sample);

/**
 * @brief Apply the shared tool-button policy to a set of buttons
 *
 * Fixes every button to @p size (a square from toolButtonSize()) and gives it
 * the standard Cfg::TOOLBAR_ICON_SIZE icon, so only a small, uniform padding
 * remains between icon and frame. The image viewer, slide show, chart window,
 * and editor status bar all use this so their button rows look identical.
 *
 * @param size    Square button size (from toolButtonSize())
 * @param buttons Buttons to size and assign the standard icon size
 */
extern void styleToolButtons(const QSize &size, std::initializer_list<QAbstractButton *> buttons);

/**
 * @brief Apply the shared window-manager hints to a top-level output window
 *
 * Strips the dialog property (so the window is an independent top-level window,
 * not a transient that stays above its parent) and removes the minimize button.
 * The maximize button is also removed, except on macOS where it is kept because
 * removing it makes the window non-resizable there. Used for the log, chart,
 * image, slide-show, file-viewer and variables windows so they share one frame
 * policy.
 *
 * @note setWindowFlags() re-shows a hidden widget, so call this before a final
 *       hide() when the window must start hidden.
 * @param window Top-level window to adjust (no-op if null)
 */
extern void applyWindowFlags(QWidget *window);

/**
 * @brief Retire an output view's own menu bar in the combined layout
 *
 * Docked, a view does not show a menu bar of its own: the main window puts the
 * view's *File* menu into its menu bar while the view has the focus. The menu
 * bar object still exists, because it is where the menu was built, so it is
 * simply hidden -- which is enough on every platform but one.
 *
 * On macOS a QMenuBar is not a widget in the window but a handle on the
 * system-wide menu bar, and the last one to claim a window replaces the one
 * before it. Docked, both the main window's menu bar and the view's live in the
 * same window, so opening the first panel handed the system menu bar to a menu
 * bar that is hidden and empty: everything but the application menu that macOS
 * assembles itself disappeared. Detaching the view's menu bar from the platform
 * gives the window back to the main window's, and costs nothing elsewhere,
 * where a QMenuBar is an ordinary widget already.
 *
 * In the individual-window layout each view is a window of its own and claims
 * its own menu bar legitimately, so this is only for the docked case.
 *
 * @param menubar Menu bar of a docked output view (no-op if null)
 */
extern void retireViewMenuBar(QMenuBar *menubar);

/**
 * @brief Give a view window its menu bar, or retire it in the combined layout
 *
 * In the individual-windows layout the bar shows the view's own menu followed
 * by the application-wide menus of the main window, so a run can be started
 * or stopped from the view without a second set of actions to keep in step
 * (and without a second binding for their accelerators).  In the combined
 * layout the main window carries one menu bar for all views and puts the
 * view's menu at its front while the view has the focus, so the view's own bar
 * is retired instead (see `retireViewMenuBar()`).
 *
 * @param menubar The view's menu bar (no-op if null)
 * @param file    The view's own menu
 * @param shared  The main window's application-wide menus; empty standalone
 * @return true when the bar is in use, false when it was retired
 */
extern bool installViewMenuBar(QMenuBar *menubar, QMenu *file, const QList<QMenu *> &shared);

/**
 * @brief Stretch a view's menu bar across the top of the view
 *
 * A QPlainTextEdit has no layout slot for a menu bar, so the text views place
 * theirs over the viewport (with the bar's height reserved as a viewport
 * margin) and re-place it here on every resize.  Does nothing while the bar
 * is hidden, i.e. retired in the combined layout.
 *
 * @param view    The view
 * @param menubar Its menu bar
 */
extern void layoutViewMenuBar(QWidget *view, QMenuBar *menubar);

/**
 * @brief Compute the scroll area size that shows the given content, within a budget
 *
 * Pure size computation behind fitViewerWindow(). The natural size is the
 * content plus the scroll area frame, with no scroll bars. An axis whose
 * natural size exceeds the budget is clamped and gets a scroll bar, which
 * consumes @p sbext pixels of viewport on the *other* axis, so that axis is
 * enlarged accordingly (still within its budget). The result depends only on
 * the arguments -- never on the current scroll bar visibility -- so repeated
 * calls with the same input always yield the same size.
 *
 * @param content Size of the displayed content (image) in pixels
 * @param budget  Largest allowed outer scroll area size (e.g. a screen fraction)
 * @param frame   Total scroll area frame thickness (2 * frameWidth())
 * @param sbext   Scroll bar thickness (QStyle::PM_ScrollBarExtent)
 * @return Outer scroll area size that best fits the content within the budget
 */
[[nodiscard]] extern QSize viewerFitSize(const QSize &content, const QSize &budget, int frame,
                                         int sbext);

/**
 * @brief Resize a viewer window so its scroll area just fits the displayed image
 *
 * Shared auto-resize policy of the image viewer and the slide show: the scroll
 * area is sized via viewerFitSize() and the window is resized around it. The
 * scroll area's minimum size is pinned only for the duration of the resize, so
 * the user can freely shrink the window afterwards. When the computed size
 * equals @p lastFit, the window is left untouched; passing the previous return
 * value back in keeps navigating a sequence of equally-sized images from ever
 * moving or resizing the window.
 *
 * While @p window is still hidden its layout uses unpolished style metrics, so
 * the applied size is only approximate: the fit is applied but an invalid
 * QSize is returned instead of the memoized size. The viewers use that in
 * their showEvent() overrides to apply the fit once more on the shown window.
 *
 * @param window  Top-level viewer window to resize
 * @param area    Scroll area inside @p window that shows the content
 * @param content Size of the displayed content (image) in pixels
 * @param budget  Largest allowed outer scroll area size (e.g. a screen fraction)
 * @param lastFit Return value of the previous call (default QSize() initially)
 * @return The scroll area size applied, or an invalid QSize while @p window is
 *         hidden; pass this value back as @p lastFit on the next call
 */
extern QSize fitViewerWindow(QWidget *window, QScrollArea *area, const QSize &content,
                             const QSize &budget, const QSize &lastFit);

/**
 * @brief Append an action with an optional icon and a triggered() handler to a menu
 * @param menu     Menu to append the new action to
 * @param text     Action label text
 * @param icon     Resource path for the action icon (empty for no icon)
 * @param receiver Object that owns the slot/callable
 * @param slot     Member function pointer or callable invoked on trigger
 * @return The created action, for any further configuration by the caller (e.g. setData())
 *
 * Collapses the recurring "addAction() + setIcon() + connect()" idiom used when
 * building context menus and tool menus across the widget classes.
 */
template <typename Recv, typename Func>
QAction *addMenuAction(QMenu *menu, const QString &text, const QString &icon, Recv *receiver,
                       Func slot)
{
    auto *action = menu->addAction(text);
    if (!icon.isEmpty()) action->setIcon(QIcon(icon));
    QObject::connect(action, &QAction::triggered, receiver, slot);
    return action;
}

/**
 * @brief Whether the output views are docked into the main window
 * @return true when the docked layout is in effect for this session
 *
 * The layout is chosen once at startup (WindowLayout applies it), so this only
 * reads the stored preference, or what forceLayout() was given instead.
 * Widgets consult it for the things that make no sense in a dock, such as
 * remembering their own window size.
 */
extern bool dockedLayout();

/**
 * @brief Override the stored layout preference for this session
 * @param docked true for the combined main window, false for individual windows
 *
 * What the -j/--joined and -w/--windows command-line flags do.  The preference
 * itself is left alone: the choice applies to the process it was given to and
 * nothing else, which is also why a relaunch (which passes no arguments on)
 * goes back to what the preferences say.  Call before the first window is
 * built, since that is when the layout is decided.
 */
extern void forceLayout(bool docked);

/**
 * @brief Record the key sequences the main window's menus already use
 * @param keys Every shortcut reachable from the main window's menu bar
 *
 * Called once by the main window after its menus are built.  A view that ends
 * up as a dock panel lives inside that same window, so a sequence it binds
 * would match at the same time as the menu does and Qt would fire neither.
 * WindowLayout consults this when a widget actually becomes a panel; a view
 * that stays a window of its own keeps all of its shortcuts.
 */
extern void setMainWindowShortcuts(const QList<QKeySequence> &keys);

/**
 * @brief Whether a key sequence belongs to the main window's menus
 * @param keys Sequence to check
 * @return true if the main window already binds it
 */
extern bool isMainWindowShortcut(const QKeySequence &keys);

/**
 * @brief Give a menu action a keyboard shortcut that is scoped to one widget
 * @param widget Widget the shortcut belongs to
 * @param action Action to bind the shortcut to
 * @param keys   Key sequence to bind
 *
 * A menu action is only associated with the menu it was added to, and a menu
 * is a popup that never holds the keyboard focus.  Associating the action with
 * @p widget as well is therefore what makes Qt::WidgetWithChildrenShortcut
 * usable here at all: without it the shortcut would never match.  See
 * addShortcut() for why the output windows want focus scope in the first place.
 */
extern void scopeShortcut(QWidget *widget, QAction *action, const QKeySequence &keys);

/**
 * @brief Add a keyboard shortcut that is scoped to one widget
 * @param widget   Widget the shortcut belongs to and that owns the QShortcut
 * @param keys     Key sequence to bind
 * @param receiver Object that owns the slot/callable
 * @param slot     Member function pointer or callable invoked on activation
 * @return The created shortcut, for any further configuration by the caller
 *
 * The shortcut uses Qt::WidgetWithChildrenShortcut, so it fires only while the
 * keyboard focus is inside @p widget rather than anywhere in its window.  That
 * is what keeps the per-window shortcuts of the output windows (several of
 * which repeat main window accelerators such as Ctrl+S, Ctrl+Q or Ctrl+/) from
 * becoming ambiguous overloads once those windows are docked into the main
 * window instead of being windows in their own right.
 */
template <typename Recv, typename Func>
QShortcut *addShortcut(QWidget *widget, const QKeySequence &keys, Recv *receiver, Func slot)
{
    auto *shortcut = new QShortcut(keys, widget);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, receiver, slot);
    return shortcut;
}

#endif
// Local Variables:
// c-basic-offset: 4
// End:
