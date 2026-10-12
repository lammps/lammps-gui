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

#include "aboutdialog.h"
#include "constants.h"
#include "helpers.h"

#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int LABEL_MARGIN = 6;
}

AboutDialog::AboutDialog(const QString &version, const QString &info, const QString &details,
                         int minwidth, QWidget *parent) :
    QDialog(parent), infoScrollArea(nullptr), detailsScrollArea(nullptr)
{
    setWindowTitle("About LAMMPS-GUI");
    setWindowIcon(QIcon(Cfg::MAIN_ICON));

    auto *mainLayout = new QVBoxLayout(this);

    // Top section: icon + version text
    auto *topLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(Cfg::MAIN_ICON).scaled(64, 64));
    iconLabel->setFixedSize(64, 64);
    topLayout->addWidget(iconLabel);
    auto *versionLabel = new QLabel(version, this);
    versionLabel->setMargin(LABEL_MARGIN);
    versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topLayout->addWidget(versionLabel, 1);
    mainLayout->addLayout(topLayout);

    // Info scroll area
    infoScrollArea = new QScrollArea(this);
    infoScrollArea->setWidgetResizable(true);
    auto *infoLabel = new QLabel(info, this);
    infoLabel->setWordWrap(false);
    infoLabel->setTextFormat(Qt::PlainText);
    infoLabel->setMargin(LABEL_MARGIN);
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoScrollArea->setWidget(infoLabel);
    mainLayout->addWidget(infoScrollArea, details.isEmpty() ? 1 : 2);

    // Details scroll area (only if details available)
    if (!details.isEmpty()) {
        auto *detailsLabel = new QLabel(details, this);
        detailsScrollArea  = new QScrollArea(this);
        detailsScrollArea->setWidgetResizable(true);
        detailsLabel->setWordWrap(false);
        detailsLabel->setTextFormat(Qt::PlainText);
        detailsLabel->setMargin(LABEL_MARGIN);
        detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        detailsLabel->setFont(monoFontFromSettings());

        detailsScrollArea->setWidget(detailsLabel);
        mainLayout->addWidget(detailsScrollArea, 1);
    }

    // Close button
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *closeButton = new QPushButton("Close", this);
    closeButton->setIcon(QIcon(":/icons/window-close.svg"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    int desiredWidth  = minwidth + 100;
    auto fsize        = QFontMetrics(infoLabel->font()).size(Qt::TextSingleLine, "LAMMPS");
    int desiredHeight = fsize.height() * (info.count('\n') + 4);

    // add space for detail display
    if (!details.isEmpty()) desiredHeight = desiredHeight * 3 / 2;

    // add space for icon and title line
    desiredWidth = std::max(desiredWidth, iconLabel->sizeHint().width());
    desiredWidth = std::max(desiredWidth, infoLabel->sizeHint().width());
    desiredWidth += 4 * LABEL_MARGIN;
    desiredWidth += infoScrollArea->verticalScrollBar()->sizeHint().width();

    // add spacer icon and close button
    desiredHeight += iconLabel->height() + closeButton->height();

    // Apply size constraints based on screen dimensions
    auto *screen = QGuiApplication::primaryScreen();
    if (screen) {
        auto screenSize = screen->availableSize();
        int maxWidth    = std::min(desiredWidth, screenSize.width() * 3 / 4);
        int maxHeight   = std::min(desiredHeight, screenSize.height() * 9 / 10);
        setMaximumSize(maxWidth, maxHeight);
        setMinimumSize(maxWidth, std::min(400, maxHeight));
        resize(maxWidth, maxHeight);
    }
}

void AboutDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // Defer auto-scroll setup to ensure layout is finalized
    QTimer::singleShot(0, this, [this]() {
        setupAutoScroll(infoScrollArea);
        if (detailsScrollArea) setupAutoScroll(detailsScrollArea);
    });
}

void AboutDialog::setupAutoScroll(QScrollArea *area)
{
    if (!area) return;
    auto *vbar = area->verticalScrollBar();
    if (!vbar || vbar->maximum() <= 0) return;

    // drop the timer from a previous showEvent(): re-showing the dialog would
    // otherwise stack timers and multiply the scroll speed
    delete area->findChild<QTimer *>("autoscroll");

    auto *scrollTimer = new QTimer(area);
    scrollTimer->setObjectName("autoscroll");
    scrollTimer->setInterval(50);

    connect(scrollTimer, &QTimer::timeout, this, [vbar, scrollTimer, this]() {
        if (vbar->value() >= vbar->maximum()) {
            scrollTimer->stop();
            // Wait 5 seconds, then reset to top
            QTimer::singleShot(5000, this, [vbar]() {
                vbar->setValue(0);
            });
        } else {
            vbar->setValue(vbar->value() + 1);
        }
    });

    // Start scrolling after 3 seconds; the timer is the context object so the
    // pending shot dies with it if the dialog is re-shown in the meantime
    QTimer::singleShot(3000, scrollTimer, [scrollTimer]() {
        scrollTimer->start();
    });
}
// Local Variables:
// c-basic-offset: 4
// End:
