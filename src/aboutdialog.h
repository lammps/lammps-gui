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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QScrollArea>

/**
 * @brief Custom About dialog for LAMMPS-GUI
 *
 * AboutDialog displays version information, LAMMPS configuration details,
 * and available styles in scrollable text areas. The dialog automatically
 * scrolls down when the content exceeds the visible area, pauses at the
 * bottom, and then returns to the top.
 * When style information is available, the dialog allocates
 * 2/3 of the combined scroll area space to the configuration information
 * text and 1/3 to the style information text.
 *
 * The style information text uses the configured fixed-width font from the
 * QSettings keys "monofamily" and "monosize" while the rest uses the
 * application's default (variable width) font.
 */

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param version   Version information text displayed at the top
     * @param info      LAMMPS configuration info displayed in a scroll area
     * @param details   Style information displayed in a scroll area with fixed-width font
     * @param minwidth  minimum width of dialog
     * @param parent Parent widget
     */
    AboutDialog(const QString &version, const QString &info, const QString &details, int minwidth,
                QWidget *parent = nullptr);

    /** @brief Destructor */
    ~AboutDialog() override = default;

    AboutDialog()                               = delete;
    AboutDialog(const AboutDialog &)            = delete;
    AboutDialog(AboutDialog &&)                 = delete;
    AboutDialog &operator=(const AboutDialog &) = delete;
    AboutDialog &operator=(AboutDialog &&)      = delete;

protected:
    /**
     * @brief Event handler for widget show events; implements the auto-scroll functionality.
     * @param event The show event
     */
    void showEvent(QShowEvent *event) override;

private:
    /**
     * @brief Configure and start the auto-scroll animation for a scroll area
     * @param area The scroll area to animate
     */
    void setupAutoScroll(QScrollArea *area);

    QScrollArea *infoScrollArea;    ///< Scroll area for LAMMPS configuration info
    QScrollArea *detailsScrollArea; ///< Scroll area for styles information (may be null)
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
