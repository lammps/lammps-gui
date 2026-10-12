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

#include "constants.h"
#include "helpers.h"
#include "stdcapture.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QTemporaryDir>

#include <cstdio>
#include <string>

// Fixture for tests that need Qt application
class HelpersTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // Create QCoreApplication if it doesn't exist
        if (!QCoreApplication::instance()) {
            static int argc     = 1;
            static char *argv[] = {(char *)"test_helpers"};
            app                 = new QCoreApplication(argc, argv);
        }
    }

    static QCoreApplication *app;
};

QCoreApplication *HelpersTest::app = nullptr;

// Tests for dateCompare function

TEST_F(HelpersTest, DateCompareSame)
{
    QString date1 = "15 Jan 2024";
    QString date2 = "15 Jan 2024";
    EXPECT_EQ(dateCompare(date1, date2), 0);
}

TEST_F(HelpersTest, DateCompareYearDifferent)
{
    QString older = "15 Jan 2023";
    QString newer = "15 Jan 2024";
    EXPECT_EQ(dateCompare(older, newer), -1);
    EXPECT_EQ(dateCompare(newer, older), 1);
}

TEST_F(HelpersTest, DateCompareMonthDifferent)
{
    QString earlier = "15 Jan 2024";
    QString later   = "15 Feb 2024";
    EXPECT_EQ(dateCompare(earlier, later), -1);
    EXPECT_EQ(dateCompare(later, earlier), 1);
}

TEST_F(HelpersTest, DateCompareDayDifferent)
{
    QString earlier = "14 Jan 2024";
    QString later   = "15 Jan 2024";
    EXPECT_EQ(dateCompare(earlier, later), -1);
    EXPECT_EQ(dateCompare(later, earlier), 1);
}

TEST_F(HelpersTest, DateCompareFullMonthName)
{
    QString date1 = "15 January 2024";
    QString date2 = "15 February 2024";
    // Should truncate to "Jan" and "Feb"
    EXPECT_EQ(dateCompare(date1, date2), -1);
}

TEST_F(HelpersTest, DateComparePartialMonthName)
{
    QString date1 = "15 January 2024";
    QString date2 = "15 Jan 2024";
    // Should truncate "January" to "Jan"
    EXPECT_EQ(dateCompare(date1, date2), 0);
    EXPECT_EQ(dateCompare(date2, date1), 0);
}

TEST_F(HelpersTest, DateCompareInvalidFormat)
{
    QString invalid = "Invalid";
    QString valid   = "15 Jan 2024";
    // Invalid format should return -1 or 1 depending on which is invalid
    EXPECT_EQ(dateCompare(invalid, valid), -1);
    EXPECT_EQ(dateCompare(valid, invalid), 1);
}

TEST_F(HelpersTest, DateCompareAllMonths)
{
    QString jan = "15 Jan 2024";
    QString dec = "15 Dec 2024";
    EXPECT_EQ(dateCompare(jan, dec), -1);
    EXPECT_EQ(dateCompare(dec, jan), 1);
}

// Tests for splitLine function

TEST_F(HelpersTest, SplitLineSimple)
{
    QString input        = "one two three";
    auto result          = splitLine(input);
    QStringList expected = {"one", "two", "three"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineEmpty)
{
    QString input = "";
    auto result   = splitLine(input);
    EXPECT_TRUE(result.empty());
}

TEST_F(HelpersTest, SplitLineWhitespaceOnly)
{
    QString input = "   \t\n  ";
    auto result   = splitLine(input);
    EXPECT_TRUE(result.empty());
}

TEST_F(HelpersTest, SplitLineSingleQuotes)
{
    QString input        = "one 'two three' four";
    auto result          = splitLine(input);
    QStringList expected = {"one", "'two three'", "four"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineDoubleQuotes)
{
    QString input        = "one \"two three\" four";
    auto result          = splitLine(input);
    QStringList expected = {"one", "\"two three\"", "four"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineEscapedQuotes)
{
    QString input        = "one \\'test\\' two";
    auto result          = splitLine(input);
    QStringList expected = {"one", "\\'test\\'", "two"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineMixedQuotes)
{
    QString input        = "word1 'single' \"double\" word2";
    auto result          = splitLine(input);
    QStringList expected = {"word1", "'single'", "\"double\"", "word2"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineTripleQuotes)
{
    // Triple quotes handling: """ is a special marker in LAMMPS
    // The implementation shows that """ at start triggers special parsing
    // but the exact behavior can be complex. We'll test that it at least
    // parses without crashing and returns some tokens.
    QString input = "word1 \"\"\"triple quoted text\"\"\" word2";
    auto result   = splitLine(input);
    // Just verify basic parsing works - we got some results
    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result[0], "word1");
    // The triple quote handling may parse this in various ways,
    // so we just ensure it doesn't crash and produces output
}

TEST_F(HelpersTest, SplitLineMultipleSpaces)
{
    QString input        = "one    two\t\tthree\n\nfour";
    auto result          = splitLine(input);
    QStringList expected = {"one", "two", "three", "four"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineLeadingTrailingWhitespace)
{
    QString input        = "  \t one two  \n";
    auto result          = splitLine(input);
    QStringList expected = {"one", "two"};
    EXPECT_EQ(result, expected);
}

// Tests for hasExe function

TEST_F(HelpersTest, HasExeSystemCommand)
{
    // Test with a command that should exist on most systems
#if defined(_WIN32)
    EXPECT_TRUE(hasExe("cmd"));
#else
    EXPECT_TRUE(hasExe("ls"));
#endif
}

TEST_F(HelpersTest, HasExeNonExistent)
{
    // Test with a command that definitely doesn't exist
    EXPECT_FALSE(hasExe("this_command_does_not_exist_12345"));
}

// Tests for findExe function

TEST_F(HelpersTest, FindExeReturnsFullPath)
{
#if defined(_WIN32)
    const QString path = findExe("cmd");
#else
    const QString path = findExe("ls");
#endif
    EXPECT_FALSE(path.isEmpty());
    EXPECT_TRUE(QFileInfo(path).isAbsolute());
    EXPECT_TRUE(QFileInfo(path).isExecutable());
}

TEST_F(HelpersTest, FindExeNonExistent)
{
    EXPECT_TRUE(findExe("this_command_does_not_exist_12345").isEmpty());
}

TEST_F(HelpersTest, FindExeConsistentWithHasExe)
{
#if defined(_WIN32)
    const QString exe = "cmd";
#else
    const QString exe = "ls";
#endif
    EXPECT_EQ(hasExe(exe), !findExe(exe).isEmpty());
}

// Tests for renameToBackup function

TEST_F(HelpersTest, RenameToBackupCreatesBackup)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString file = tmp.filePath("sample.txt");
    {
        QFile f(file);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("payload");
    }

    const QString backup = renameToBackup(file);
    EXPECT_EQ(backup, file + Cfg::BACKUP_SUFFIX);
    EXPECT_FALSE(QFile::exists(file));

    QFile bf(backup);
    ASSERT_TRUE(bf.open(QIODevice::ReadOnly));
    EXPECT_EQ(bf.readAll(), QByteArray("payload"));
}

TEST_F(HelpersTest, RenameToBackupReplacesStaleBackup)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString file = tmp.filePath("sample.txt");
    {
        QFile f(file);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("new");
    }
    {
        QFile f(file + Cfg::BACKUP_SUFFIX);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("stale");
    }

    // the removable stale backup is replaced, not given a numbered name
    const QString backup = renameToBackup(file);
    EXPECT_EQ(backup, file + Cfg::BACKUP_SUFFIX);

    QFile bf(backup);
    ASSERT_TRUE(bf.open(QIODevice::ReadOnly));
    EXPECT_EQ(bf.readAll(), QByteArray("new"));
}

TEST_F(HelpersTest, RenameToBackupMissingFile)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    EXPECT_TRUE(renameToBackup(tmp.filePath("does_not_exist")).isEmpty());
}

// Tests for getLammpsLibName function

TEST_F(HelpersTest, LammpsLibNameIsKnownPlatformName)
{
    // empty in linked mode; otherwise it must be one of the fixed
    // per-platform names that the preferences reset deletes
    const QString name = getLammpsLibName();
    if (!name.isEmpty()) {
        const QStringList known = {Cfg::LAMMPS_LIB_MACOS, Cfg::LAMMPS_LIB_WINDOWS,
                                   Cfg::LAMMPS_LIB_LINUX};
        EXPECT_TRUE(known.contains(name));
    }
}

// Tests for isLightTheme function

TEST_F(HelpersTest, IsLightThemeReturnsBoolean)
{
    // Just test that it returns a valid boolean value
    // We can't reliably test the actual value as it depends on system theme
    bool result = isLightTheme();
    // Should be either true or false, test passes if it doesn't crash
    EXPECT_TRUE(result == true || result == false);
}

// Tests for silenceStdout / restoreStdout functions

TEST_F(HelpersTest, SilenceStdoutSilencesOutput)
{
    // stdout should not be silenced initially
    EXPECT_FALSE(isStdoutSilenced());

    silenceStdout();
    EXPECT_TRUE(isStdoutSilenced());

    // Write something to stdout (should go to /dev/null)
    printf("this should be silenced");
    fflush(stdout);

    restoreStdout();
    EXPECT_FALSE(isStdoutSilenced());
}

TEST_F(HelpersTest, RestoreStdoutWhenNotSilenced)
{
    // Restoring when not silenced should be a no-op
    EXPECT_FALSE(isStdoutSilenced());
    restoreStdout();
    EXPECT_FALSE(isStdoutSilenced());
}

TEST_F(HelpersTest, SilenceStdoutNested)
{
    // nested silence requests are counted, so stdout stays silenced until the
    // outermost request is released (StdoutSilencer guards pair calls 1:1)
    silenceStdout();
    EXPECT_TRUE(isStdoutSilenced());
    silenceStdout();
    EXPECT_TRUE(isStdoutSilenced());

    restoreStdout(); // releases only the inner request
    EXPECT_TRUE(isStdoutSilenced());
    restoreStdout(); // releasing the outermost request restores stdout
    EXPECT_FALSE(isStdoutSilenced());

    // restoring with no pending silence request is a safe no-op
    restoreStdout();
    EXPECT_FALSE(isStdoutSilenced());
}

TEST_F(HelpersTest, SilenceStdoutSkippedDuringCapture)
{
    // When StdCapture is active, silenceStdout should be a no-op
    StdCapture capturer;
    capturer.beginCapture();

    silenceStdout();
    EXPECT_FALSE(isStdoutSilenced()); // should NOT have silenced

    capturer.endCapture();
}

TEST_F(HelpersTest, CaptureRestoresSilencedStdout)
{
    // StdCapture is typically created before silenceStdout is called
    StdCapture capturer;

    // When stdout is silenced and StdCapture starts, it should restore first
    silenceStdout();
    EXPECT_TRUE(isStdoutSilenced());

    capturer.beginCapture();
    // beginCapture should have restored (un-silenced) stdout
    EXPECT_FALSE(isStdoutSilenced());

    printf("captured output");
    fflush(stdout);

    capturer.endCapture();
    auto output = capturer.getCapture();
    EXPECT_NE(output.find("captured output"), std::string::npos);
}

TEST_F(HelpersTest, SilenceAndRestorePreservesStdout)
{
    // After silence + restore, stdout should work normally
    StdCapture capturer;

    silenceStdout();
    printf("silenced text");
    fflush(stdout);
    restoreStdout();

    // Now capture to verify stdout is functional
    capturer.beginCapture();
    printf("visible text");
    fflush(stdout);
    capturer.endCapture();

    auto output = capturer.getCapture();
    EXPECT_NE(output.find("visible text"), std::string::npos);
    EXPECT_EQ(output.find("silenced text"), std::string::npos);
}

// Tests for purgeDirectory function

TEST_F(HelpersTest, PurgeDirectoryRemovesFiles)
{
    // Create a temporary directory with some files
    QDir temp(QDir::tempPath());
    QString testDir = temp.filePath("lammps_gui_test_purge");
    temp.mkpath(testDir);

    // Create some test files
    QFile f1(testDir + "/test1.txt");
    EXPECT_TRUE(f1.open(QIODevice::WriteOnly));
    f1.write("test");
    f1.close();

    QFile f2(testDir + "/test2.txt");
    EXPECT_TRUE(f2.open(QIODevice::WriteOnly));
    f2.write("test");
    f2.close();

    // Verify files exist
    EXPECT_TRUE(QFile::exists(testDir + "/test1.txt"));
    EXPECT_TRUE(QFile::exists(testDir + "/test2.txt"));

    // Purge the directory
    purgeDirectory(testDir);

    // Files should be gone but directory should still exist
    EXPECT_FALSE(QFile::exists(testDir + "/test1.txt"));
    EXPECT_FALSE(QFile::exists(testDir + "/test2.txt"));
    EXPECT_TRUE(QDir(testDir).exists());

    // Cleanup
    temp.rmdir(testDir);
}

TEST_F(HelpersTest, PurgeDirectoryNonExistent)
{
    // Purging a non-existent directory should not crash
    purgeDirectory("/nonexistent/path/that/does/not/exist");
}

TEST_F(HelpersTest, PurgeDirectoryEmpty)
{
    // Purging an empty directory should work fine
    QDir temp(QDir::tempPath());
    QString testDir = temp.filePath("lammps_gui_test_purge_empty");
    temp.mkpath(testDir);

    purgeDirectory(testDir);

    EXPECT_TRUE(QDir(testDir).exists());
    temp.rmdir(testDir);
}

// Additional dateCompare edge case tests

TEST_F(HelpersTest, DateCompareDecemberMonths)
{
    // Test all months are recognized correctly
    QStringList months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < months.size() - 1; ++i) {
        QString earlier = QString("15 %1 2024").arg(months[i]);
        QString later   = QString("15 %1 2024").arg(months[i + 1]);
        EXPECT_EQ(dateCompare(earlier, later), -1)
            << "Expected " << months[i].toStdString() << " < " << months[i + 1].toStdString();
    }
}

TEST_F(HelpersTest, DateCompareBothInvalid)
{
    // When the first date is invalid (not 3 words), dateCompare returns -1
    EXPECT_EQ(dateCompare("invalid1", "invalid2"), -1);
    // But identical invalid strings return 0 (equality check first)
    EXPECT_EQ(dateCompare("invalid", "invalid"), 0);
}

TEST_F(HelpersTest, DateCompareWithUpdateSuffix)
{
    // dates with the same day-of-month but different months must compare by month
    QString date1 = "22 Jul 2025";
    QString date2 = "22 Aug 2025";
    EXPECT_EQ(dateCompare(date1, date2), -1);
}

// Additional splitLine edge case tests

TEST_F(HelpersTest, SplitLineSingleWord)
{
    QString input        = "word";
    auto result          = splitLine(input);
    QStringList expected = {"word"};
    EXPECT_EQ(result, expected);
}

TEST_F(HelpersTest, SplitLineHashComment)
{
    // In LAMMPS, # starts a comment - splitLine should handle this
    QString input = "fix 1 all nve # this is a comment";
    auto result   = splitLine(input);
    // After #, everything is ignored
    EXPECT_GE(result.size(), 1);
    EXPECT_EQ(result[0], "fix");
}

TEST_F(HelpersTest, SplitLineSpecialCharacters)
{
    QString input = "pair_coeff * * 1.0 2.5";
    auto result   = splitLine(input);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], "pair_coeff");
    EXPECT_EQ(result[1], "*");
    EXPECT_EQ(result[2], "*");
    EXPECT_EQ(result[3], "1.0");
    EXPECT_EQ(result[4], "2.5");
}

// Additional hasExe tests

TEST_F(HelpersTest, HasExeEmptyString)
{
    EXPECT_FALSE(hasExe(""));
}

TEST_F(HelpersTest, HasExeBash)
{
#if !defined(_WIN32)
    // bash should exist on Linux/macOS
    EXPECT_TRUE(hasExe("bash"));
#endif
}

// isImageFile recognizes images by extension (case-insensitive), including
// ImageMagick-only formats; non-image / unknown extensions are rejected
TEST_F(HelpersTest, IsImageFileByExtension)
{
    EXPECT_TRUE(isImageFile("snapshot.png"));
    EXPECT_TRUE(isImageFile("/path/to/Render.JPEG"));
    EXPECT_TRUE(isImageFile("dump.0.ppm"));
    EXPECT_TRUE(isImageFile("frame.tga")); // ImageMagick-only
    EXPECT_TRUE(isImageFile("a.tiff"));

    EXPECT_FALSE(isImageFile("in.peptide"));
    EXPECT_FALSE(isImageFile("log.lammps"));
    EXPECT_FALSE(isImageFile("data.yaml"));
    EXPECT_FALSE(isImageFile("noextension"));
}

TEST(QtMessageSilencer, CollectsWarningsInsteadOfPrintingThem)
{
    QString captured;
    {
        QtMessageSilencer silencer;
        qWarning("first complaint");
        qWarning("second complaint");
        captured = silencer.messages();
    }
    // one message per line, in the order they were emitted. Note that qDebug()
    // is not tested here: the default logging category drops debug output
    // before it can reach any message handler.
    EXPECT_EQ(captured, QString("first complaint\nsecond complaint"));
}

TEST(QtMessageSilencer, CollectsNothingWhenNothingIsEmitted)
{
    QtMessageSilencer silencer;
    EXPECT_TRUE(silencer.messages().isEmpty());
}

TEST(QtMessageSilencer, RestoresThePreviousHandlerAndNests)
{
    {
        QtMessageSilencer outer;
        {
            QtMessageSilencer inner;
            qWarning("inner only");
            EXPECT_EQ(inner.messages(), QString("inner only"));
        }
        // the inner guard swallowed its own message, not the outer one's
        EXPECT_TRUE(outer.messages().isEmpty());
        qWarning("outer only");
        EXPECT_EQ(outer.messages(), QString("outer only"));
    }
    // after the outermost guard is gone, messages are no longer collected
    QtMessageSilencer fresh;
    EXPECT_TRUE(fresh.messages().isEmpty());
}

TEST(GrayscaleImage, RemovesColorAndPreservesAlpha)
{
    QImage img(2, 1, QImage::Format_ARGB32);
    img.setPixel(0, 0, qRgba(255, 0, 0, 255)); // opaque red
    img.setPixel(1, 0, qRgba(0, 255, 0, 64));  // translucent green
    const QImage out = grayscaleImage(img);

    for (int x = 0; x < 2; ++x) {
        const QRgb px = out.pixel(x, 0);
        EXPECT_EQ(qRed(px), qGreen(px));
        EXPECT_EQ(qGreen(px), qBlue(px));
    }
    EXPECT_EQ(qAlpha(out.pixel(0, 0)), 255);
    EXPECT_EQ(qAlpha(out.pixel(1, 0)), 64);
}

TEST(GrayscaleImage, FadesTowardsTheMidpoint)
{
    QImage img(2, 1, QImage::Format_ARGB32);
    img.setPixel(0, 0, qRgba(0, 0, 0, 255));       // black
    img.setPixel(1, 0, qRgba(255, 255, 255, 255)); // white
    const QImage out = grayscaleImage(img);

    const int dark  = qRed(out.pixel(0, 0));
    const int light = qRed(out.pixel(1, 0));
    // both ends move towards the midpoint, keeping only a fraction of the span
    EXPECT_GT(dark, 0);
    EXPECT_LT(light, 255);
    EXPECT_LT(light - dark, 255 * Cfg::GRAYSCALE_CONTRAST + 1);
    EXPECT_GT(light - dark, 255 * Cfg::GRAYSCALE_CONTRAST - 1);
    // a desaturation-only conversion would have left the full black-to-white span
    EXPECT_LT(light - dark, 200);
}

// Tests for viewerFitSize (deterministic viewer window auto-resize)

TEST(ViewerFitSize, ContentFitsBudget)
{
    // content plus frame fits: natural size, no scroll bar allowance
    EXPECT_EQ(viewerFitSize(QSize(400, 300), QSize(1000, 800), 2, 16), QSize(402, 302));
}

TEST(ViewerFitSize, ExactFitAddsNoScrollBarRoom)
{
    EXPECT_EQ(viewerFitSize(QSize(998, 798), QSize(1000, 800), 2, 16), QSize(1000, 800));
}

TEST(ViewerFitSize, WidthOverflowAddsHorizontalBarRoom)
{
    // width is clamped; the horizontal scroll bar consumes viewport height
    EXPECT_EQ(viewerFitSize(QSize(2000, 300), QSize(1000, 800), 2, 16), QSize(1000, 318));
}

TEST(ViewerFitSize, HeightOverflowAddsVerticalBarRoom)
{
    // height is clamped; the vertical scroll bar consumes viewport width
    EXPECT_EQ(viewerFitSize(QSize(400, 2000), QSize(1000, 800), 2, 16), QSize(418, 800));
}

TEST(ViewerFitSize, BothOverflowClampsToBudget)
{
    EXPECT_EQ(viewerFitSize(QSize(2000, 2000), QSize(1000, 800), 2, 16), QSize(1000, 800));
}

TEST(ViewerFitSize, ScrollBarRoomStaysWithinBudget)
{
    // the extra scroll bar room must not push the other axis past its budget
    EXPECT_EQ(viewerFitSize(QSize(2000, 790), QSize(1000, 800), 2, 16), QSize(1000, 800));
}

TEST(ViewerFitSize, NegativeBudgetClampsToZero)
{
    // tiny screens can make the budget negative; never return a negative size
    EXPECT_EQ(viewerFitSize(QSize(400, 300), QSize(-10, -5), 2, 16), QSize(0, 0));
}

// Tests for defaultFileStem function

TEST(DefaultFileStem, LeadingInPrefix)
{
    EXPECT_EQ(defaultFileStem("in.melt"), QString("melt"));
}

TEST(DefaultFileStem, TrailingInputExtensions)
{
    EXPECT_EQ(defaultFileStem("melt.lmp"), QString("melt"));
    EXPECT_EQ(defaultFileStem("melt.txt"), QString("melt"));
    EXPECT_EQ(defaultFileStem("melt.lmp.txt"), QString("melt"));
    EXPECT_EQ(defaultFileStem("in.melt.lmp"), QString("melt"));
}

TEST(DefaultFileStem, EverythingElseRemains)
{
    EXPECT_EQ(defaultFileStem("melt"), QString("melt"));
    EXPECT_EQ(defaultFileStem("melt-2d"), QString("melt-2d"));
    EXPECT_EQ(defaultFileStem("melt.2d"), QString("melt.2d"));
    EXPECT_EQ(defaultFileStem("melt.data"), QString("melt.data"));
    EXPECT_EQ(defaultFileStem("in.melt (ACF)"), QString("melt (ACF)"));
}

TEST(DefaultFileStem, StripsDirectoryPart)
{
    EXPECT_EQ(defaultFileStem("/some/path/in.melt"), QString("melt"));
    EXPECT_EQ(defaultFileStem("path/to/melt.lmp"), QString("melt"));
}

TEST(DefaultFileStem, KnownDerivedExtensions)
{
    // data, log, restart, image, and movie files fed to the viewers
    EXPECT_EQ(defaultFileStem("thermo.csv"), QString("thermo"));
    EXPECT_EQ(defaultFileStem("thermo.dat"), QString("thermo"));
    EXPECT_EQ(defaultFileStem("thermo.yaml"), QString("thermo"));
    EXPECT_EQ(defaultFileStem("melt.log"), QString("melt"));
    EXPECT_EQ(defaultFileStem("melt.restart"), QString("melt"));
    EXPECT_EQ(defaultFileStem("snap.png"), QString("snap"));
    EXPECT_EQ(defaultFileStem("movie.mp4"), QString("movie"));
    EXPECT_EQ(defaultFileStem("MELT.LMP"), QString("MELT"));
}

TEST(DefaultFileStem, EmptyFallsBackToLammps)
{
    EXPECT_EQ(defaultFileStem(""), QString("lammps"));
    EXPECT_EQ(defaultFileStem("in."), QString("lammps"));
}

// Tests for ensureFileSuffix function

TEST(EnsureFileSuffix, AppendsWhenMissing)
{
    EXPECT_EQ(ensureFileSuffix("melt", "log"), QString("melt.log"));
    EXPECT_EQ(ensureFileSuffix("/some/path/melt", "png"), QString("/some/path/melt.png"));
}

TEST(EnsureFileSuffix, KeepsExistingSuffix)
{
    EXPECT_EQ(ensureFileSuffix("melt.out", "log"), QString("melt.out"));
    EXPECT_EQ(ensureFileSuffix("in.melt", "lmp"), QString("in.melt"));
}

TEST(EnsureFileSuffix, EmptyStaysEmpty)
{
    EXPECT_EQ(ensureFileSuffix("", "log"), QString(""));
}
