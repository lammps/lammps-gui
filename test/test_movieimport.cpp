// Unit tests for the pure movie-probing helpers (src/movieimport.cpp).
//
// These tests exercise parseFrameRate(), selectedFrameCount(), and
// parseProbeOutput() without running ffprobe or opening a dialog. The JSON
// samples are verbatim output of "ffprobe -of json" for an MP4 file (which
// stores the frame count) and a WebM file (which does not).

#include "movieimport.h"

#include <QByteArray>
#include <QString>

#include "gtest/gtest.h"

namespace {

// "ffprobe -v error -select_streams v:0 -show_entries
//  stream=width,height,nb_frames,r_frame_rate,duration -show_entries
//  format=duration -of json test.mp4" for a 4 second, 25 fps movie
const char mp4_probe[] = R"({
    "programs": [],
    "stream_groups": [],
    "streams": [
        {
            "width": 320,
            "height": 240,
            "r_frame_rate": "25/1",
            "duration": "4.000000",
            "nb_frames": "100"
        }
    ],
    "format": {
        "duration": "4.000000"
    }
})";

// the same for a 2 second, 10 fps WebM movie: the container stores neither the
// frame count nor the stream duration, only the duration of the whole file
const char webm_probe[] = R"({
    "programs": [],
    "stream_groups": [],
    "streams": [
        {
            "width": 320,
            "height": 240,
            "r_frame_rate": "10/1"
        }
    ],
    "format": {
        "duration": "2.000000"
    }
})";

} // namespace

TEST(ParseFrameRate, Rational)
{
    EXPECT_DOUBLE_EQ(parseFrameRate("25/1"), 25.0);
    EXPECT_DOUBLE_EQ(parseFrameRate("50/2"), 25.0);
    // NTSC rates are not integers
    EXPECT_NEAR(parseFrameRate("30000/1001"), 29.97, 1.0e-3);
}

TEST(ParseFrameRate, PlainNumber)
{
    EXPECT_DOUBLE_EQ(parseFrameRate("25"), 25.0);
    EXPECT_DOUBLE_EQ(parseFrameRate("23.976"), 23.976);
}

TEST(ParseFrameRate, Invalid)
{
    // a stream without a frame rate reports "0/0"
    EXPECT_DOUBLE_EQ(parseFrameRate("0/0"), 0.0);
    EXPECT_DOUBLE_EQ(parseFrameRate("N/A"), 0.0);
    EXPECT_DOUBLE_EQ(parseFrameRate(""), 0.0);
    EXPECT_DOUBLE_EQ(parseFrameRate("25/"), 0.0);
}

TEST(SelectedFrameCount, EveryFrame)
{
    EXPECT_EQ(selectedFrameCount(1, 1, 1), 1);
    EXPECT_EQ(selectedFrameCount(1, 100, 1), 100);
    // counting the frames from 0 must give the same result
    EXPECT_EQ(selectedFrameCount(0, 99, 1), 100);
}

TEST(SelectedFrameCount, WithInterval)
{
    // frames 1, 3, 5, 7, 9
    EXPECT_EQ(selectedFrameCount(1, 10, 2), 5);
    // frames 1, 3, 5, 7, 9, 11 -- the last one is included
    EXPECT_EQ(selectedFrameCount(1, 11, 2), 6);
    // frames 10, 15, 20, 25, 30
    EXPECT_EQ(selectedFrameCount(10, 30, 5), 5);
    // an interval larger than the range still selects the first frame
    EXPECT_EQ(selectedFrameCount(10, 12, 100), 1);
}

TEST(SelectedFrameCount, Invalid)
{
    EXPECT_EQ(selectedFrameCount(10, 9, 1), 0);
    EXPECT_EQ(selectedFrameCount(1, 10, 0), 0);
    EXPECT_EQ(selectedFrameCount(1, 10, -1), 0);
}

TEST(FrameToSeconds, Valid)
{
    MovieInfo info;
    info.frames   = 100;
    info.duration = 4.0;
    // the first frame is at the start of the movie
    EXPECT_DOUBLE_EQ(frameToSeconds(1, info), 0.0);
    // the middle frame of the movie is near half of the duration
    EXPECT_DOUBLE_EQ(frameToSeconds(51, info), 2.0);
    // the last frame is one frame time before the end
    EXPECT_DOUBLE_EQ(frameToSeconds(100, info), 4.0 * 99 / 100);
}

TEST(FrameToSeconds, Invalid)
{
    MovieInfo info;
    info.frames   = 100;
    info.duration = 4.0;
    EXPECT_DOUBLE_EQ(frameToSeconds(0, info), 0.0);
    EXPECT_DOUBLE_EQ(frameToSeconds(-5, info), 0.0);
    info.frames = 0;
    EXPECT_DOUBLE_EQ(frameToSeconds(10, info), 0.0);
    info.frames   = 100;
    info.duration = 0.0;
    EXPECT_DOUBLE_EQ(frameToSeconds(10, info), 0.0);
}

TEST(SampleOutdated, StaysCurrent)
{
    // the initial sample sits in the middle of the full range
    EXPECT_FALSE(sampleOutdated(1, 1000, 500, 1000));
    // trimming either end a little keeps the middle close to the sample
    EXPECT_FALSE(sampleOutdated(100, 1000, 500, 1000));
    EXPECT_FALSE(sampleOutdated(1, 900, 500, 1000));
    // the threshold is a tenth of the movie: a shift of exactly that much
    // is still accepted
    EXPECT_FALSE(sampleOutdated(1, 1200, 500, 1000));
}

TEST(SampleOutdated, NeedsRefresh)
{
    // a narrow range at the start is far from a mid-movie sample
    EXPECT_TRUE(sampleOutdated(1, 10, 500, 1000));
    // ... and a narrow range at the end, too
    EXPECT_TRUE(sampleOutdated(990, 1000, 500, 1000));
    // shifting the middle by more than a tenth of the movie
    EXPECT_TRUE(sampleOutdated(1, 1210, 500, 1000));
    // short movies use a minimum threshold of one frame
    EXPECT_TRUE(sampleOutdated(1, 1, 3, 5));
}

TEST(SampleOutdated, Invalid)
{
    // inconsistent input never triggers a re-sample
    EXPECT_FALSE(sampleOutdated(10, 1, 5, 1000));
    EXPECT_FALSE(sampleOutdated(1, 10, 5, 0));
}

TEST(ParseProbeOutput, FrameCountFromContainer)
{
    const MovieInfo info = parseProbeOutput(mp4_probe);
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.width, 320);
    EXPECT_EQ(info.height, 240);
    EXPECT_EQ(info.frames, 100);
    EXPECT_DOUBLE_EQ(info.fps, 25.0);
    EXPECT_DOUBLE_EQ(info.duration, 4.0);
    EXPECT_TRUE(info.error.isEmpty());
}

TEST(ParseProbeOutput, MissingFrameCount)
{
    const MovieInfo info = parseProbeOutput(webm_probe);
    // a movie without a stored frame count is still valid: the caller then
    // counts the video packets, which parseProbeOutput() cannot do
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.width, 320);
    EXPECT_EQ(info.height, 240);
    EXPECT_EQ(info.frames, 0);
    EXPECT_DOUBLE_EQ(info.fps, 10.0);
    // the duration falls back to the one of the container
    EXPECT_DOUBLE_EQ(info.duration, 2.0);
    // and the frame count derived from it matches the packet count
    EXPECT_EQ(static_cast<int>(info.duration * info.fps), 20);
}

TEST(ParseProbeOutput, NoVideoStream)
{
    const MovieInfo info = parseProbeOutput(R"({"streams": [], "format": {"duration": "2.0"}})");
    EXPECT_FALSE(info.valid);
    EXPECT_FALSE(info.error.isEmpty());
}

TEST(ParseProbeOutput, NoFrameSize)
{
    const MovieInfo info = parseProbeOutput(R"({"streams": [{"r_frame_rate": "25/1"}]})");
    EXPECT_FALSE(info.valid);
    EXPECT_FALSE(info.error.isEmpty());
}

TEST(ParseProbeOutput, NotJson)
{
    const MovieInfo info = parseProbeOutput("ffprobe: command not found");
    EXPECT_FALSE(info.valid);
    EXPECT_FALSE(info.error.isEmpty());
}

TEST(ParseProbeOutput, NumbersAsJsonNumbers)
{
    // ffprobe writes nb_frames and duration as strings, but be robust in case
    // a future version writes them as JSON numbers instead
    const MovieInfo info = parseProbeOutput(
        R"({"streams": [{"width": 64, "height": 48, "r_frame_rate": "5/1",
             "nb_frames": 7, "duration": 1.4}]})");
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.frames, 7);
    EXPECT_DOUBLE_EQ(info.duration, 1.4);
}
