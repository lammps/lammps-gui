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

#include "downloadprogress.h"

#include "constants.h"
#include "helpers.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QVBoxLayout>

DownloadProgress::DownloadProgress(const QString &headline, const QPixmap &logo, QWidget *parent) :
    QDialog(parent), message(new QLabel), progress(new QProgressBar)
{
    setWindowTitle("LAMMPS-GUI - Download");
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(Cfg::DOWNLOAD_DIALOG_WIDTH);

    auto *hlayout = new QHBoxLayout(this);
    auto *pixmap  = new QLabel;
    if (!logo.isNull())
        pixmap->setPixmap(logo.scaled(Cfg::DOWNLOAD_DIALOG_LOGO_SIZE,
                                      Cfg::DOWNLOAD_DIALOG_LOGO_SIZE, Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
    hlayout->addWidget(pixmap);

    auto *vlayout = new QVBoxLayout;
    vlayout->addWidget(new QLabel(QString("<b>%1</b>").arg(headline)));
    message->setTextFormat(Qt::PlainText);
    vlayout->addWidget(message);
    vlayout->addWidget(progress);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    styleDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vlayout->addWidget(buttons);
    hlayout->addLayout(vlayout);
    setLayout(hlayout);
}

void DownloadProgress::setBusy(const QString &text)
{
    message->setText(text);
    progress->setRange(0, 0);
    // paint the new state now: the caller blocks in a synchronous download next
    show();
    QCoreApplication::processEvents();
}

void DownloadProgress::setProgress(const QString &text, int value, int maximum)
{
    message->setText(text);
    progress->setRange(0, maximum);
    progress->setValue(value);
    show();
    QCoreApplication::processEvents();
}

// Local Variables:
// c-basic-offset: 4
// End:
