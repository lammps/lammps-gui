// Unit tests for the converted-image cache (src/imagecache.cpp).
//
// The cache only engages for image formats that Qt cannot decode, so the
// conversion tests build their fixtures in ImageMagick's own MIFF format and
// are skipped when neither "magick" nor "convert" is in the executable search
// path; the tests that do not convert anything always run.

#include "imagecache.h"

#include "helpers.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QTemporaryDir>

#include "gtest/gtest.h"

namespace {

bool haveImageMagick()
{
    return hasExe("magick") || hasExe("convert");
}

QString magickExe()
{
    return hasExe("magick") ? "magick" : "convert";
}

// write a solid color image of the given size to a file, converting it to
// whatever format the file name suffix asks for
bool writeImage(const QString &filename, int width, int height, const QColor &color)
{
    QTemporaryDir dir;
    if (!dir.isValid()) return false;
    const QString pngname = dir.filePath("source.png");

    QImage img(width, height, QImage::Format_RGB32);
    img.fill(color);
    if (!img.save(pngname)) return false;
    if (filename.endsWith(".png")) return QFile::copy(pngname, filename);

    QProcess proc;
    proc.start(magickExe(), {pngname, filename});
    return proc.waitForFinished(30000) && (proc.exitStatus() == QProcess::NormalExit) &&
           (proc.exitCode() == 0);
}

// number of conversions the cache has written to its temporary directory
int convertedFiles(ImageCache &cache)
{
    const QString path = cache.path();
    if (path.isEmpty()) return -1;
    return QDir(path).entryList({"convert_*.png"}, QDir::Files).size();
}

} // namespace

TEST(ImageCache, QtReadableFormatIsNotCached)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString png = dir.filePath("plain.png");

    QImage img(8, 8, QImage::Format_RGB32);
    img.fill(Qt::red);
    ASSERT_TRUE(img.save(png));

    ImageCache cache;
    const QImage read = cache.readImage(png);
    EXPECT_FALSE(read.isNull());
    EXPECT_EQ(read.width(), 8);
    // Qt decodes PNG directly, so nothing is converted and nothing is cached
    EXPECT_EQ(cache.count(), 0);
    EXPECT_EQ(cache.conversions(), 0);
    EXPECT_EQ(cache.imageSize(png), QSize(8, 8));
}

TEST(ImageCache, MissingFileReturnsNullImage)
{
    ImageCache cache;
    EXPECT_TRUE(cache.readImage("/nonexistent/directory/missing.png").isNull());
    EXPECT_EQ(cache.count(), 0);
}

TEST(ImageCache, ExoticFormatIsConvertedOnlyOnce)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString miff = dir.filePath("image.miff");
    ASSERT_TRUE(writeImage(miff, 16, 12, Qt::green));
    // the premise of the test: Qt must not be able to decode this file
    ASSERT_TRUE(QImageReader(miff).read().isNull());

    ImageCache cache;
    const QImage first = cache.readImage(miff);
    ASSERT_FALSE(first.isNull());
    EXPECT_EQ(first.width(), 16);
    EXPECT_EQ(first.height(), 12);
    EXPECT_EQ(cache.count(), 1);
    EXPECT_EQ(cache.conversions(), 1);
    EXPECT_EQ(convertedFiles(cache), 1);

    // reading it again must reuse the converted PNG instead of running
    // ImageMagick a second time, which would leave a second file behind
    for (int i = 0; i < 5; ++i) {
        const QImage again = cache.readImage(miff);
        ASSERT_FALSE(again.isNull());
        EXPECT_EQ(again.size(), first.size());
    }
    EXPECT_EQ(cache.count(), 1);
    EXPECT_EQ(cache.conversions(), 1);
    EXPECT_EQ(convertedFiles(cache), 1);

    // the dimensions now come from the converted PNG, so the source file is
    // never handed to Qt again -- that is what keeps its plugins from
    // complaining about the unsupported format once per display
    EXPECT_EQ(cache.imageSize(miff), QSize(16, 12));
    EXPECT_EQ(cache.conversions(), 1);
}

TEST(ImageCache, UnreadableFileIsReportedOnceAndNotRetried)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString junk = dir.filePath("garbage.miff");
    QFile file(junk);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write(QByteArray(512, '\x17')), 0);
    file.close();

    ImageCache cache;
    // neither Qt nor ImageMagick can make sense of this, so it stays unreadable
    EXPECT_TRUE(cache.readImage(junk).isNull());
    EXPECT_EQ(cache.count(), 1);
    EXPECT_EQ(cache.conversions(), 1);

    // and the failure is remembered: no second conversion attempt, and no
    // second complaint on standard error
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(cache.readImage(junk).isNull());
    EXPECT_EQ(cache.count(), 1);
    EXPECT_EQ(cache.conversions(), 1);
    EXPECT_EQ(convertedFiles(cache), 0);

    // an unreadable file has no dimensions to report either, and asking for
    // them must not go back to Qt
    EXPECT_FALSE(cache.imageSize(junk).isValid());
    EXPECT_EQ(cache.conversions(), 1);
}

TEST(ImageCache, RepairedFileIsReadAgain)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("broken.miff");
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write(QByteArray(512, '\x17')), 0);
    file.close();

    ImageCache cache;
    ASSERT_TRUE(cache.readImage(path).isNull());
    EXPECT_EQ(cache.conversions(), 1);

    // a remembered failure must not outlive the file it was recorded for
    ASSERT_TRUE(QFile::remove(path));
    ASSERT_TRUE(writeImage(path, 20, 10, Qt::yellow));

    const QImage fixed = cache.readImage(path);
    ASSERT_FALSE(fixed.isNull());
    EXPECT_EQ(fixed.size(), QSize(20, 10));
    EXPECT_EQ(cache.conversions(), 2);
}

TEST(ImageCache, ChangedSourceFileIsConvertedAgain)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString miff = dir.filePath("image.miff");
    ASSERT_TRUE(writeImage(miff, 16, 12, Qt::green));

    ImageCache cache;
    const QImage first = cache.readImage(miff);
    ASSERT_FALSE(first.isNull());
    EXPECT_EQ(first.size(), QSize(16, 12));

    // rewrite the source with a different image: its size and modification
    // time both change, so the cached conversion must be discarded
    ASSERT_TRUE(QFile::remove(miff));
    ASSERT_TRUE(writeImage(miff, 32, 24, Qt::blue));

    const QImage second = cache.readImage(miff);
    ASSERT_FALSE(second.isNull());
    EXPECT_EQ(second.size(), QSize(32, 24));
    // the stale PNG is overwritten rather than leaked, so still one of each
    EXPECT_EQ(cache.count(), 1);
    EXPECT_EQ(convertedFiles(cache), 1);
    EXPECT_EQ(cache.conversions(), 2);
}

TEST(ImageCache, UsageTotalsTrackTheConvertedImages)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString one = dir.filePath("one.miff");
    const QString two = dir.filePath("two.miff");
    ASSERT_TRUE(writeImage(one, 16, 12, Qt::green));
    ASSERT_TRUE(writeImage(two, 16, 12, Qt::blue));

    ImageCache cache;
    EXPECT_TRUE(cache.isEmpty());
    EXPECT_EQ(cache.cachedImages(), 0);
    EXPECT_EQ(cache.cachedBytes(), 0);

    ASSERT_FALSE(cache.readImage(one).isNull());
    EXPECT_FALSE(cache.isEmpty());
    EXPECT_EQ(cache.cachedImages(), 1);
    const qint64 onebytes = cache.cachedBytes();
    EXPECT_GT(onebytes, 0);

    ASSERT_FALSE(cache.readImage(two).isNull());
    EXPECT_EQ(cache.cachedImages(), 2);
    EXPECT_GT(cache.cachedBytes(), onebytes);

    // an unreadable file adds an entry but no bytes and no image
    const QString junk = dir.filePath("junk.miff");
    QFile file(junk);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write(QByteArray(512, '\x17')), 0);
    file.close();
    EXPECT_TRUE(cache.readImage(junk).isNull());
    EXPECT_EQ(cache.count(), 3);
    EXPECT_EQ(cache.cachedImages(), 2);
}

TEST(ImageCache, ForgetDropsTheConversionOfADeletedFile)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString miff = dir.filePath("image.miff");
    ASSERT_TRUE(writeImage(miff, 16, 12, Qt::green));

    ImageCache cache;
    ASSERT_FALSE(cache.readImage(miff).isNull());
    ASSERT_EQ(cache.cachedImages(), 1);
    ASSERT_EQ(convertedFiles(cache), 1);

    // this is what deleting an image in the slide show must do
    cache.forget(miff);
    EXPECT_EQ(cache.count(), 0);
    EXPECT_EQ(cache.cachedImages(), 0);
    EXPECT_EQ(cache.cachedBytes(), 0);
    EXPECT_TRUE(cache.isEmpty());
    // the converted copy is gone from the cache directory, not just forgotten
    EXPECT_EQ(convertedFiles(cache), 0);

    // forgetting a file the cache never saw is harmless
    cache.forget(dir.filePath("never-seen.miff"));
    EXPECT_EQ(cache.count(), 0);
}

TEST(ImageCache, PurgeConversionsKeepsFramesAndFailureRecords)
{
    if (!haveImageMagick()) GTEST_SKIP() << "neither magick nor convert found in PATH";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString miff = dir.filePath("image.miff");
    const QString junk = dir.filePath("junk.miff");
    ASSERT_TRUE(writeImage(miff, 16, 12, Qt::green));
    QFile file(junk);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write(QByteArray(512, '\x17')), 0);
    file.close();

    ImageCache cache;
    ASSERT_FALSE(cache.readImage(miff).isNull());
    ASSERT_TRUE(cache.readImage(junk).isNull());
    ASSERT_EQ(cache.count(), 2);
    ASSERT_EQ(cache.cachedImages(), 1);
    ASSERT_EQ(cache.conversions(), 2);

    // pretend a movie was imported into a subdirectory of the cache
    const QString frames = cache.makeSubDir("movie");
    ASSERT_FALSE(frames.isEmpty());
    ASSERT_TRUE(writeImage(QDir(frames).filePath("frame_00001.png"), 8, 8, Qt::red));
    ASSERT_TRUE(writeImage(QDir(frames).filePath("frame_00002.png"), 8, 8, Qt::red));
    cache.registerFrames(frames);
    EXPECT_EQ(cache.frameImages(), 2);
    EXPECT_GT(cache.frameBytes(), 0);

    cache.purgeConversions();

    // the converted image is gone from disk and from the totals ...
    EXPECT_EQ(cache.cachedImages(), 0);
    EXPECT_EQ(cache.cachedBytes(), 0);
    EXPECT_EQ(convertedFiles(cache), 0);
    // ... the movie frames are untouched, since re-creating them needs FFmpeg ...
    EXPECT_EQ(cache.frameImages(), 2);
    EXPECT_TRUE(QFileInfo::exists(QDir(frames).filePath("frame_00002.png")));
    EXPECT_FALSE(cache.isEmpty());
    // ... and the unreadable file is still remembered, so it stays quiet
    EXPECT_EQ(cache.count(), 1);

    // the purged image is converted again on the next display, the junk is not
    ASSERT_FALSE(cache.readImage(miff).isNull());
    EXPECT_EQ(cache.conversions(), 3);
    EXPECT_TRUE(cache.readImage(junk).isNull());
    EXPECT_EQ(cache.conversions(), 3);
    EXPECT_EQ(cache.cachedImages(), 1);
}

TEST(ImageCache, SubDirsAreUniqueAndSanitized)
{
    ImageCache cache;
    const QString one = cache.makeSubDir("my movie.mp4");
    const QString two = cache.makeSubDir("my movie.mp4");
    ASSERT_FALSE(one.isEmpty());
    ASSERT_FALSE(two.isEmpty());
    EXPECT_NE(one, two);
    EXPECT_TRUE(QDir(one).exists());
    EXPECT_TRUE(QDir(two).exists());
    // the name is derived from a file name and must not carry separators
    EXPECT_FALSE(QFileInfo(one).fileName().contains(' '));
    EXPECT_FALSE(QFileInfo(one).fileName().contains('.'));
    // both live inside the cache directory
    EXPECT_TRUE(one.startsWith(cache.path()));
    EXPECT_TRUE(two.startsWith(cache.path()));
}

TEST(ImageCache, ClearRemovesTheTemporaryDirectory)
{
    ImageCache cache;
    const QString path = cache.path();
    ASSERT_FALSE(path.isEmpty());
    ASSERT_TRUE(QDir(path).exists());

    cache.clear();
    EXPECT_FALSE(QDir(path).exists());
    EXPECT_EQ(cache.count(), 0);
    // a new directory is created on demand after a clear
    const QString fresh = cache.path();
    EXPECT_FALSE(fresh.isEmpty());
    EXPECT_NE(fresh, path);
}

TEST(ImageCache, DestructorRemovesTheTemporaryDirectory)
{
    QString path;
    {
        ImageCache cache;
        path = cache.path();
        ASSERT_FALSE(path.isEmpty());
        ASSERT_TRUE(QDir(path).exists());
    }
    EXPECT_FALSE(QDir(path).exists());
}
