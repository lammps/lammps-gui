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

#ifndef FILEVIEWER_H
#define FILEVIEWER_H

#include <QPlainTextEdit>

class LammpsGui;
class QEvent;
class QMenuBar;
class QResizeEvent;

/**
 * @brief Read-only text viewer for displaying file contents
 *
 * FileViewer provides a simple read-only text window for viewing
 * file contents. It's used in the context menu of the code editor
 * to view files referenced in LAMMPS input scripts (data files,
 * potential files, etc.). Compressed files (`.gz`, `.bz2`, `.zst`, `.xz`, `.lzma`,
 * `.lz4`) are decompressed with the matching command line tool. The viewer
 * supports keyboard shortcuts for closing and stopping the simulation.
 */
class FileViewer : public QPlainTextEdit {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param filename Path to file to display
     * @param lammpsgui Pointer to the main window for stopping runs, quitting, and its shared
     *                  menus (may be null)
     * @param title Window title (derived from the file name if empty)
     * @param parent Parent widget
     */
    explicit FileViewer(const QString &filename, LammpsGui *lammpsgui, const QString &title = "",
                        QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~FileViewer() override = default;

    FileViewer()                              = delete;
    FileViewer(const FileViewer &)            = delete;
    FileViewer(FileViewer &&)                 = delete;
    FileViewer &operator=(const FileViewer &) = delete;
    FileViewer &operator=(FileViewer &&)      = delete;

private slots:
    void quit();    ///< Quit the entire application (via LammpsGui::quit)
    void stopRun(); ///< Stop the running simulation

protected:
    /**
     * @brief Keep the menu bar across the top of the viewport
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Keep the fixed-width document font when the widget font changes
     * @param event Change event
     */
    void changeEvent(QEvent *event) override;

private:
    /// Build the File menu, append the main window's shared menus, and reserve
    /// the viewport margin the bar sits in.
    void createMenuBar();

    QMenuBar *menubar = nullptr; ///< Own menu bar, in reserved viewport margin
    QString fileName;            ///< Path to the displayed file
    LammpsGui *lammpsgui;        ///< Main window for stop and quit requests; may be null
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
