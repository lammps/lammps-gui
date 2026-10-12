// Unit tests for the block-structured fix ave/* file parsers
// (src/plotblockdata.cpp), exercised without a GUI.

#include "plotblockdata.h"

#include "gtest/gtest.h"

#include <QList>
#include <QString>

#include <cmath>

namespace {

// --- sample files, matching what the LAMMPS fixes actually write -------

// fix ave/time in vector mode: block header "step nrows", rows "row v1 ... vN"
const char ave_time_vector[] = "# Time-averaged data for fix myvec\n"
                               "# TimeStep Number-of-rows\n"
                               "# Row c_rdf[1] c_rdf[2]\n"
                               "1000 3\n"
                               "1 0.05 0.1\n"
                               "2 0.15 0.5\n"
                               "3 0.25 1.2\n"
                               "2000 3\n"
                               "1 0.05 0.3\n"
                               "2 0.15 0.7\n"
                               "3 0.25 1.4\n";

// fix ave/histo: block header "step nbins total missing min max", rows
// "bin coord count count/total"
const char ave_histo[] =
    "# Histogrammed data for fix myhisto\n"
    "# TimeStep Number-of-bins Total-counts Missing-counts Min-value Max-value\n"
    "# Bin Coord Count Count/Total\n"
    "1000 4 100 0 -1.5 2.5\n"
    "1 -1 10 0.1\n"
    "2 0 20 0.2\n"
    "3 1 40 0.4\n"
    "4 2 30 0.3\n"
    "2000 4 200 5 -1.4 2.6\n"
    "1 -1 40 0.2\n"
    "2 0 40 0.2\n"
    "3 1 80 0.4\n"
    "4 2 40 0.2\n";

// fix ave/correlate: block header "step nwindows", rows
// "index timedelta ncount c1 ..."  (here with nevery 5).  With the default
// "ave one" the sample counts start over in every window.
const char ave_correlate[] = "# Time-correlated data for fix mycorr\n"
                             "# Timestep Number-of-time-windows\n"
                             "# Index TimeDelta Ncount v_vx*v_vx\n"
                             "1000 3\n"
                             "1 0 100 1\n"
                             "2 5 99 0.5\n"
                             "3 10 98 0.25\n"
                             "2000 3\n"
                             "1 0 100 1\n"
                             "2 5 99 0.6\n"
                             "3 10 98 0.3\n";

// fix ave/correlate/long: two title lines only, blocks delimited by a comment,
// first column already simulation time
const char ave_correlate_long[] = "# Time-correlated data for fix myclong\n"
                                  "# Time v_vx*v_vx\n"
                                  "# Timestep: 1000\n"
                                  "0 1 \n"
                                  "0.005 0.5 \n"
                                  "0.01 0.25 \n"
                                  "# Timestep: 2000\n"
                                  "0 1 \n"
                                  "0.005 0.6 \n"
                                  "0.01 0.3 \n";

// fix ave/time in scalar mode: a flat table, must not be taken for blocks
const char ave_time_scalar[] = "# Time-averaged data for fix mysca\n"
                               "# TimeStep v_a v_b\n"
                               "100 1 2\n"
                               "200 3 4\n"
                               "300 5 6\n";

// fix ave/time vector mode YAML, as written for a .yaml file argument
const char ave_time_yaml[] = "---\n"
                             "keywords: ['c_rdf[1]', 'c_rdf[2]', ]\n"
                             "data:\n"
                             "  1000:\n"
                             "  - [0.05, 0.1, ]\n"
                             "  - [0.15, 0.5, ]\n"
                             "  2000:\n"
                             "  - [0.05, 0.3, ]\n"
                             "  - [0.15, 0.7, ]\n";

// fix ave/time scalar mode YAML: a flat list of rows, no per-step keys
const char ave_time_yaml_scalar[] = "---\n"
                                    "keywords: ['Step', 'v_a', ]\n"
                                    "data:\n"
                                    "  - [100, 1, ]\n"
                                    "  - [200, 2, ]\n";

QStringList names(const PlotDataBlock &b)
{
    return b.rows.columnNames();
}

// --- native format ----------------------------------------------------

TEST(AveBlocks, TimeVector)
{
    const PlotBlockData d = parseAveBlocks(ave_time_vector);
    EXPECT_EQ(d.kind, AveFileKind::AveTimeVector);
    EXPECT_EQ(d.fixId, "myvec");
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[0].step, 1000);
    EXPECT_EQ(d.blocks[1].step, 2000);
    EXPECT_TRUE(d.blocks[0].scalars.empty());
    EXPECT_TRUE(d.scalarNames.isEmpty());

    EXPECT_EQ(names(d.blocks[0]), QStringList({"Row", "c_rdf[1]", "c_rdf[2]"}));
    ASSERT_EQ(d.blocks[0].rows.rowCount(), 3);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(0)[2], 3.0);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(2)[2], 1.2);
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(2)[0], 0.3);
}

TEST(AveBlocks, Histo)
{
    const PlotBlockData d = parseAveBlocks(ave_histo);
    EXPECT_EQ(d.kind, AveFileKind::AveHisto);
    EXPECT_EQ(d.fixId, "myhisto");
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Bin", "Coord", "Count", "Count/Total"}));
    EXPECT_EQ(d.scalarNames,
              QStringList({"Total-counts", "Missing-counts", "Min-value", "Max-value"}));

    ASSERT_EQ(d.blocks[1].scalars.size(), 4u);
    EXPECT_DOUBLE_EQ(d.blocks[1].scalars[0], 200.0);
    EXPECT_DOUBLE_EQ(d.blocks[1].scalars[1], 5.0);
    EXPECT_DOUBLE_EQ(d.blocks[1].scalars[2], -1.4);
    EXPECT_DOUBLE_EQ(d.blocks[1].scalars[3], 2.6);

    ASSERT_EQ(d.blocks[0].rows.rowCount(), 4);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(3)[2], 0.4);
}

TEST(AveBlocks, Correlate)
{
    const PlotBlockData d = parseAveBlocks(ave_correlate);
    EXPECT_EQ(d.kind, AveFileKind::AveCorrelate);
    EXPECT_EQ(d.fixId, "mycorr");
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Index", "TimeDelta", "Ncount", "v_vx*v_vx"}));
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(1)[2], 10.0);
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(3)[1], 0.6);
}

TEST(AveBlocks, CorrelateLong)
{
    const PlotBlockData d = parseAveBlocks(ave_correlate_long);
    EXPECT_EQ(d.kind, AveFileKind::AveCorrelateLong);
    EXPECT_EQ(d.fixId, "myclong");
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[0].step, 1000);
    EXPECT_EQ(d.blocks[1].step, 2000);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Time", "v_vx*v_vx"}));
    ASSERT_EQ(d.blocks[0].rows.rowCount(), 3);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(0)[1], 0.005);
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(1)[2], 0.3);
}

// A single value in vector mode makes the block header exactly as wide as a
// data row; the row count on the header is what still separates them.
TEST(AveBlocks, VectorWithOneValue)
{
    const char text[]     = "# Time-averaged data for fix v1\n"
                            "# TimeStep Number-of-rows\n"
                            "# Row c_x\n"
                            "100 2\n"
                            "1 1.5\n"
                            "2 2.5\n"
                            "200 2\n"
                            "1 3.5\n"
                            "2 4.5\n";
    const PlotBlockData d = parseAveBlocks(text);
    EXPECT_EQ(d.kind, AveFileKind::AveTimeVector);
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Row", "c_x"}));
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(1)[1], 4.5);
}

// "overwrite" truncates the file to the latest block
TEST(AveBlocks, OverwriteSingleBlock)
{
    const char text[] =
        "# Histogrammed data for fix h\n"
        "# TimeStep Number-of-bins Total-counts Missing-counts Min-value Max-value\n"
        "# Bin Coord Count Count/Total\n"
        "5000 2 10 0 0 1\n"
        "1 0.25 4 0.4\n"
        "2 0.75 6 0.6\n";
    const PlotBlockData d = parseAveBlocks(text);
    EXPECT_EQ(d.kind, AveFileKind::AveHisto);
    ASSERT_EQ(d.blockCount(), 1);
    EXPECT_EQ(d.blocks[0].step, 5000);
    EXPECT_EQ(d.blocks[0].rows.rowCount(), 2);
}

// a second fix instance re-opens the file and writes its headers again
TEST(AveBlocks, AppendedHeaderMidFile)
{
    const char text[]     = "# Time-averaged data for fix a\n"
                            "# TimeStep Number-of-rows\n"
                            "# Row c_x\n"
                            "100 2\n"
                            "1 1\n"
                            "2 2\n"
                            "# Time-averaged data for fix a\n"
                            "# TimeStep Number-of-rows\n"
                            "# Row c_x\n"
                            "200 2\n"
                            "1 3\n"
                            "2 4\n";
    const PlotBlockData d = parseAveBlocks(text);
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[1].step, 200);
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(1)[1], 4.0);
}

// a run killed mid-write leaves the last block short of its announced rows
TEST(AveBlocks, TruncatedLastBlock)
{
    const char text[]     = "# Time-averaged data for fix a\n"
                            "# TimeStep Number-of-rows\n"
                            "# Row c_x\n"
                            "100 3\n"
                            "1 1\n"
                            "2 2\n"
                            "3 3\n"
                            "200 3\n"
                            "1 4\n"
                            "2 5\n";
    const PlotBlockData d = parseAveBlocks(text);
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[0].rows.rowCount(), 3);
    EXPECT_EQ(d.blocks[1].rows.rowCount(), 2);
}

TEST(AveBlocks, ChangingRowCount)
{
    const char text[]     = "# Time-averaged data for fix a\n"
                            "# TimeStep Number-of-rows\n"
                            "# Row c_x\n"
                            "100 2\n"
                            "1 1\n"
                            "2 2\n"
                            "200 3\n"
                            "1 3\n"
                            "2 4\n"
                            "3 5\n";
    const PlotBlockData d = parseAveBlocks(text);
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[0].rows.rowCount(), 2);
    EXPECT_EQ(d.blocks[1].rows.rowCount(), 3);
}

// --- structural fallback when title1/2/3 replaced the headers ---------

TEST(AveBlocksFallback, HistoWithoutDefaultTitles)
{
    const char text[]     = "# my own histogram\n"
                            "# step bins total missing lo hi\n"
                            "# index position hits fraction\n"
                            "1000 2 10 0 0 1\n"
                            "1 0.25 4 0.4\n"
                            "2 0.75 6 0.6\n";
    const PlotBlockData d = parseAveBlocks(text);
    EXPECT_EQ(d.kind, AveFileKind::AveHisto);
    EXPECT_TRUE(d.fixId.isEmpty());
    // the column names still come from the comment as wide as a data row
    EXPECT_EQ(names(d.blocks[0]), QStringList({"index", "position", "hits", "fraction"}));
    EXPECT_EQ(d.scalarNames, QStringList({"total", "missing", "lo", "hi"}));
}

TEST(AveBlocksFallback, CorrelateWithoutDefaultTitles)
{
    const char text[]     = "# custom title\n"
                            "1000 3\n"
                            "1 0 100 1\n"
                            "2 5 99 0.5\n"
                            "3 10 98 0.25\n";
    const PlotBlockData d = parseAveBlocks(text);
    // recognized by the 0, nevery, 2*nevery progression in column 2
    EXPECT_EQ(d.kind, AveFileKind::AveCorrelate);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"column1", "column2", "column3", "column4"}));
}

TEST(AveBlocksFallback, VectorWithoutDefaultTitles)
{
    const char text[]     = "1000 3\n"
                            "1 10 100 1\n"
                            "2 20 99 0.5\n"
                            "3 30 98 0.25\n";
    const PlotBlockData d = parseAveBlocks(text);
    // same header width as correlate, but column 2 does not start at zero
    EXPECT_EQ(d.kind, AveFileKind::AveTimeVector);
}

// fix ave/chunk from a 1-d binning: recognized, and the coordinate is the x axis
TEST(AveBlocksFallback, ChunkIsBlockData)
{
    const char text[]     = "# Chunk-averaged data for fix mychunk and group all\n"
                            "# Timestep Number-of-chunks Total-count\n"
                            "# Chunk Coord1 Ncount density/mass\n"
                            "1000 2 500\n"
                            "1 0.5 250 0.8\n"
                            "2 1.5 250 0.9\n";
    const PlotBlockData d = parseAveBlocks(text);
    EXPECT_EQ(d.kind, AveFileKind::AveChunk);
    EXPECT_EQ(d.fixId, "mychunk");
    ASSERT_EQ(d.blockCount(), 1);
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Chunk", "Coord1", "Ncount", "density/mass"}));
    EXPECT_EQ(d.scalarNames, QStringList({"Total-count"}));
    EXPECT_DOUBLE_EQ(d.blocks[0].scalars[0], 500.0);
}

// --- detection --------------------------------------------------------

TEST(AveBlocksDetect, RecognizesBlockFiles)
{
    EXPECT_TRUE(looksLikeAveBlocks(ave_time_vector));
    EXPECT_TRUE(looksLikeAveBlocks(ave_histo));
    EXPECT_TRUE(looksLikeAveBlocks(ave_correlate));
    EXPECT_TRUE(looksLikeAveBlocks(ave_correlate_long));
}

TEST(AveBlocksDetect, RejectsFlatTables)
{
    // a fix ave/time scalar file: its header comment says "Time-averaged data
    // for fix", but there is no block structure behind it
    EXPECT_FALSE(looksLikeAveBlocks(ave_time_scalar));
    EXPECT_FALSE(looksLikeAveBlocks("# Step Temp Press\n0 300 1.0\n100 310 1.1\n200 305 1.2\n"));
    EXPECT_FALSE(looksLikeAveBlocks(""));
    EXPECT_FALSE(looksLikeAveBlocks("just some text\nand more of it\n"));
}

TEST(AveBlocksDetect, KindNames)
{
    EXPECT_EQ(aveFileKindName(AveFileKind::AveHisto), "fix ave/histo");
    EXPECT_EQ(aveFileKindName(AveFileKind::AveCorrelateLong), "fix ave/correlate/long");
    EXPECT_FALSE(aveFileKindName(AveFileKind::Unknown).isEmpty());
}

// --- YAML -------------------------------------------------------------

TEST(AveBlocksYaml, VectorMode)
{
    const PlotBlockData d = parseAveBlocksYaml(ave_time_yaml);
    EXPECT_EQ(d.kind, AveFileKind::AveTimeVector);
    ASSERT_EQ(d.blockCount(), 2);
    EXPECT_EQ(d.blocks[0].step, 1000);
    EXPECT_EQ(d.blocks[1].step, 2000);
    // a Row column is synthesized so the table matches the native format
    EXPECT_EQ(names(d.blocks[0]), QStringList({"Row", "c_rdf[1]", "c_rdf[2]"}));
    ASSERT_EQ(d.blocks[0].rows.rowCount(), 2);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(0)[1], 2.0);
    EXPECT_DOUBLE_EQ(d.blocks[0].rows.column(2)[1], 0.5);
    EXPECT_DOUBLE_EQ(d.blocks[1].rows.column(2)[1], 0.7);
}

TEST(AveBlocksYaml, ScalarModeIsNotBlockData)
{
    QString err;
    const PlotBlockData d = parseAveBlocksYaml(ave_time_yaml_scalar, &err);
    EXPECT_TRUE(d.isEmpty());
    EXPECT_FALSE(err.isEmpty());
}

// --- reduction to a plottable table -----------------------------------

// three blocks of two rows, with hand-computable statistics:
//   column "v" row 0:  10, 12, 14  -> mean 12, stddev 2,        stderr 2/sqrt(3)
//   column "v" row 1:  20, 26, 20  -> mean 22, stddev sqrt(12), stderr 2
const char reduce_sample[] = "# Time-averaged data for fix r\n"
                             "# TimeStep Number-of-rows\n"
                             "# Row v\n"
                             "100 2\n"
                             "1 10\n"
                             "2 20\n"
                             "200 2\n"
                             "1 12\n"
                             "2 26\n"
                             "300 2\n"
                             "1 14\n"
                             "2 20\n";

TEST(AveReduce, SingleBlock)
{
    const PlotBlockData d = parseAveBlocks(reduce_sample);
    ASSERT_EQ(d.blockCount(), 3);

    const PlotData last = singleBlock(d, 2);
    ASSERT_EQ(last.rowCount(), 2);
    EXPECT_EQ(last.columnNames(), QStringList({"Row", "v"}));
    EXPECT_DOUBLE_EQ(last.column(1)[0], 14.0);

    // out-of-range indices clamp rather than fail
    EXPECT_DOUBLE_EQ(singleBlock(d, -5).column(1)[0], 10.0);
    EXPECT_DOUBLE_EQ(singleBlock(d, 99).column(1)[0], 14.0);
    EXPECT_TRUE(singleBlock(PlotBlockData(), 0).isEmpty());
}

TEST(AveReduce, MeanOverAllBlocks)
{
    const PlotBlockData d  = parseAveBlocks(reduce_sample);
    const BlockAverage avg = averageBlocks(d, 0, 2, BlockErrorType::None);
    EXPECT_EQ(avg.usedBlocks, 3);
    EXPECT_EQ(avg.skippedBlocks, 0);
    ASSERT_EQ(avg.data.rowCount(), 2);
    EXPECT_EQ(avg.data.columnNames(), QStringList({"Row", "v"}));
    // the row index column averages to itself
    EXPECT_DOUBLE_EQ(avg.data.column(0)[0], 1.0);
    EXPECT_DOUBLE_EQ(avg.data.column(0)[1], 2.0);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[0], 12.0);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[1], 22.0);
    EXPECT_TRUE(avg.errors.isEmpty());
}

TEST(AveReduce, StandardDeviation)
{
    const PlotBlockData d  = parseAveBlocks(reduce_sample);
    const BlockAverage avg = averageBlocks(d, 0, 2, BlockErrorType::StdDev);
    ASSERT_EQ(avg.errors.columnCount(), 2u);
    ASSERT_EQ(avg.errors.upper[1].size(), 2u);
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][0], 2.0);
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][1], std::sqrt(12.0));
    // a column that is the same in every block has no spread
    EXPECT_DOUBLE_EQ(avg.errors.upper[0][0], 0.0);
    // the deviation reaches equally far up and down
    EXPECT_FALSE(avg.errors.isAsymmetric());
}

TEST(AveReduce, StandardError)
{
    const PlotBlockData d  = parseAveBlocks(reduce_sample);
    const BlockAverage avg = averageBlocks(d, 0, 2, BlockErrorType::StdError);
    ASSERT_EQ(avg.errors.columnCount(), 2u);
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][0], 2.0 / std::sqrt(3.0));
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][1], 2.0);
}

TEST(AveReduce, MinMaxSpread)
{
    const PlotBlockData d  = parseAveBlocks(reduce_sample);
    const BlockAverage avg = averageBlocks(d, 0, 2, BlockErrorType::MinMax);
    ASSERT_TRUE(avg.errors.isAsymmetric());
    ASSERT_EQ(avg.errors.columnCount(), 2u);
    ASSERT_EQ(avg.errors.lower.size(), 2u);
    ASSERT_EQ(avg.errors.upper[1].size(), 2u);
    // row 1: 10, 12, 14 around a mean of 12 -- the spread happens to be even
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][0], 2.0);
    EXPECT_DOUBLE_EQ(avg.errors.lower[1][0], 2.0);
    // row 2: 20, 26, 20 around a mean of 22 -- reaching further up than down
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][1], 4.0);
    EXPECT_DOUBLE_EQ(avg.errors.lower[1][1], 2.0);
    // a column that is the same in every block spans nothing
    EXPECT_DOUBLE_EQ(avg.errors.upper[0][0], 0.0);
    EXPECT_DOUBLE_EQ(avg.errors.lower[0][0], 0.0);
    // the bars end exactly on the extreme values of the blocks
    EXPECT_DOUBLE_EQ(avg.data.column(1)[1] + avg.errors.upper[1][1], 26.0);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[1] - avg.errors.lower[1][1], 20.0);
}

TEST(AveReduce, SubRangeAndClamping)
{
    const PlotBlockData d = parseAveBlocks(reduce_sample);
    // trimming the first block as equilibration: mean of 12, 14 and of 26, 20
    const BlockAverage avg = averageBlocks(d, 1, 2, BlockErrorType::StdDev);
    EXPECT_EQ(avg.usedBlocks, 2);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[0], 13.0);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[1], 23.0);
    EXPECT_DOUBLE_EQ(avg.errors.upper[1][0], std::sqrt(2.0));

    // a reversed or out-of-range range is repaired, not rejected
    EXPECT_EQ(averageBlocks(d, 2, 1, BlockErrorType::None).usedBlocks, 2);
    EXPECT_EQ(averageBlocks(d, -3, 42, BlockErrorType::None).usedBlocks, 3);
}

TEST(AveReduce, SingleBlockHasNoErrorBars)
{
    const PlotBlockData d  = parseAveBlocks(reduce_sample);
    const BlockAverage avg = averageBlocks(d, 1, 1, BlockErrorType::StdDev);
    EXPECT_EQ(avg.usedBlocks, 1);
    EXPECT_TRUE(avg.errors.isEmpty());
    EXPECT_DOUBLE_EQ(avg.data.column(1)[0], 12.0);
}

TEST(AveReduce, BlocksOfOtherShapeAreDropped)
{
    const char text[] = "# Time-averaged data for fix a\n"
                        "# TimeStep Number-of-rows\n"
                        "# Row v\n"
                        "100 2\n"
                        "1 10\n"
                        "2 20\n"
                        "200 3\n" // a chunk count changed mid-run
                        "1 99\n"
                        "2 99\n"
                        "3 99\n"
                        "300 2\n"
                        "1 14\n"
                        "2 24\n";
    const PlotBlockData d  = parseAveBlocks(text);
    const BlockAverage avg = averageBlocks(d, 0, 2, BlockErrorType::None);
    EXPECT_EQ(avg.usedBlocks, 2);
    EXPECT_EQ(avg.skippedBlocks, 1);
    ASSERT_EQ(avg.data.rowCount(), 2);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[0], 12.0);
    EXPECT_DOUBLE_EQ(avg.data.column(1)[1], 22.0);
}

TEST(AveReduce, ErrorTypeNames)
{
    EXPECT_EQ(blockErrorTypeName(BlockErrorType::StdDev), "standard deviation");
    EXPECT_EQ(blockErrorTypeName(BlockErrorType::StdError), "standard error of the mean");
    EXPECT_EQ(blockErrorTypeName(BlockErrorType::MinMax), "min/max of the blocks");
    EXPECT_EQ(blockErrorTypeName(BlockErrorType::None), "none");
}

// --- per-format import defaults ---------------------------------------

TEST(AveDefaults, Histo)
{
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(ave_histo));
    // averaging the bins is what a histogram import is usually for
    EXPECT_TRUE(def.averageBlocks);
    EXPECT_EQ(def.xColumn, 1); // Coord
    // the per-block totals differ, so only the normalized column is preselected
    EXPECT_EQ(def.yColumns, QList<int>({3})); // Count/Total
}

TEST(AveDefaults, Correlate)
{
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(ave_correlate));
    EXPECT_TRUE(def.averageBlocks);
    EXPECT_EQ(def.xColumn, 1);                // TimeDelta
    EXPECT_EQ(def.yColumns, QList<int>({3})); // neither Index nor Ncount
}

TEST(AveDefaults, RunningCorrelateUsesLastBlock)
{
    // "ave running": Ncount accumulates, so every block is a better estimate of
    // the same quantity and averaging them would be wrong
    const char text[]           = "# Time-correlated data for fix c\n"
                                  "# Timestep Number-of-time-windows\n"
                                  "# Index TimeDelta Ncount v_a*v_a\n"
                                  "1000 2\n"
                                  "1 0 100 1\n"
                                  "2 5 99 0.5\n"
                                  "2000 2\n"
                                  "1 0 200 1\n"
                                  "2 5 199 0.6\n";
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(text));
    EXPECT_FALSE(def.averageBlocks);
}

TEST(AveDefaults, CorrelateLongAndTimeVector)
{
    const AveImportDefaults cl = aveImportDefaults(parseAveBlocks(ave_correlate_long));
    EXPECT_FALSE(cl.averageBlocks); // the correlator accumulates over the run
    EXPECT_EQ(cl.xColumn, 0);       // Time
    EXPECT_EQ(cl.yColumns, QList<int>({1}));

    const AveImportDefaults tv = aveImportDefaults(parseAveBlocks(ave_time_vector));
    EXPECT_FALSE(tv.averageBlocks);
    EXPECT_EQ(tv.xColumn, 0); // Row
    EXPECT_EQ(tv.yColumns, QList<int>({1, 2}));
}

// --- fix ave/chunk: only 1-d binning gets defaults ---------------------

// bin/1d and bin/sphere write a single Coord column
TEST(AveChunkDefaults, OneDimensionalBinning)
{
    const char text[]           = "# Chunk-averaged data for fix p and group all\n"
                                  "# Timestep Number-of-chunks Total-count\n"
                                  "# Chunk Coord1 Ncount density/mass\n"
                                  "1000 3 300\n"
                                  "1 0.5 100 0.8\n"
                                  "2 1.5 110 0.9\n"
                                  "3 2.5 90 0.7\n"
                                  "2000 3 300\n"
                                  "1 0.5 102 0.82\n"
                                  "2 1.5 108 0.88\n"
                                  "3 2.5 90 0.7\n";
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(text));
    // a profile is normally wanted averaged over the run.  The first bin's
    // Ncount grows from block to block here, which must NOT be read as an
    // accumulating average the way the identically named sample count of a
    // correlate file is: in a chunk file it is an atom count that fluctuates.
    EXPECT_TRUE(def.averageBlocks);
    EXPECT_EQ(def.xColumn, 1); // Coord1
    // the chunk index is bookkeeping, but the atom count per bin is data
    EXPECT_EQ(def.yColumns, QList<int>({2, 3}));
}

// "compress yes" inserts an OrigID column ahead of the coordinates
TEST(AveChunkDefaults, CompressedChunkIds)
{
    const char text[]           = "# Chunk-averaged data for fix p and group all\n"
                                  "# Timestep Number-of-chunks Total-count\n"
                                  "# Chunk OrigID Coord1 Ncount vx\n"
                                  "1000 2 200\n"
                                  "1 7 0.5 100 0.1\n"
                                  "2 9 1.5 100 0.2\n";
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(text));
    EXPECT_EQ(def.xColumn, 2);                   // Coord1
    EXPECT_EQ(def.yColumns, QList<int>({3, 4})); // Ncount and vx, not OrigID
}

// bin/cylinder writes an axial and a radial coordinate; with a single bin along
// the axis the data is still 1-d, which only the values can tell us
TEST(AveChunkDefaults, DegenerateCylinderIsOneDimensional)
{
    const char text[]           = "# Chunk-averaged data for fix p and group all\n"
                                  "# Timestep Number-of-chunks Total-count\n"
                                  "# Chunk Coord1 Coord2 Ncount temp\n"
                                  "1000 3 300\n"
                                  "1 5.0 0.5 100 300.1\n"
                                  "2 5.0 1.5 100 299.4\n"
                                  "3 5.0 2.5 100 301.7\n";
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(text));
    EXPECT_EQ(def.xColumn, 2);                   // the radial Coord2, which varies
    EXPECT_EQ(def.yColumns, QList<int>({3, 4})); // the constant Coord1 is dropped
}

// a real 2-d grid has no single x axis, so it keeps the generic defaults
TEST(AveChunkDefaults, TwoDimensionalGridStaysGeneric)
{
    const char text[]           = "# Chunk-averaged data for fix p and group all\n"
                                  "# Timestep Number-of-chunks Total-count\n"
                                  "# Chunk Coord1 Coord2 Ncount temp\n"
                                  "1000 4 400\n"
                                  "1 0.5 0.5 100 300.1\n"
                                  "2 0.5 1.5 100 299.4\n"
                                  "3 1.5 0.5 100 301.7\n"
                                  "4 1.5 1.5 100 300.9\n";
    const PlotBlockData d       = parseAveBlocks(text);
    const AveImportDefaults def = aveImportDefaults(d);
    EXPECT_EQ(d.kind, AveFileKind::AveChunk); // still recognized ...
    EXPECT_FALSE(def.averageBlocks);          // ... but reduced to nothing special
    EXPECT_EQ(def.xColumn, 0);
    EXPECT_EQ(def.yColumns, QList<int>({1, 2, 3, 4})); // every column is offered
}

// chunks that are not bins (by type, molecule, compute) have no Coord column
TEST(AveChunkDefaults, NonBinnedChunksStayGeneric)
{
    const char text[]           = "# Chunk-averaged data for fix p and group all\n"
                                  "# Timestep Number-of-chunks Total-count\n"
                                  "# Chunk Ncount c_ke\n"
                                  "1000 2 200\n"
                                  "1 100 1.5\n"
                                  "2 100 1.6\n";
    const AveImportDefaults def = aveImportDefaults(parseAveBlocks(text));
    EXPECT_FALSE(def.averageBlocks);
    EXPECT_EQ(def.xColumn, 0);
    EXPECT_EQ(def.yColumns, QList<int>({1, 2}));
}

} // namespace
