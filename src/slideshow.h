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

#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include "imagecache.h"

#include <QDialog>
#include <QIcon>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

class QLabel;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QSpinBox;
class QTimer;
class LammpsGui;
class QMenuBar;
class RangeBandSlider;

/**
 * @brief Slideshow viewer for displaying sequences of images
 *
 * SlideShow provides a dialog for viewing and navigating through
 * sequences of images, typically from LAMMPS dump image commands.
 * It supports manual navigation (first/prev/next/last), automatic
 * playback with configurable timing, looping, and zoom controls.
 * Images can be exported as a movie file.
 */
class SlideShow : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param fileName Path to first image file
     * @param lammpsgui Pointer to LammpsGui for sending signals (optional;
     *                  nullptr for a standalone viewer with no live simulation)
     * @param parent Parent widget
     */
    explicit SlideShow(const QString &fileName, LammpsGui *lammpsgui = nullptr,
                       QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~SlideShow() override = default;

    SlideShow()                             = delete;
    SlideShow(const SlideShow &)            = delete;
    SlideShow(SlideShow &&)                 = delete;
    SlideShow &operator=(const SlideShow &) = delete;
    SlideShow &operator=(SlideShow &&)      = delete;

    /**
     * @brief Add an image to the slideshow sequence
     * @param filename Path to image file to add
     * @param label Text shown in place of the file name (optional)
     */
    void addImage(const QString &filename, const QString &label = QString());

    /**
     * @brief Extract the frames of a movie file and add them as images
     * @param filename Path to the movie file
     * @return Number of images added; 0 when canceled or on failure
     *
     * Probes the movie, asks the user to confirm the extraction and to select
     * a frame range and interval, and decodes the selected frames into PNG
     * files inside the image cache, where they are removed together with the
     * rest of the cache when the slide show window is closed.
     */
    int addMovie(const QString &filename);

    /**
     * @brief Number of images currently in the slideshow sequence
     */
    [[nodiscard]] int imageCount() const { return imagefiles.size(); }

    /**
     * @brief Clear all images from slideshow
     */
    void clear();

private:
    /// Build the window's File menu and append the main window's shared menus.
    void createMenuBar();

    QMenuBar *menubar = nullptr; ///< Own menu bar; hidden in the combined layout

private slots:
    void quit();             ///< Quit the entire application (via LammpsGui::quit)
    void copy();             ///< Copy image to clipboard
    void purgeCache();       ///< Discard the converted images held in the image cache
    void deleteImages();     ///< Delete image files in the selected range
    void stopRun();          ///< Stop running simulation
    void movie();            ///< Export images as movie file
    void saveCurrentImage(); ///< Save current image with zoom/flip/rotate applied
    void setDelay();         ///< Set timer delay for slideshow animation
    void first();            ///< Jump to first image
    void last();             ///< Jump to last image
    void next();             ///< Advance to next image
    void prev();             ///< Go back to previous image
    void play();             ///< Start/stop automatic playback
    void loop();             ///< Toggle looping mode
    void zoomIn();           ///< Zoom in on current image
    void zoomOut();          ///< Zoom out on current image
    void normalSize();       ///< Reset zoom to 100% and clear rotation and flips
    void doImageRotateCw();  ///< Rotate displayed image 90 degrees clockwise
    void doImageRotateCcw(); ///< Rotate displayed image 90 degrees counter-clockwise
    void doImageFlipH();     ///< Mirror displayed image horizontally
    void doImageFlipV();     ///< Mirror displayed image vertically
    void resetWindowSize();  ///< Resize window to fit the displayed image size

protected:
    void showEvent(QShowEvent *event) override; ///< Redo the initial window fit once shown

private:
    /**
     * @brief Scale the displayed image
     * @param factor Scaling factor to apply
     */
    void scaleImage(double factor);

    /**
     * @brief Load and display image at given index
     * @param idx Image index in sequence
     */
    void loadImage(int idx);

    /**
     * @brief Apply rotation and flip transformations to displayed image
     */
    void applyImageTransform();

    /**
     * @brief Auto-resize window to fit image
     */
    void adjustWindowSize();

    /**
     * @brief First image of the active range, as a 0-based index
     */
    int startIdx() const;

    /**
     * @brief Last image of the active range, as a 0-based index
     */
    int stopIdx() const;

    /**
     * @brief Push the current [Start, Stop] range to the navigation slider so
     *        it can highlight the active vs. skipped images
     */
    void updateSliderRange();

    /**
     * @brief Match the cache indicator to what the image cache currently holds
     *
     * The icon is shown in color while the cache holds anything and grayed out
     * when it is empty.  It can only be pressed when there is something to
     * discard, that is when at least one image has been converted: extracted
     * movie frames are kept, since re-creating them means running FFmpeg again.
     */
    void updateCacheIndicator();

private:
    LammpsGui *lammpsgui;       ///< Main widget pointer for receiving signals
    QString filename;           ///< Input or first image file name for default save-file names
    ImageCache cache;           ///< Converted images and extracted movie frames
    QImage image;               ///< Currently displayed image
    QImage rawImage;            ///< Raw image before transformations
    QTimer *playtimer;          ///< Timer for automatic playback
    QLabel *imageLabel;         ///< Label displaying the image
    QScrollArea *scrollArea;    ///< Scrollable area for image display
    RangeBandSlider *scrollBar; ///< Scroll bar for selecting images (highlights active range)
    QLabel *imageCounter;       ///< Label showing image count
    QLabel *imageName;          ///< Label showing image filename
    QSpinBox *startBox;         ///< First image of the active range (1-based UI value)
    QSpinBox *stopBox;          ///< Last image of the active range (1-based UI value)
    QPushButton *cacheButton;   ///< Image cache indicator, discards conversions when pressed
    QIcon cacheFullIcon;        ///< Cache indicator icon for a cache holding images
    QIcon cacheEmptyIcon;       ///< Grayed out cache indicator icon for an empty cache
    double scaleFactor = 1.0;   ///< Current zoom scale factor
    QSize lastFitSize;          ///< Scroll area size applied by the last auto-resize

    int current;             ///< Index of current image
    int maxwidth, maxheight; ///< Maximum image dimensions
    int timerDelay;          ///< delay between images when playing images
    bool doLoop;             ///< Loop playback flag
    QStringList imagefiles;  ///< List of image file paths
    QStringList imagelabels; ///< Display name of each image, parallel to imagefiles
    int imageRotation;       ///< Image rotation angle (0, 90, 180, 270)
    bool imageFlipH;         ///< Horizontal flip state
    bool imageFlipV;         ///< Vertical flip state
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
