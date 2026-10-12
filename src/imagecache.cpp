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

#include "imagecache.h"

#include "helpers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdio>

ImageCache::ImageCache() :
    converted(0), subdirs(0), runs(0), cachedimages(0), cachedbytes(0), frameimages(0),
    framebytes(0)
{
}

// out of line so that the unique_ptr sees a complete QTemporaryDir
ImageCache::~ImageCache() = default;

QString ImageCache::path()
{
    if (!tmpdir) {
        tmpdir = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/lammpsgui_cacheXXXXXX");
        if (!tmpdir->isValid()) {
            tmpdir.reset();
            return {};
        }
    }
    return tmpdir->path();
}

void ImageCache::clear()
{
    entries.clear();
    tmpdir.reset();
    converted    = 0;
    subdirs      = 0;
    runs         = 0;
    cachedimages = 0;
    cachedbytes  = 0;
    frameimages  = 0;
    framebytes   = 0;
}

void ImageCache::dropConversion(const Entry &entry)
{
    if (entry.png.isEmpty()) return;
    QFile::remove(entry.png);
    cachedbytes -= entry.pngsize;
    --cachedimages;
}

void ImageCache::forget(const QString &filename)
{
    const QString key = QFileInfo(filename).absoluteFilePath();
    const auto entry  = entries.constFind(key);
    if (entry == entries.constEnd()) return;
    dropConversion(*entry);
    entries.remove(key);
}

void ImageCache::purgeConversions()
{
    for (auto entry = entries.begin(); entry != entries.end();) {
        if (entry->png.isEmpty()) {
            // the record of a file that cannot be read at all is worth keeping:
            // dropping it would only make the cache report the file once more
            ++entry;
        } else {
            dropConversion(*entry);
            entry = entries.erase(entry);
        }
    }
}

void ImageCache::registerFrames(const QString &subdir)
{
    const QDir dir(subdir);
    const auto files = dir.entryInfoList(QDir::Files);
    for (const auto &file : files) {
        ++frameimages;
        framebytes += file.size();
    }
}

QString ImageCache::makeSubDir(const QString &prefix)
{
    const QString base = path();
    if (base.isEmpty()) return {};

    // the prefix is derived from a file name and may contain path separators
    QString safe;
    for (const QChar &c : prefix)
        safe += (c.isLetterOrNumber() || (c == '_') || (c == '-')) ? c : QChar('_');
    if (safe.isEmpty()) safe = "movie";

    QDir dir(base);
    const QString name = QString("%1_%2").arg(safe).arg(++subdirs);
    if (!dir.mkpath(name)) return {};
    return dir.absoluteFilePath(name);
}

QImage ImageCache::decodeQuietly(const QString &filename, QString &qterror)
{
    // Several of Qt's image format plugins print a warning for every file they
    // reject. Rejection is the expected outcome here, since ImageMagick takes
    // over, so collect the complaints instead of letting them reach the user.
    QtMessageSilencer silencer;
    QImageReader reader(filename);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        qterror = silencer.messages();
        if (qterror.isEmpty()) qterror = reader.errorString();
    }
    return img;
}

QSize ImageCache::imageSize(const QString &filename)
{
    const QFileInfo info(filename);
    if (!info.exists()) return {};

    const auto entry = entries.constFind(info.absoluteFilePath());
    if ((entry != entries.constEnd()) && (entry->mtime == info.lastModified()) &&
        (entry->size == info.size())) {
        // a file Qt already refused has nothing to offer beyond its conversion
        if (entry->png.isEmpty()) return {};
        return QImageReader(entry->png).size();
    }

    QtMessageSilencer silencer;
    return QImageReader(filename).size();
}

QImage ImageCache::readImage(const QString &filename)
{
    const QFileInfo info(filename);
    if (!info.exists()) return {};
    const QString key = info.absoluteFilePath();

    // An entry exists only for a file Qt refused to decode. While it is fresh
    // it answers the request on its own, so neither Qt nor ImageMagick sees
    // the source file again, however often the image is displayed.
    QString pngname;
    QString qterror;
    const auto entry = entries.constFind(key);
    const bool known = (entry != entries.constEnd());
    if (known) pngname = entry->png;

    if (known && (entry->mtime == info.lastModified()) && (entry->size == info.size())) {
        if (!pngname.isEmpty()) {
            QImageReader cached(pngname);
            cached.setAutoTransform(true);
            const QImage img = cached.read();
            if (!img.isNull()) return img;
        }
        if (!entry->convertible) return {}; // nothing can read it: do not try again
        qterror = entry->qterror;
    } else {
        const QImage img = decodeQuietly(filename, qterror);
        if (!img.isNull()) {
            // Qt handles the file now, so any earlier conversion is obsolete
            if (known) {
                dropConversion(*entry);
                entries.remove(key);
            }
            return img;
        }
    }

    // a stale conversion is about to be overwritten and no longer counts
    if (known) dropConversion(*entry);

    // The file needs ImageMagick. Record what we learn about it either way, so
    // that a hopeless file is neither converted nor reported a second time.
    Entry updated{info.lastModified(), info.size(), QString(), 0, qterror, true};
    if (!hasExe("magick") && !hasExe("convert")) {
        updated.convertible = false;
        entries.insert(key, updated);
        fprintf(stderr, "Cannot read image file %s: %s\nInstall ImageMagick to convert it.\n",
                qUtf8Printable(filename), qUtf8Printable(qterror));
        return {};
    }

    const QString base = path();
    if (base.isEmpty()) return {}; // no temporary directory: retry on the next call

    // overwrite the stale PNG of a changed file rather than leaking a new one
    if (pngname.isEmpty())
        pngname = QString("%1/convert_%2.png").arg(base).arg(++converted, 5, 10, QLatin1Char('0'));

    QString cmd = findExe("magick");
    if (cmd.isEmpty()) cmd = findExe("convert");
    QImage img;
    QProcess proc;
    ++runs;
    proc.start(cmd, {filename, pngname});
    if (proc.waitForFinished(-1) && (proc.exitStatus() == QProcess::NormalExit) &&
        (proc.exitCode() == 0)) {
        QImageReader pngreader(pngname);
        pngreader.setAutoTransform(true);
        img = pngreader.read();
    }

    if (img.isNull()) {
        QFile::remove(pngname);
        updated.convertible = false;
        entries.insert(key, updated);
        fprintf(stderr, "Cannot read image file %s: %s\nConverting it with %s failed as well.\n",
                qUtf8Printable(filename), qUtf8Printable(qterror), qUtf8Printable(cmd));
        return {};
    }

    updated.png     = pngname;
    updated.pngsize = QFileInfo(pngname).size();
    entries.insert(key, updated);
    ++cachedimages;
    cachedbytes += updated.pngsize;
    return img;
}

// Local Variables:
// c-basic-offset: 4
// End:
