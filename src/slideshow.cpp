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

#include "slideshow.h"

#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "movieimport.h"
#include "qaddon.h"
#include "rangebandslider.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QSpacerItem>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int LAYOUT_SPACING = 6;
constexpr int EXTRA_HEIGHT   = 130;
} // namespace

SlideShow::SlideShow(const QString &fileName, LammpsGui *_lammpsgui, QWidget *parent) :
    QDialog(parent), lammpsgui(_lammpsgui), filename(fileName), playtimer(nullptr),
    imageLabel(new QLabel), scrollArea(new QScrollArea), scrollBar(new RangeBandSlider),
    imageCounter(new QLabel("Image   0 /   0 :")), imageName(new QLabel("(none)")),
    startBox(new QSpinBox), stopBox(new QSpinBox), cacheButton(new QPushButton), current(0),
    maxwidth(0), maxheight(0), timerDelay(100), doLoop(true), imageRotation(0), imageFlipH(false),
    imageFlipV(false)
{
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    imageLabel->setScaledContents(false);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    scrollArea->setVisible(false);

    scrollBar->setMinimum(0);
    scrollBar->setMaximum(1);
    scrollBar->setOrientation(Qt::Horizontal);
    scrollBar->setToolTip("Select Image to display");
    connect(scrollBar, &QSlider::valueChanged, this, &SlideShow::loadImage);

    imageCounter->setFrameStyle(QFrame::Raised);
    imageCounter->setFrameShape(QFrame::Panel);
    imageCounter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    imageCounter->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    imageName->setFrameStyle(QFrame::Raised);
    imageName->setFrameShape(QFrame::Panel);
    imageName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    imageName->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    imageName->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *stoprun = new QPushButton(QIcon(":/icons/process-stop.svg"), "");
    stoprun->setToolTip("Stop running simulation");
    // shared toolbar button policy (matches the image viewer); the spin boxes
    // and image-name/counter labels below are matched to the button height
    const QSize buttonhint = toolButtonSize(stoprun);
    connect(stoprun, &QPushButton::released, this, &SlideShow::stopRun);

    imageCounter->setMinimumHeight(buttonhint.height());
    imageCounter->setMaximumHeight(buttonhint.height());
    imageName->setMinimumHeight(buttonhint.height());
    imageName->setMaximumHeight(buttonhint.height());

    // the File menu below carries these, so they are bound once each; only
    // "stop run" has no menu entry of its own
    addShortcut(this, QKeySequence(Qt::CTRL | Qt::Key_Slash), this, &SlideShow::stopRun);
    createMenuBar();

    auto *mainLayout = new QVBoxLayout;
    mainLayout->setMenuBar(menubar);
    auto *toolsLayout = new QHBoxLayout;
    auto *botLayout   = new QHBoxLayout;
    auto *navLayout   = new QHBoxLayout;

    // workaround for incorrect highlight bug on macOS
    auto *dummy = new QPushButton(QIcon(), "");
    dummy->hide();
    dummy->setMinimumSize(QSize(0, 0));
    dummy->setMaximumSize(QSize(0, 0));

    auto *tomovie = new QPushButton(QIcon(":/icons/export-movie.svg"), "");
    tomovie->setToolTip("Export to movie file");
    tomovie->setEnabled(hasExe("ffmpeg") || hasExe("magick") || hasExe("convert"));

    auto *toimage = new QPushButton(QIcon(":/icons/document-save-as.svg"), "");
    toimage->setToolTip("Export to image file");

    auto *toclip = new QPushButton(QIcon(":/icons/edit-copy.svg"), "");
    toclip->setToolTip("Copy image to clipboard");

    auto *totrash = new QPushButton(QIcon(":/icons/trash.svg"), "");
    totrash->setToolTip("Delete image files in the selected range");

    // The cache indicator is colored while the cache holds images and grayed
    // out when it is empty. Both states are given to the icon for the disabled
    // mode as well, since the icon must stay colored while the button is
    // disabled because only movie frames, which are never discarded, are held.
    const QPixmap cachePix =
        QIcon(":/icons/image-cache.svg")
            .pixmap(QSize(Cfg::TOOLBAR_ICON_SIZE, Cfg::TOOLBAR_ICON_SIZE), devicePixelRatioF());
    const QPixmap grayPix = grayscalePixmap(cachePix);
    cacheFullIcon.addPixmap(cachePix, QIcon::Normal);
    cacheFullIcon.addPixmap(cachePix, QIcon::Disabled);
    cacheEmptyIcon.addPixmap(grayPix, QIcon::Normal);
    cacheEmptyIcon.addPixmap(grayPix, QIcon::Disabled);
    cacheButton->setIcon(cacheEmptyIcon);

    // a standalone slideshow (no live simulation) has no run to stop, and must
    // not offer to delete the user's own image files
    if (!lammpsgui) {
        stoprun->hide();
        totrash->hide();
    }

    auto *empty = new QLabel("");
    empty->setMinimumSize(buttonhint);
    empty->setMaximumSize(buttonhint);

    auto dsize = QFontMetrics(QApplication::font()).size(Qt::TextSingleLine, "Delay:100");
    // need some extra space on Windows
#if defined(Q_OS_WIN32)
    dsize = dsize * 3 / 2;
#endif
    auto *delay = new QSpinBox;
    delay->setRange(10, 10000);
    delay->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    delay->setValue(timerDelay);
    delay->setObjectName("delay");
    delay->setToolTip("Set delay between images in milliseconds");
    delay->setMinimumWidth(dsize.width());
    delay->setMaximumWidth(dsize.width());
    delay->setMinimumHeight(buttonhint.height());
    delay->setMaximumHeight(buttonhint.height());

    // Start/Stop spinboxes bound the active image range for play, single
    // stepping, movie export, and deletion. Both are 1-based to match the
    // "Image N / M" counter. Stop defaults to the last image and follows the
    // growing maximum (see addImage()) until the user sets it explicitly.
    startBox->setRange(1, 1);
    startBox->setValue(1);
    startBox->setToolTip("First image of the active range for play, step, movie, and delete");
    startBox->setMinimumWidth(dsize.width());
    startBox->setMaximumWidth(dsize.width());
    startBox->setMinimumHeight(buttonhint.height());
    startBox->setMaximumHeight(buttonhint.height());

    stopBox->setRange(1, 1);
    stopBox->setValue(1);
    stopBox->setToolTip("Last image of the active range for play, step, movie, and delete");
    stopBox->setMinimumWidth(dsize.width());
    stopBox->setMaximumWidth(dsize.width());
    stopBox->setMinimumHeight(buttonhint.height());
    stopBox->setMaximumHeight(buttonhint.height());

    // keep the range ordered (Start must never exceed Stop) and mirror the
    // active range onto the navigation slider's colored band
    connect(startBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (v > stopBox->value()) stopBox->setValue(v);
        updateSliderRange();
    });
    connect(stopBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (v < startBox->value()) startBox->setValue(v);
        updateSliderRange();
    });

    auto *gofirst = new QPushButton(QIcon(":/icons/go-first.svg"), "");
    gofirst->setToolTip("Go to first image");
    auto *goprev = new QPushButton(QIcon(":/icons/go-previous-2.svg"), "");
    goprev->setToolTip("Go to previous image");
    auto *goplay = new QPushButton(QIcon(":/icons/media-playback-start-2.svg"), "");
    goplay->setToolTip("Play animation");
    goplay->setCheckable(true);
    goplay->setChecked(playtimer);
    goplay->setObjectName("play");
    auto *gonext = new QPushButton(QIcon(":/icons/go-next-2.svg"), "");
    gonext->setToolTip("Go to next image");
    auto *golast = new QPushButton(QIcon(":/icons/go-last.svg"), "");
    golast->setToolTip("Go to last image");
    auto *goloop = new QPushButton(QIcon(":/icons/media-playlist-repeat.svg"), "");
    goloop->setToolTip("Loop animation");
    goloop->setCheckable(true);
    goloop->setChecked(doLoop);

    auto *zoomin = new QPushButton(QIcon(":/icons/gtk-zoom-in.svg"), "");
    zoomin->setToolTip("Zoom in by 10 percent");
    auto *zoomout = new QPushButton(QIcon(":/icons/gtk-zoom-out.svg"), "");
    zoomout->setToolTip("Zoom out by 10 percent");
    auto *imgrotcw = new QPushButton(QIcon(":/icons/object-rotate-right.svg"), "");
    imgrotcw->setToolTip("Rotate displayed image 90<sup>o</sup> clockwise");
    auto *imgrotccw = new QPushButton(QIcon(":/icons/object-rotate-left.svg"), "");
    imgrotccw->setToolTip("Rotate displayed image 90<sup>o</sup> counter-clockwise");
    auto *imgfliph = new QPushButton(QIcon(":/icons/object-flip-horizontal.svg"), "");
    imgfliph->setToolTip("Mirror displayed image horizontally");
    auto *imgflipv = new QPushButton(QIcon(":/icons/object-flip-vertical.svg"), "");
    imgflipv->setToolTip("Mirror displayed image vertically");
    auto *normal = new QPushButton(QIcon(":/icons/gtk-zoom-fit.svg"), "");
    normal->setToolTip("Reset zoom to normal");
    auto *fitwin = new QPushButton(QIcon(":/icons/fit-window.svg"), "");
    fitwin->setToolTip("Resize window to fit the displayed image size");

    // square toolbar buttons with a snug, uniform icon (shared policy)
    styleToolButtons(buttonhint,
                     {stoprun,  tomovie,   toimage,  toclip,   totrash, cacheButton, gofirst,
                      goprev,   goplay,    gonext,   golast,   goloop,  zoomin,      zoomout,
                      imgrotcw, imgrotccw, imgfliph, imgflipv, normal,  fitwin});

    connect(tomovie, &QPushButton::released, this, &SlideShow::movie);
    connect(toimage, &QPushButton::released, this, &SlideShow::saveCurrentImage);
    connect(toclip, &QPushButton::released, this, &SlideShow::copy);
    connect(totrash, &QPushButton::released, this, &SlideShow::deleteImages);
    connect(cacheButton, &QPushButton::released, this, &SlideShow::purgeCache);
    connect(delay, &QAbstractSpinBox::editingFinished, this, &SlideShow::setDelay);

    connect(gofirst, &QPushButton::released, this, &SlideShow::first);
    connect(goprev, &QPushButton::released, this, &SlideShow::prev);
    connect(goplay, &QPushButton::released, this, &SlideShow::play);
    connect(gonext, &QPushButton::released, this, &SlideShow::next);
    connect(golast, &QPushButton::released, this, &SlideShow::last);
    connect(goloop, &QPushButton::released, this, &SlideShow::loop);
    connect(zoomin, &QPushButton::released, this, &SlideShow::zoomIn);
    connect(zoomout, &QPushButton::released, this, &SlideShow::zoomOut);
    connect(imgrotcw, &QPushButton::released, this, &SlideShow::doImageRotateCw);
    connect(imgrotccw, &QPushButton::released, this, &SlideShow::doImageRotateCcw);
    connect(imgfliph, &QPushButton::released, this, &SlideShow::doImageFlipH);
    connect(imgflipv, &QPushButton::released, this, &SlideShow::doImageFlipV);
    connect(normal, &QPushButton::released, this, &SlideShow::normalSize);
    connect(fitwin, &QPushButton::released, this, &SlideShow::resetWindowSize);
    // a docked panel is sized by its dock area, not by the image, and the main
    // window toolbar right above already carries a stop button
    if (dockedLayout()) {
        fitwin->hide();
        stoprun->hide();
    }

    toolsLayout->addWidget(tomovie, 1);
    toolsLayout->addWidget(toimage, 1);
    toolsLayout->addWidget(toclip, 1);
    toolsLayout->addWidget(totrash, 1);
    toolsLayout->addWidget(cacheButton, 1);
    toolsLayout->addWidget(empty);
    toolsLayout->addWidget(dummy);
    toolsLayout->addWidget(zoomin, 1);
    toolsLayout->addWidget(zoomout, 1);
    toolsLayout->addWidget(imgrotcw, 1);
    toolsLayout->addWidget(imgrotccw, 1);
    toolsLayout->addWidget(imgfliph, 1);
    toolsLayout->addWidget(imgflipv, 1);
    toolsLayout->addWidget(normal, 1);
    toolsLayout->addWidget(fitwin, 1);
    toolsLayout->addSpacerItem(
        new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
    toolsLayout->addWidget(stoprun);
    toolsLayout->setSizeConstraint(QLayout::SetMinimumSize);
    toolsLayout->setSpacing(LAYOUT_SPACING);

    mainLayout->addLayout(toolsLayout);
    mainLayout->addWidget(new QHline);
    mainLayout->addWidget(scrollArea, 10);

    botLayout->addWidget(goplay, 1);
    botLayout->addWidget(goloop, 1);
    botLayout->addWidget(new QLabel("Delay:"));
    botLayout->addWidget(delay, 5);
    botLayout->addWidget(new QLabel("Start:"));
    botLayout->addWidget(startBox, 5);
    botLayout->addWidget(new QLabel("Stop:"));
    botLayout->addWidget(stopBox, 5);
    botLayout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
    botLayout->addWidget(imageCounter);
    botLayout->addWidget(imageName);
    botLayout->setStretch(8, 3);
    botLayout->setSizeConstraint(QLayout::SetMinimumSize);
    botLayout->setSpacing(LAYOUT_SPACING);

    navLayout->addWidget(gofirst, 1);
    navLayout->addWidget(goprev, 1);
    navLayout->addWidget(scrollBar, 10);
    navLayout->addWidget(gonext, 1);
    navLayout->addWidget(golast, 1);
    navLayout->setSpacing(LAYOUT_SPACING);

    mainLayout->addWidget(new QHline);
    mainLayout->addLayout(botLayout);
    mainLayout->addLayout(navLayout);
    mainLayout->setSpacing(LAYOUT_SPACING);
    goplay->setFocus();

    setWindowIcon(QIcon(Cfg::MAIN_ICON));
    setWindowTitle(QString("LAMMPS-GUI - Slide Show: ") + QFileInfo(fileName).fileName());

    updateCacheIndicator();

    scrollArea->setVisible(true);
    setLayout(mainLayout);
    mainLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    adjustWindowSize();

    applyWindowFlags(this);
}

void SlideShow::addImage(const QString &filename, const QString &label)
{
    if (imagefiles.contains(filename)) return;

    // update max dimensions from header only — no full decode needed
    const QSize sz = cache.imageSize(filename);
    if (sz.isValid()) {
        maxwidth  = qMax(maxwidth, sz.width());
        maxheight = qMax(maxheight, sz.height());
    }

    const int lastidx = imagefiles.size();
    imagefiles.append(filename);
    // the directory is the same for every image of a sequence and says nothing
    // about which one this is, while the layout is held open to whatever the
    // longest label needs -- so only the name is shown, and the path it came
    // from is put in the tool tip when the image is displayed
    imagelabels.append(label.isEmpty() ? QFileInfo(filename).fileName() : label);
    scrollBar->setMaximum(lastidx);

    // Grow the active-range bounds with the sequence. If Stop was pinned to the
    // previous maximum, keep it tracking the last image; otherwise leave the
    // user's explicit choice untouched.
    const int total       = imagefiles.size();
    const bool followStop = (stopBox->value() >= stopBox->maximum());
    startBox->setMaximum(total);
    stopBox->setMaximum(total);
    if (followStop) stopBox->setValue(total);
    updateSliderRange();

    if (lammpsgui || lastidx == 0) {
        // live mode: display every incoming image; or first image in any mode
        loadImage(lastidx);
        scrollBar->setValue(lastidx);
    } else {
        // viewer mode, non-first image: dimensions already captured above;
        // update the counter total and resize without reloading the display
        imageCounter->setText(
            QString("Image %1 / %2 :").arg(current + 1, 3).arg(imagefiles.size(), 3));
        adjustWindowSize();
    }
}

int SlideShow::addMovie(const QString &filename)
{
    const MovieInfo info = probeMovie(filename);
    if (!info.valid) {
        warning(this, "Cannot Import Movie File",
                "\"" + QFileInfo(filename).fileName() + "\" cannot be imported:", info.error);
        return 0;
    }

    MovieImportDialog dialog(filename, info, this);
    if (dialog.exec() != QDialog::Accepted) return 0;

    const QString outdir = cache.makeSubDir(QFileInfo(filename).completeBaseName());
    if (outdir.isEmpty()) {
        warning(this, "Cannot Import Movie File",
                "Cannot create a temporary folder for the frames of \"" +
                    QFileInfo(filename).fileName() + "\"");
        return 0;
    }

    QString error;
    const int first    = dialog.firstFrame();
    const int interval = dialog.frameInterval();
    const QStringList frames =
        extractMovieFrames(this, filename, outdir, first, dialog.lastFrame(), interval, error);
    if (frames.isEmpty()) {
        warning(this, "Cannot Import Movie File",
                "No frames were extracted from \"" + QFileInfo(filename).fileName() + "\"", error);
        return 0;
    }

    cache.registerFrames(outdir);

    // the temporary file names carry no meaning, so show the movie file and
    // the number of the frame in the movie instead
    const QString base = QFileInfo(filename).fileName();
    for (int i = 0; i < frames.size(); ++i)
        addImage(frames[i], QString("%1 [frame %2]").arg(base).arg(first + i * interval));
    updateCacheIndicator();
    return frames.size();
}

void SlideShow::deleteImages()
{
    const int lo = startIdx();
    const int hi = stopIdx();
    if ((lo < 0) || (hi < lo) || (hi >= imagefiles.size())) return;

    const int count = hi - lo + 1;
    QMessageBox mb(this);
    mb.setWindowTitle("Delete Images");
    mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
    mb.setText(QString("Delete %1 image file%2 (image %3 to %4) from disk?")
                   .arg(count)
                   .arg(count == 1 ? "" : "s")
                   .arg(lo + 1)
                   .arg(hi + 1));
    mb.setInformativeText("This operation cannot be undone.");
    mb.setIconPixmap(QIcon(":/icons/warning.svg").pixmap(QSize(64, 64), mb.devicePixelRatioF()));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    mb.setDefaultButton(QMessageBox::No);
    mb.setEscapeButton(QMessageBox::No);

    auto *button = mb.button(QMessageBox::Yes);
    button->setIcon(QIcon(":/icons/dialog-ok.svg"));
    button = mb.button(QMessageBox::No);
    button->setIcon(QIcon(":/icons/dialog-no.svg"));

    if (mb.exec() != QMessageBox::Yes) return;

    // remove back-to-front so the lower indices stay valid while deleting
    for (int i = hi; i >= lo; --i) {
        // the conversion of a deleted file is useless and must not linger on
        cache.forget(imagefiles[i]);
        QFile::remove(imagefiles[i]);
        imagefiles.removeAt(i);
        imagelabels.removeAt(i);
    }
    updateCacheIndicator();

    // nothing left: reset to the empty state
    if (imagefiles.isEmpty()) {
        clear();
        return;
    }

    // resync navigation and reset the active range to the full remaining set
    const int total = imagefiles.size();
    scrollBar->setMaximum(total - 1);
    startBox->setMaximum(total);
    stopBox->setMaximum(total);
    startBox->setValue(1);
    stopBox->setValue(total);
    updateSliderRange();

    current = std::clamp(current, 0, total - 1);
    loadImage(current);
}

void SlideShow::clear()
{
    imagefiles.clear();
    imagelabels.clear();
    image.fill(Qt::black);
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->resize(image.width(), image.height());
    // forget the old sequence's dimensions so the next one sizes the window fresh
    maxwidth    = 0;
    maxheight   = 0;
    lastFitSize = QSize();
    imageCounter->setText("Image   0 /   0 :");
    imageName->setText("(none)");
    imageName->setToolTip(QString());
    scrollBar->setMaximum(1);
    startBox->setRange(1, 1);
    startBox->setValue(1);
    stopBox->setRange(1, 1);
    stopBox->setValue(1);
    updateSliderRange();
    updateCacheIndicator();
    repaint();
}

void SlideShow::setDelay()
{
    auto *field = qobject_cast<QSpinBox *>(sender());
    if (field && (field->objectName() == "delay")) {
        timerDelay = field->value();
    }
}

int SlideShow::startIdx() const
{
    return startBox->value() - 1;
}

int SlideShow::stopIdx() const
{
    return stopBox->value() - 1;
}

void SlideShow::updateSliderRange()
{
    scrollBar->setActiveRange(startIdx(), stopIdx());
}

void SlideShow::updateCacheIndicator()
{
    const int images = cache.cachedImages();
    const int frames = cache.frameImages();

    cacheButton->setIcon(cache.isEmpty() ? cacheEmptyIcon : cacheFullIcon);
    // only the converted images can be discarded, so there is nothing to press
    // for when the cache holds movie frames alone
    cacheButton->setEnabled(images > 0);

    if (cache.isEmpty()) {
        cacheButton->setToolTip("The image cache is empty");
        return;
    }

    QStringList held;
    if (images > 0)
        held << QString("%1 converted image%2 (%3)")
                    .arg(images)
                    .arg(images == 1 ? "" : "s", locale().formattedDataSize(cache.cachedBytes()));
    if (frames > 0)
        held << QString("%1 movie frame%2 (%3)")
                    .arg(frames)
                    .arg(frames == 1 ? "" : "s", locale().formattedDataSize(cache.frameBytes()));

    QString tip = "Image cache: " + held.join(", ") + ".\n";
    if (images > 0)
        tip += "Click to discard the converted images. They are converted again when needed.";
    else
        tip += "Movie frames are kept until this window is closed.";
    cacheButton->setToolTip(tip);
}

void SlideShow::purgeCache()
{
    const int images = cache.cachedImages();
    if (images < 1) return;

    QMessageBox mb(this);
    mb.setWindowTitle("Discard Converted Images");
    mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
    mb.setText(QString("Discard %1 converted image%2 (%3) from the image cache?")
                   .arg(images)
                   .arg(images == 1 ? "" : "s", locale().formattedDataSize(cache.cachedBytes())));
    mb.setInformativeText("The original image files are not touched.  Each of them is converted "
                          "again the next time it is displayed.");
    mb.setIconPixmap(
        QIcon(":/icons/image-cache.svg").pixmap(QSize(64, 64), mb.devicePixelRatioF()));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    mb.setDefaultButton(QMessageBox::Yes);
    mb.setEscapeButton(QMessageBox::No);

    auto *button = mb.button(QMessageBox::Yes);
    button->setIcon(QIcon(":/icons/dialog-ok.svg"));
    button = mb.button(QMessageBox::No);
    button->setIcon(QIcon(":/icons/dialog-no.svg"));

    if (mb.exec() != QMessageBox::Yes) return;

    // the image on display was decoded into memory and stays valid
    cache.purgeConversions();
    updateCacheIndicator();
}

void SlideShow::loadImage(int idx)
{
    if ((idx < 0) || (idx >= imagefiles.size())) return;

    do {
        const QImage newImage = cache.readImage(imagefiles[idx]);

        // There was an error reading the image file. Try reading the previous image instead.
        if (newImage.isNull()) {
            --idx;
        } else {
            rawImage  = newImage;
            maxwidth  = qMax(maxwidth, newImage.width());
            maxheight = qMax(maxheight, newImage.height());
            applyImageTransform();
            imageCounter->setText(
                QString("Image %1 / %2 :").arg(idx + 1, 3).arg(imagefiles.size(), 3));
            imageName->setText(imagelabels[idx]);
            imageName->setToolTip(imagefiles[idx]);
            current = idx;
            break;
        }
    } while (idx >= 0);
    scrollBar->setValue(idx);
    adjustWindowSize();
    // a display may have converted the image and thus filled the cache
    updateCacheIndicator();
}

void SlideShow::copy()
{
#if QT_CONFIG(clipboard)
    auto *clip = QGuiApplication::clipboard();
    if (clip && !image.isNull()) {
        clip->setImage(image, QClipboard::Clipboard);
        if (clip->supportsSelection()) clip->setImage(image, QClipboard::Selection);
    } else
        fprintf(stderr, "Copy image to clipboard currently not available\n");
#else
    fprintf(stderr, "Copy image to clipboard not supported on this platform\n");
#endif
}

// Its own File menu plus the application-wide menus of the main window, so a
// run can be started or stopped from here as well.  Docked, the main window
// shows the File menu for us and this bar stays hidden.
void SlideShow::createMenuBar()
{
    menubar    = new QMenuBar;
    auto *file = new QMenu("&File", menubar);
    file->setObjectName(Cfg::VIEW_FILE_MENU);

    scopeShortcut(this,
                  addMenuAction(file, "&Save Image As...", ":/icons/document-save-as.svg", this,
                                &SlideShow::saveCurrentImage),
                  QKeySequence(Qt::CTRL | Qt::Key_S));
    scopeShortcut(
        this, addMenuAction(file, "&Copy Image", ":/icons/edit-copy.svg", this, &SlideShow::copy),
        QKeySequence(Qt::CTRL | Qt::Key_C));
    scopeShortcut(this,
                  addMenuAction(file, "&Export Movie...", ":/icons/export-movie.svg", this,
                                &SlideShow::movie),
                  QKeySequence(Qt::CTRL | Qt::Key_E));
    file->addSeparator();
    scopeShortcut(this,
                  addMenuAction(file, "&Close", ":/icons/window-close.svg", this, &QWidget::close),
                  QKeySequence(Qt::CTRL | Qt::Key_W));
    auto *quitAct =
        addMenuAction(file, "&Quit", ":/icons/application-exit.svg", this, &SlideShow::quit);
    scopeShortcut(this, quitAct, QKeySequence(Qt::CTRL | Qt::Key_Q));
    if (!lammpsgui) quitAct->setVisible(false); // quit == close in standalone mode

    installViewMenuBar(menubar, file, lammpsgui ? lammpsgui->sharedMenus() : QList<QMenu *>());
}

void SlideShow::quit()
{
    if (lammpsgui) lammpsgui->quit();
}

void SlideShow::stopRun()
{
    if (lammpsgui) lammpsgui->stopRun();
}

void SlideShow::saveCurrentImage()
{
    exportImage(this, &image, "SlideShow", defaultFileStem(filename) + ".png");
}

void SlideShow::movie()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export to Movie File",
        QDir::current().absoluteFilePath(defaultFileStem(filename) + ".mp4"), Cfg::FILTER_MOVIE);
    if (fileName.isEmpty()) return;
    fileName = ensureFileSuffix(fileName, "mp4");

    // restrict the exported frames to the active [Start, Stop] range
    const int lo = startIdx();
    const int hi = stopIdx();
    if ((lo < 0) || (hi < lo) || (hi >= imagefiles.size())) return;
    const QStringList frames = imagefiles.mid(lo, hi - lo + 1);

    if (hasExe("ffmpeg")) {
        QDir curdir(".");
        QTemporaryFile concatfile;
        if (concatfile.open()) {
            for (const auto &img : frames) {
                // the concat demuxer resolves any entry without a protocol prefix relative
                // to the list file's directory; a Windows drive letter is not a protocol,
                // so absolute C:/... paths would be mangled into <tempdir>/C:/...
                // An explicit file: URL is always taken verbatim.  FFmpeg expects UTF-8
                // and single quotes in the path must be escaped shell-style.
                QString entry = curdir.absoluteFilePath(img);
                entry.replace('\'', "'\\''");
                concatfile.write("file 'file:");
                concatfile.write(entry.toUtf8());
                concatfile.write("'\n");
            }
            concatfile.close();

            const auto fps = QString::number(1.0 / (static_cast<double>(timerDelay) / 1000.0));
            // construct command line
            QStringList args;
            args << "-y" << "-safe" << "0" << "-r" << fps << "-f" << "concat";
            args << "-i" << concatfile.fileName();

            // apply scaling and rotating/flipping
            QString filters;
            if (scaleFactor != 1.0) filters += QString("scale=iw*%1:-1,").arg(scaleFactor);
            if (imageRotation == 90.0) {
                filters += "transpose=1,";
            } else if (imageRotation == 180.0) {
                filters += "transpose=1,transpose=1,";
            } else if (imageRotation == 270.0) {
                filters += "transpose=2,";
            }
            if (imageFlipH) filters += "hflip,";
            if (imageFlipV) filters += "vflip,";
            if (!filters.isEmpty()) {
                // chop off trailing comma
                filters.resize(filters.size() - 1);
                args << "-vf" << filters;
            }

            // set encoder explicitly and tune settings based on file name extension
            if (fileName.endsWith(".mp4") || fileName.endsWith(".mkv"))
                args << "-c:v" << "libx264" << "-preset" << "slow" << "-crf" << "22"
                     << "-tune" << "animation" << "-pix_fmt" << "yuv420p";
            // VP9 must set bitrate to 0 to enable constant quality setting
            if (fileName.endsWith(".webm"))
                args << "-c:v" << "libvpx-vp9" << "-crf" << "24" << "-row-mt" << "1"
                     << "-pix_fmt" << "yuv420p" << "-b:v" << "0";
            else
                args << "-b:v" << "2M";

            // set bitrate and pixel format for decent quality and maximum compatibility
            args << "-r" << fps;
            // set metadata
            args << "-metadata" << "encoding_tool=LAMMPS-GUI v" LAMMPS_GUI_VERSION;

            args << fileName;

            QProcess ffmpeg;
            ffmpeg.start(findExe("ffmpeg"), args);
            ffmpeg.waitForFinished(-1);
            if (ffmpeg.exitCode()) {
                auto err = ffmpeg.readAllStandardError();
                // trim off the verbose FFMpeg configuration dump and skip to the error message
                int eol = err.indexOf("Error");
                if (eol > 0) err.replace(0, eol, "");
                critical(this, "Movie Creation Error", "FFMpeg returned:", err);
            }
        } else {
            warning(this, "SlideShow Error",
                    "Cannot create temporary file for generating movie:", concatfile.errorString());
        }
    } else {
        QString cmd = findExe("magick");
        if (cmd.isEmpty()) cmd = findExe("convert");
        QStringList args;
        args << "-delay" << QString::number(timerDelay / 10);
        QDir curdir(".");
        for (const auto &img : frames)
            args << curdir.absoluteFilePath(img);
        if (scaleFactor != 1.0) args << "-resize" << QString("%1%%").arg(100.0 * scaleFactor);
        if (imageRotation != 0.0) args << "-rotate" << QString("%1").arg(imageRotation);
        if (imageFlipH) args << "-flop";
        if (imageFlipV) args << "-flip";
        args << fileName;

        // run the conversion command
        QProcess convert;
        convert.start(cmd, args);
        convert.waitForFinished(-1);
        if (convert.exitCode()) {
            auto err = convert.readAllStandardError();
            int eol  = err.indexOf("Error");
            if (eol > 0) err.replace(0, eol, "");
            critical(this, "Movie Creation Error", "ImageMagick returned:", err);
        }
    }
}

void SlideShow::first()
{
    current = startIdx();
    loadImage(current);
}

void SlideShow::last()
{
    current = stopIdx();
    loadImage(current);
}

void SlideShow::play()
{
    // if we do not loop, start animation from beginning of the active range
    if (!doLoop) current = startIdx();
    auto *delay = findChild<QSpinBox *>("delay");

    if (playtimer) {
        playtimer->stop();
        delete playtimer;
        playtimer = nullptr;
        if (delay) delay->setEnabled(true);
    } else {
        playtimer = new QTimer(this);
        connect(playtimer, &QTimer::timeout, this, &SlideShow::next);
        playtimer->start(timerDelay);
        if (delay) delay->setEnabled(false);
    }

    // reset push button state. use findChild() if not triggered from button.
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) button = findChild<QPushButton *>("play");
    if (button) button->setChecked(playtimer);
}

void SlideShow::next()
{
    const int lo = startIdx();
    const int hi = stopIdx();
    ++current;
    if (current > hi) {
        if (doLoop) {
            current = lo;
        } else {
            // stop animation at the end of the active range
            if (playtimer) play();
            current = hi;
        }
    }
    loadImage(current);
}

void SlideShow::prev()
{
    const int lo = startIdx();
    const int hi = stopIdx();
    --current;
    if (current < lo) {
        if (doLoop)
            current = hi;
        else
            current = lo;
    }
    loadImage(current);
}

void SlideShow::loop()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    doLoop       = !doLoop;
    if (button) button->setChecked(doLoop);
}

void SlideShow::zoomIn()
{
    scaleImage(1.1);
}

void SlideShow::zoomOut()
{
    scaleImage(0.9);
}

void SlideShow::normalSize()
{
    scaleFactor   = 1.0;
    imageRotation = 0;
    imageFlipH    = false;
    imageFlipV    = false;
    loadImage(current);
}

void SlideShow::scaleImage(double factor)
{
    scaleFactor *= factor;
    // don't let the image become smaller than 10%
    scaleFactor = std::max(scaleFactor, 0.1);

    loadImage(current);
}

void SlideShow::adjustWindowSize()
{
    // A docked panel is sized by its dock area.  Fitting the window around the
    // image resizes this widget, and that request travels up through the dock to
    // the main window -- which then jumps about as images are loaded.
    if (dockedLayout()) return;

    if (maxwidth == 0 || maxheight == 0) return;

    // size of the largest image as displayed, i.e. with the current rotation
    // and zoom applied the same way as applyImageTransform() applies them
    QSize content(maxwidth, maxheight);
    if ((imageRotation == 90) || (imageRotation == 270)) content.transpose();
    content.setWidth(static_cast<int>(content.width() * scaleFactor));
    content.setHeight(static_cast<int>(content.height() * scaleFactor));

    // make sure the scroll area is not resized beyond a certain fraction of the screen
    const QSize avail = screen()->availableSize();
    const QSize budget(avail.width() * 3 / 4, (avail.height() * 9 / 10) - EXTRA_HEIGHT);
    lastFitSize = fitViewerWindow(this, scrollArea, content, budget, lastFitSize);
}

void SlideShow::resetWindowSize()
{
    // discard both a manual window resize and the memoized fit
    lastFitSize = QSize();
    adjustWindowSize();
}

void SlideShow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // any fit computed while the window was hidden used unpolished style
    // metrics and was not memoized (see fitViewerWindow()); apply the fit
    // again as soon as the shown window has settled
    if (!lastFitSize.isValid()) QTimer::singleShot(0, this, &SlideShow::adjustWindowSize);
}

void SlideShow::doImageRotateCw()
{
    imageRotation = (imageRotation + 90) % 360;
    applyImageTransform();
    adjustWindowSize();
}

void SlideShow::doImageRotateCcw()
{
    imageRotation = (imageRotation + 270) % 360;
    applyImageTransform();
    adjustWindowSize();
}

void SlideShow::doImageFlipH()
{
    imageFlipH = !imageFlipH;
    applyImageTransform();
    adjustWindowSize();
}

void SlideShow::doImageFlipV()
{
    imageFlipV = !imageFlipV;
    applyImageTransform();
    adjustWindowSize();
}

void SlideShow::applyImageTransform()
{
    // If no raw image is available yet, use the current image
    if (rawImage.isNull()) {
        if (!image.isNull()) {
            rawImage = image;
        } else {
            return;
        }
    }

    QImage transformedImage = rawImage;

    // Apply rotation
    if (imageRotation != 0) {
        QTransform transform;
        transform.rotate(imageRotation);
        transformedImage = transformedImage.transformed(transform, Qt::SmoothTransformation);
    }

    // Apply horizontal flip
    if (imageFlipH) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        transformedImage = transformedImage.flipped(Qt::Horizontal);
#else
        transformedImage = transformedImage.mirrored(true, false);
#endif
    }

    // Apply vertical flip
    if (imageFlipV) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        transformedImage = transformedImage.flipped(Qt::Vertical);
#else
        transformedImage = transformedImage.mirrored(false, true);
#endif
    }

    // Scale the transformed image
    int newheight = transformedImage.height() * scaleFactor;
    int newwidth  = transformedImage.width() * scaleFactor;
    image         = transformedImage.scaled(newwidth, newheight, Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation);
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->adjustSize();
}

// Local Variables:
// c-basic-offset: 4
// End:
