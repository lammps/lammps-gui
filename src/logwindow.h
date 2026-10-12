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

#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QPlainTextEdit>

class FlagWarnings;
class LammpsGui;
class QAction;
class QEvent;
class QLabel;
class QMenuBar;
class QResizeEvent;

/**
 * @brief Text viewer for LAMMPS log output with warning/error detection
 *
 * LogWindow specializes QPlainTextEdit for LAMMPS log viewing.
 * It highlights warnings and errors, detects embedded YAML data for
 * extraction, provides navigation between warnings, and makes error
 * URLs clickable.
 */
class LogWindow : public QPlainTextEdit {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param filename Name of the input file the run belongs to (used for default save-file names)
     * @param lammpsgui Pointer to LammpsGui for sending signals
     * @param parent Parent widget
     */
    LogWindow(const QString &filename, LammpsGui *lammpsgui, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LogWindow() override;

    /**
     * @brief Clear the window for a new run
     * @param filename Name of the input file the new run belongs to
     *
     * Discards the collected log text and the warning/error counters so the
     * window can be reused instead of destroyed and recreated for every run.
     * Keeps the position and size the window currently has on screen.
     */
    void reset(const QString &filename);

    LogWindow()                             = delete;
    LogWindow(const LogWindow &)            = delete;
    LogWindow(LogWindow &&)                 = delete;
    LogWindow &operator=(const LogWindow &) = delete;
    LogWindow &operator=(LogWindow &&)      = delete;

private slots:
    void extractYaml();  ///< Extract YAML data to separate file
    void quit();         ///< Quit the entire application (via LammpsGui::quit)
    void saveAs();       ///< Save log to file
    void stopRun();      ///< Stop running simulation
    void runBuffer();    ///< Start running simulation
    void nextWarning();  ///< Navigate to next warning
    void openErrorUrl(); ///< Open error documentation URL in browser

protected:
    /**
     * @brief Handle window close event
     * @param event Close event
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief Handle double-click to open URLs
     * @param event Mouse event
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * @brief Show context menu with log-specific actions
     * @param event Context menu event
     */
    void contextMenuEvent(QContextMenuEvent *event) override;

    /**
     * @brief Keep the fixed-width document font when the inherited font changes
     * @param event Change event
     */
    void changeEvent(QEvent *event) override;

    /**
     * @brief Keep the menu bar across the top of the viewport
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Check if log contains embedded YAML data
     * @return true if YAML data detected, false otherwise
     */
    bool checkYaml();

private:
    /// Create the window's actions, give them focus-scoped shortcuts and add
    /// them to the widget.  Called once from the constructor; contextMenuEvent()
    /// puts the same action objects into the menu it pops up, so every shortcut
    /// has exactly one binding and works whether or not the menu is open.
    void createActions();

    /// Build the File menu from those actions, append the main window's shared
    /// menus, and reserve the viewport margin the bar sits in.
    void createMenuBar();

    QString filename;            ///< Input file name used to derive default save-file names
    LammpsGui *lammpsgui;        ///< Main widget pointer for receiving signals
    QString errorurl;            ///< URL of last detected error
    FlagWarnings *warnings;      ///< Warning highlighter
    QLabel *summary;             ///< Summary label for warning count
    QMenuBar *menubar = nullptr; ///< Own menu bar, in reserved viewport margin

    QAction *saveAsAct;   ///< Save the log to a file
    QAction *yamlAct;     ///< Export embedded YAML data to a file
    QAction *urlAct;      ///< Open the error URL under the cursor in a browser
    QAction *nextWarnAct; ///< Jump to the next warning or error
    QAction *closeAct;    ///< Close the Output window
    QAction *quitAct;     ///< Quit the application
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
