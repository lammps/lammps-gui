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

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QString>

#include <memory>

class QTemporaryDir;

/**
 * @brief Cache of images converted to a format that Qt can decode
 *
 * Qt reads only a subset of the image formats that LAMMPS and other tools can
 * write.  For the remaining ones (tga, eps, sgi, ...) readImage() shells out to
 * ImageMagick and converts the file to a temporary PNG.  That conversion is
 * expensive, so the PNG is kept and reused: a file is converted at most once,
 * unless it changes on disk.  Cache entries are keyed by the absolute source
 * path and validated against its modification time and size, so a file that is
 * rewritten (a growing dump image sequence, for example) is converted again.
 *
 * An entry therefore exists only for a file that Qt cannot decode, and a fresh
 * entry answers a request without handing the source file to Qt again.  That
 * matters beyond the saved work: several of Qt's image format plugins print a
 * warning for every file they reject (the TGA plugin is a notorious example),
 * which would otherwise be repeated on every single display of the image.  The
 * one unavoidable attempt is made under a QtMessageSilencer, and its output is
 * reported only if ImageMagick cannot rescue the file either.  A file that
 * cannot be read at all is remembered as such, so it is neither converted nor
 * complained about twice.
 *
 * All temporary files live in a single QTemporaryDir that is created on first
 * use and removed, with everything in it, when the cache is destroyed.  The
 * directory also hosts the frames extracted from imported movie files, for
 * which makeSubDir() hands out private subdirectories.
 *
 * The class is not thread-safe and must only be used from the GUI thread.
 */
class ImageCache {
public:
    /**
     * @brief Constructor.  The temporary directory is created lazily.
     */
    ImageCache();

    /**
     * @brief Destructor.  Removes the temporary directory and its contents.
     */
    ~ImageCache();

    ImageCache(const ImageCache &)            = delete;
    ImageCache(ImageCache &&)                 = delete;
    ImageCache &operator=(const ImageCache &) = delete;
    ImageCache &operator=(ImageCache &&)      = delete;

    /**
     * @brief Read an image file, converting it first if Qt cannot decode it
     * @param filename Path to the image file
     * @return The decoded image, or a null QImage if it cannot be read
     *
     * Files that Qt understands are decoded directly and never cached.  For the
     * others ImageMagick (@c magick or @c convert) is used, if available, and
     * the resulting PNG is cached for subsequent calls.  A file that neither Qt
     * nor ImageMagick can read is reported once, on standard error, and then
     * remembered as unreadable.
     */
    [[nodiscard]] QImage readImage(const QString &filename);

    /**
     * @brief Dimensions of an image file, without decoding its pixels
     * @param filename Path to the image file
     * @return Image size, or an invalid QSize if it cannot be determined
     *
     * Reads the header only, so it is much cheaper than readImage() when just
     * the dimensions are needed.  For a file with a cached conversion the
     * header of the converted PNG is read instead of the original, so a format
     * that Qt cannot decode is not handed to it a second time.
     */
    [[nodiscard]] QSize imageSize(const QString &filename);

    /**
     * @brief Create a private subdirectory inside the cache directory
     * @param prefix Name hint; characters outside [A-Za-z0-9_-] are replaced
     * @return Absolute path of the new directory, or an empty string on failure
     *
     * Each call creates a new directory, so two imports of the same movie do
     * not overwrite each other's frames.
     */
    [[nodiscard]] QString makeSubDir(const QString &prefix);

    /**
     * @brief Account for the files a caller has written into a subdirectory
     * @param subdir Directory from makeSubDir() that now holds extracted frames
     *
     * The cache does not create movie frames itself, so it has to be told about
     * them before it can report them in frameImages() and frameBytes().
     */
    void registerFrames(const QString &subdir);

    /**
     * @brief Drop what the cache knows about a single source file
     * @param filename Path to the source file
     *
     * Deletes its converted copy, if any.  Call this when the source file is
     * removed from disk, since its conversion is then useless and would occupy
     * the cache directory until the cache is destroyed.
     */
    void forget(const QString &filename);

    /**
     * @brief Delete every converted image, keeping the extracted movie frames
     *
     * A conversion can be redone from its source file on demand, so discarding
     * it only costs time.  Movie frames cannot be re-created without running
     * FFmpeg over the movie again and are therefore left alone, as are the
     * records of files that could not be read at all: forgetting those would
     * only make the cache report them a second time.
     */
    void purgeConversions();

    /**
     * @brief Path of the temporary cache directory, creating it if needed
     * @return Absolute path, or an empty string if the directory cannot be made
     */
    [[nodiscard]] QString path();

    /**
     * @brief Number of source files Qt cannot decode that the cache knows about
     *
     * Counts both the files with a converted copy and those remembered as
     * unreadable.  Files that Qt decodes directly are never given an entry.
     */
    [[nodiscard]] int count() const { return static_cast<int>(entries.size()); }

    /**
     * @brief Number of times ImageMagick has been run to convert a file
     *
     * A cache that works keeps this at one run per file, however often the
     * file is displayed.
     */
    [[nodiscard]] int conversions() const { return runs; }

    /**
     * @brief Number of converted images currently held in the cache directory
     */
    [[nodiscard]] int cachedImages() const { return cachedimages; }

    /**
     * @brief Total size in bytes of the converted images
     */
    [[nodiscard]] qint64 cachedBytes() const { return cachedbytes; }

    /**
     * @brief Number of extracted movie frames registered with registerFrames()
     */
    [[nodiscard]] int frameImages() const { return frameimages; }

    /**
     * @brief Total size in bytes of the extracted movie frames
     */
    [[nodiscard]] qint64 frameBytes() const { return framebytes; }

    /**
     * @brief Whether the cache directory holds any images at all
     */
    [[nodiscard]] bool isEmpty() const { return (cachedimages == 0) && (frameimages == 0); }

    /**
     * @brief Drop all cached conversions and delete the temporary directory
     */
    void clear();

private:
    /** @brief What the cache knows about a source file Qt refused to decode */
    struct Entry {
        QDateTime mtime;         ///< Modification time of the source file when examined
        qint64 size = -1;        ///< Size in bytes of the source file when examined
        QString png;             ///< Converted PNG in the cache directory, empty if none
        qint64 pngsize = 0;      ///< Size in bytes of that converted PNG
        QString qterror;         ///< What Qt said when it refused to decode the source
        bool convertible = true; ///< False once ImageMagick has failed on this file
    };

    /** @brief Decode a file with Qt, collecting rather than printing its complaints */
    static QImage decodeQuietly(const QString &filename, QString &qterror);

    /** @brief Remove a converted PNG from disk and from the usage totals */
    void dropConversion(const Entry &entry);

    std::unique_ptr<QTemporaryDir> tmpdir; ///< Cache directory, created on first use
    QHash<QString, Entry> entries;         ///< Absolute source path -> what we know about it
    int converted;                         ///< Counter for unique conversion file names
    int subdirs;                           ///< Counter for unique subdirectory names
    int runs;                              ///< Number of ImageMagick invocations
    int cachedimages;                      ///< Converted images currently on disk
    qint64 cachedbytes;                    ///< Total size of the converted images
    int frameimages;                       ///< Extracted movie frames currently on disk
    qint64 framebytes;                     ///< Total size of the extracted movie frames
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
