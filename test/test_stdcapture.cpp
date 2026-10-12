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

#include "stdcapture.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

// Test fixture for StdCapture tests
class StdCaptureTest : public ::testing::Test {
protected:
    StdCapture capturer;
};

// Basic capture via endCapture/getCapture

TEST_F(StdCaptureTest, CaptureSimpleOutput)
{
    capturer.beginCapture();
    printf("Hello World");
    capturer.endCapture();

    std::string result = capturer.getCapture();
    EXPECT_EQ(result, "Hello World");
}

TEST_F(StdCaptureTest, CaptureMultipleLines)
{
    capturer.beginCapture();
    printf("Line 1\n");
    printf("Line 2\n");
    printf("Line 3");
    capturer.endCapture();

    std::string result = capturer.getCapture();
    EXPECT_NE(result.find("Line 1"), std::string::npos);
    EXPECT_NE(result.find("Line 2"), std::string::npos);
    EXPECT_NE(result.find("Line 3"), std::string::npos);
}

TEST_F(StdCaptureTest, CaptureEmptyOutput)
{
    capturer.beginCapture();
    // write nothing
    capturer.endCapture();

    std::string result = capturer.getCapture();
    EXPECT_TRUE(result.empty());
}

TEST_F(StdCaptureTest, MultipleCapturesSameInstance)
{
    // First capture
    capturer.beginCapture();
    printf("First");
    capturer.endCapture();
    std::string first = capturer.getCapture();
    EXPECT_EQ(first, "First");

    // Second capture reuses the same pipe
    capturer.beginCapture();
    printf("Second");
    capturer.endCapture();
    std::string second = capturer.getCapture();
    EXPECT_EQ(second, "Second");
}

// getChunk tests

TEST_F(StdCaptureTest, GetChunkDuringCapture)
{
    capturer.beginCapture();
    printf("chunk data");

    std::string chunk = capturer.getChunk();
    EXPECT_EQ(chunk, "chunk data");

    capturer.endCapture();
}

TEST_F(StdCaptureTest, GetChunkWhenNotCapturing)
{
    // getChunk without active capture should return empty
    std::string chunk = capturer.getChunk();
    EXPECT_TRUE(chunk.empty());
}

TEST_F(StdCaptureTest, GetChunkMultipleTimes)
{
    capturer.beginCapture();

    printf("part1");
    std::string chunk1 = capturer.getChunk();
    EXPECT_EQ(chunk1, "part1");

    printf("part2");
    std::string chunk2 = capturer.getChunk();
    EXPECT_EQ(chunk2, "part2");

    capturer.endCapture();
}

// endCapture without beginCapture

TEST_F(StdCaptureTest, EndCaptureWithoutBegin)
{
    EXPECT_FALSE(capturer.endCapture());
}

// The self-test built into beginCapture(): a working capture verifies its
// marker and stays usable, and the marker is fully drained again -- the
// exact-match expectations of the tests above depend on that as well.

TEST_F(StdCaptureTest, BeginCaptureVerifiesItselfAndStaysUsable)
{
    EXPECT_TRUE(capturer.isUsable());
    capturer.beginCapture();
    EXPECT_TRUE(capturer.isUsable());
    EXPECT_TRUE(capturer.diagnostic().empty());

    // nothing of the verification marker may leak into the captured output
    printf("after the marker");
    capturer.endCapture();
    EXPECT_EQ(capturer.getCapture(), "after the marker");
}

TEST_F(StdCaptureTest, TotalReadCountsChunkBytes)
{
    capturer.beginCapture();
    EXPECT_EQ(capturer.totalRead(), 0u); // the verification marker does not count

    printf("12345");
    (void)capturer.getChunk();
    EXPECT_EQ(capturer.totalRead(), 5u);

    capturer.endCapture();
}

// The end-of-run probe: on a working capture its marker comes back (an intact
// redirect), it never leaks the marker, and real output drained along with the
// marker is handed back through endCapture()/getCapture().

TEST_F(StdCaptureTest, ProbeRunEndReportsAnIntactCapture)
{
    capturer.beginCapture();
    const std::string report = capturer.probeRunEnd();
    EXPECT_NE(report.find("intact"), std::string::npos);

    capturer.endCapture();
    EXPECT_TRUE(capturer.getCapture().empty());
}

TEST_F(StdCaptureTest, ProbeRunEndPreservesRealOutput)
{
    capturer.beginCapture();
    printf("late output");
    const std::string report = capturer.probeRunEnd();
    EXPECT_NE(report.find("intact"), std::string::npos);

    capturer.endCapture();
    EXPECT_EQ(capturer.getCapture(), "late output");
}

TEST_F(StdCaptureTest, ProbeRunEndWithoutCaptureIsEmpty)
{
    EXPECT_TRUE(capturer.probeRunEnd().empty());
}

// Buffer use tracking

TEST_F(StdCaptureTest, BufferUseInitiallyZero)
{
    EXPECT_DOUBLE_EQ(capturer.getBufferUse(), 0.0);
}

TEST_F(StdCaptureTest, BufferUseAfterChunk)
{
    capturer.beginCapture();
    printf("some data");
    capturer.getChunk();

    double usage = capturer.getBufferUse();
    EXPECT_GT(usage, 0.0);
    EXPECT_LT(usage, 1.0);

    capturer.endCapture();
}

// Verify stdout is restored after capture

TEST_F(StdCaptureTest, StdoutRestoredAfterCapture)
{
    // Capture something
    capturer.beginCapture();
    printf("captured");
    capturer.endCapture();

    // After endCapture, stdout should work normally again.
    // We verify indirectly: a second capture cycle should still work.
    capturer.beginCapture();
    printf("also captured");
    capturer.endCapture();
    std::string result = capturer.getCapture();
    EXPECT_EQ(result, "also captured");
}

// Local Variables:
// c-basic-offset: 4
// End:
