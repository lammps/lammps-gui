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

#ifndef MOVIEIMPORT_H
#define MOVIEIMPORT_H

#include <QDialog>
#include <QString>
#include <QStringList>

class QLabel;
class QSpinBox;
class QTimer;

/**
 * @brief Properties of the first video stream of a movie file
 *
 * Filled in by probeMovie().  When @c valid is false, @c error holds a message
 * suitable for display in a dialog and all other members are meaningless.
 */
struct MovieInfo {
    bool valid      = false; ///< True when the movie was probed successfully
    int width       = 0;     ///< Frame width in pixels
    int height      = 0;     ///< Frame height in pixels
    int frames      = 0;     ///< Number of frames in the video stream
    double fps      = 0.0;   ///< Nominal frame rate in frames per second
    double duration = 0.0;   ///< Duration of the movie in seconds
    QString error;           ///< Reason why the movie could not be probed
};

/**
 * @brief Convert an FFmpeg frame rate to a number
 * @param rate Rate as a rational ("30000/1001") or plain number ("25")
 * @return Frames per second, or 0.0 if @p rate cannot be parsed
 */
[[nodiscard]] extern double parseFrameRate(const QString &rate);

/**
 * @brief Extract the movie properties from the JSON output of ffprobe
 * @param json Output of "ffprobe -of json" for the first video stream
 * @return Movie properties; MovieInfo::frames is 0 when the frame count is not
 *         stored in the container and must be determined separately
 *
 * Separated from probeMovie() so that the parsing can be tested without
 * running ffprobe.
 */
[[nodiscard]] extern MovieInfo parseProbeOutput(const QByteArray &json);

/**
 * @brief Number of frames selected by a range and a stride
 * @param first First frame of the range
 * @param last  Last frame of the range (inclusive)
 * @param interval Stride; 1 selects every frame, 2 every other one, ...
 * @return Number of selected frames, or 0 if the arguments are inconsistent
 *
 * The frame numbers may be counted from either 0 or 1, the result is the same.
 */
[[nodiscard]] extern int selectedFrameCount(int first, int last, int interval);

/**
 * @brief Time offset of a movie frame
 * @param frame Frame number, counted from 1
 * @param info  Movie properties; frames and duration must be set
 * @return Offset of the frame in seconds, 0.0 when it cannot be computed
 */
[[nodiscard]] extern double frameToSeconds(int frame, const MovieInfo &info);

/**
 * @brief Check whether the sampled frame is far from the selected range
 * @param first       First frame of the selected range
 * @param last        Last frame of the selected range (inclusive)
 * @param samplepos   Frame number of the current sample frame
 * @param totalframes Number of frames of the whole movie
 * @return True when a frame near the middle of the selected range should be
 *         decoded to recalibrate the size estimate
 *
 * The size of a compressed frame can vary over the course of a movie, so the
 * estimate is refreshed when the middle of the selected range has moved away
 * from the sampled frame by more than a tenth of the movie.
 */
[[nodiscard]] extern bool sampleOutdated(int first, int last, int samplepos, int totalframes);

/**
 * @brief Determine the properties of a movie file by running ffprobe
 * @param filename Path to the movie file
 * @return Movie properties, with MovieInfo::valid indicating success
 */
[[nodiscard]] extern MovieInfo probeMovie(const QString &filename);

/**
 * @brief Extract selected frames of a movie into individual PNG files
 * @param parent   Parent widget of the progress dialog
 * @param filename Path to the movie file
 * @param outdir   Existing directory that receives the PNG files
 * @param first    First frame to extract, counted from 1
 * @param last     Last frame to extract (inclusive), counted from 1
 * @param interval Stride; 1 extracts every frame, 2 every other one, ...
 * @param error    Set to a message when the result is empty
 * @return Paths of the extracted PNG files in movie order, empty on failure
 *
 * Runs FFmpeg with a progress dialog that lets the user abort a long
 * extraction.  On abort or failure the partial output is removed and an empty
 * list is returned.
 */
[[nodiscard]] extern QStringList extractMovieFrames(QWidget *parent, const QString &filename,
                                                    const QString &outdir, int first, int last,
                                                    int interval, QString &error);

/**
 * @brief Dialog to confirm and configure the import of movie frames as images
 *
 * Shows the properties of the movie, lets the user pick a frame range and a
 * frame interval, and estimates how much temporary disk space the extracted
 * images will need.  The estimate is the size of a single decoded sample frame
 * times the number of selected frames.  When it exceeds Cfg::MOVIE_WARN_BYTES,
 * Cfg::MOVIE_WARN_FRAMES frames, or most of the free space on the volume
 * holding the temporary directory, a highlighted warning is displayed.
 *
 * The sample frame is shown as a thumbnail next to the movie properties.  When
 * the middle of the selected range moves away from the sampled frame (see
 * sampleOutdated()), a frame near the new middle is decoded after a short
 * delay, and the thumbnail and the size estimate are refreshed from it.
 */
class MovieImportDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param filename Path to the movie file, used for the labels only
     * @param info     Movie properties from probeMovie(); must be valid
     * @param parent   Parent widget
     */
    MovieImportDialog(const QString &filename, const MovieInfo &info, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MovieImportDialog() override = default;

    MovieImportDialog()                                     = delete;
    MovieImportDialog(const MovieImportDialog &)            = delete;
    MovieImportDialog(MovieImportDialog &&)                 = delete;
    MovieImportDialog &operator=(const MovieImportDialog &) = delete;
    MovieImportDialog &operator=(MovieImportDialog &&)      = delete;

    /**
     * @brief First selected frame, counted from 1
     */
    [[nodiscard]] int firstFrame() const;

    /**
     * @brief Last selected frame (inclusive), counted from 1
     */
    [[nodiscard]] int lastFrame() const;

    /**
     * @brief Selected frame interval; 1 selects every frame
     */
    [[nodiscard]] int frameInterval() const;

private slots:
    /**
     * @brief Recompute the frame count, the size estimate, and the warning
     *
     * Also schedules updateSample() when the selected range has moved away
     * from the frame the current estimate was calibrated with.
     */
    void updateEstimate();

    /**
     * @brief Decode a frame near the middle of the selected range
     *
     * Refreshes the thumbnail and the per-frame size for the estimate.  The
     * previous sample is kept when decoding fails.
     */
    void updateSample();

private:
    MovieInfo movieinfo;  ///< Properties of the movie to import
    QString moviefile;    ///< Path of the movie file, needed to decode samples
    qint64 samplebytes;   ///< Size of a single extracted frame, 0 if unknown
    qint64 diskfree;      ///< Free space on the temporary volume, 0 if unknown
    int samplepos;        ///< Frame number the sample was taken near, counted from 1
    QSpinBox *firstBox;   ///< First frame of the extracted range
    QSpinBox *lastBox;    ///< Last frame of the extracted range
    QSpinBox *stepBox;    ///< Interval between extracted frames
    QLabel *countLabel;   ///< Number of frames that will be extracted
    QLabel *sizeLabel;    ///< Estimated size of the extracted frames
    QLabel *noteIcon;     ///< Icon of the size estimate note, swapped when warning
    QLabel *noteLabel;    ///< Size estimate note, highlighted when warning
    QLabel *previewImage; ///< Thumbnail of the sample frame
    QLabel *previewText;  ///< Caption stating which frame the thumbnail shows
    QTimer *sampleTimer;  ///< Delays re-sampling while the range is being edited
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
