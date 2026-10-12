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

#ifndef DOWNLOADPROGRESS_H
#define DOWNLOADPROGRESS_H

#include <QDialog>
#include <QString>

class QLabel;
class QPixmap;
class QProgressBar;

/**
 * @brief Splash-style transient dialog showing the progress of a batch download
 *
 * Shows a logo, a headline, a single activity line, a dedicated progress bar,
 * and a Cancel button while files are downloaded.  The bar is indeterminate
 * (busy indicator) while the number of files is not yet known (e.g. during the
 * initial fetch of a manifest) and switches to determinate per-file progress
 * afterwards.  Canceling (button, escape, or closing the dialog) emits
 * QDialog::rejected(); the caller connects that to aborting the download.
 * When the batch ends (success or failure), the caller must end the dialog
 * with accept() -- QDialog::close() implies reject() and would be
 * indistinguishable from a user cancellation.
 *
 * The setters show the dialog and process pending events, so the updated state
 * is painted even when the caller immediately blocks in a synchronous
 * download loop afterwards.
 */
class DownloadProgress : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Create the dialog
     * @param headline  Bold headline describing the batch (e.g. the tutorial name)
     * @param logo      Image shown to the left of the text (may be null)
     * @param parent    Parent widget
     */
    explicit DownloadProgress(const QString &headline, const QPixmap &logo,
                              QWidget *parent = nullptr);
    ~DownloadProgress() override = default;

    DownloadProgress()                                    = delete;
    DownloadProgress(const DownloadProgress &)            = delete;
    DownloadProgress(DownloadProgress &&)                 = delete;
    DownloadProgress &operator=(const DownloadProgress &) = delete;
    DownloadProgress &operator=(DownloadProgress &&)      = delete;

    /** @brief Show indeterminate (busy) progress with the given activity text */
    void setBusy(const QString &text);

    /**
     * @brief Show determinate progress with the given activity text
     * @param text     Current activity (e.g. name of the file being downloaded)
     * @param value    Number of completed items
     * @param maximum  Total number of items
     */
    void setProgress(const QString &text, int value, int maximum);

private:
    QLabel *message;        ///< current activity line
    QProgressBar *progress; ///< busy or per-file progress bar
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
