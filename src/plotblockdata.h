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

#ifndef PLOTBLOCKDATA_H
#define PLOTBLOCKDATA_H

// Block-structured data model and parsers for the output files written by the
// LAMMPS `fix ave/time` (vector mode), `fix ave/histo` and `fix ave/correlate`
// styles.  Unlike the flat tables of plotdata.h these files are a sequence of
// per-timestep blocks, each a small table of its own, which has to be reduced
// to one flat PlotData before it can be plotted.

#include "plotdata.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <vector>

/**
 * @brief Which LAMMPS fix wrote a block-structured data file
 *
 * The kind only selects the sensible import defaults (which column is the x
 * axis, whether averaging blocks is meaningful); the parser itself is generic,
 * so an unrecognized file still imports as @c Unknown without losing data.
 */
enum class AveFileKind {
    Unknown,          ///< block structure recognized, but not the writing fix
    AveTimeVector,    ///< `fix ave/time` in vector mode
    AveHisto,         ///< `fix ave/histo` (and `fix ave/histo/weight`)
    AveCorrelate,     ///< `fix ave/correlate`
    AveCorrelateLong, ///< `fix ave/correlate/long`
    AveChunk,         ///< `fix ave/chunk`
};

/**
 * @brief One per-timestep block of a fix ave/\* output file
 *
 * A block is the table LAMMPS writes for a single output timestep: the rows of
 * a vector, the bins of a histogram, or the time windows of a correlation
 * function.  @ref scalars carries the extra per-block numbers some styles put
 * on the block header line (for `fix ave/histo` the total and missing counts
 * and the value range); their names are in PlotBlockData::scalarNames.
 */
struct PlotDataBlock {
    long long step = 0;          ///< timestep this block was written for
    std::vector<double> scalars; ///< per-block extras from the block header line
    PlotData rows;               ///< the block's table of named columns
};

/**
 * @brief A parsed block-structured fix ave/\* output file
 *
 * All blocks of a well-formed file have the same columns; blocks of differing
 * shape can still occur when a file was appended to by a second fix, and are
 * dropped by the reduction step rather than by the parser.
 */
struct PlotBlockData {
    AveFileKind kind = AveFileKind::Unknown; ///< detected format
    QString fixId;                           ///< fix ID from the default header, if present
    QStringList scalarNames;                 ///< names for PlotDataBlock::scalars
    std::vector<PlotDataBlock> blocks;       ///< the per-timestep blocks, in file order

    /** @brief Number of blocks */
    int blockCount() const { return static_cast<int>(blocks.size()); }
    /** @brief True if no block carrying data was found */
    bool isEmpty() const { return blocks.empty(); }
    /** @brief Column names of the first block (empty if there is none) */
    QStringList columnNames() const
    {
        return blocks.empty() ? QStringList() : blocks.front().rows.columnNames();
    }
};

/**
 * @brief Human-readable name of a file kind, as shown in the import dialog
 * @param kind Detected or user-selected kind
 * @return Descriptive name, e.g. "fix ave/histo"
 */
QString aveFileKindName(AveFileKind kind);

/**
 * @brief Parse the native (whitespace-separated) fix ave/\* block format
 * @param text  File contents
 * @param error Optional out-parameter set to a message on failure
 * @return Parsed blocks (empty on failure)
 *
 * Recognizes both block layouts LAMMPS writes: a numeric block header line
 * (`step nrows ...`) followed by @c nrows data rows, and the `# Timestep: N`
 * comment delimiter of `fix ave/correlate/long`.  Comment lines anywhere in
 * the file re-synchronize the scanner, so a file that a second fix instance
 * appended its own header to still parses.
 */
PlotBlockData parseAveBlocks(const QString &text, QString *error = nullptr);

/**
 * @brief Parse the vector-mode YAML variant `fix ave/time` writes
 * @param text  File contents
 * @param error Optional out-parameter set to a message on failure
 * @return Parsed blocks (empty on failure)
 *
 * The shape is a @c keywords: list followed by @c data: as a map of timestep
 * keys, each holding a list of flow-style rows.  A leading @c Row column is
 * synthesized so that the table matches the native format, whose rows carry an
 * explicit row index.  Returns nothing for the scalar-mode YAML shape (a plain
 * list of rows), which parsePlotYaml() already handles.
 */
PlotBlockData parseAveBlocksYaml(const QString &text, QString *error = nullptr);

/**
 * @brief Test whether a file's contents are block-structured fix ave/\* output
 * @param text File contents
 * @return true if the file should be imported through the block path
 *
 * Requires an actual block structure, not just a matching header comment: the
 * row-index column of every block has to count 1, 2, ... n.  A flat table is
 * therefore never mistaken for a block file, whatever its comments say.
 */
bool looksLikeAveBlocks(const QString &text);

/**
 * @brief Load a file as block-structured data, choosing the parser by extension or content
 * @param filename Path to the data file
 * @param error    Optional out-parameter set to a message on failure
 * @return Parsed blocks; empty if the file is not block structured
 */
PlotBlockData loadPlotBlockData(const QString &filename, QString *error = nullptr);

/* --- reducing blocks to one plottable table --------------------------- */

/** @brief Which uncertainty a block average reports */
enum class BlockErrorType {
    None,     ///< no error bars
    StdDev,   ///< standard deviation of the blocks
    StdError, ///< standard error of the mean, sigma/sqrt(N)
    MinMax,   ///< full spread: the bar spans the smallest to the largest value
};

/**
 * @brief Name of an error type as offered in the import dialog
 * @param type Error type
 * @return Descriptive name, e.g. "standard deviation"
 */
QString blockErrorTypeName(BlockErrorType type);

/**
 * @brief One flat table averaged over a range of blocks
 */
struct BlockAverage {
    PlotData data;         ///< per-row mean of every column
    PlotErrors errors;     ///< per-column error bars; empty if there are none.  Only
                           ///< BlockErrorType::MinMax fills the lower half, since the
                           ///< spread of the blocks need not straddle their mean evenly.
    int usedBlocks    = 0; ///< number of blocks that contributed to the mean
    int skippedBlocks = 0; ///< blocks in the range dropped for having a different shape
};

/**
 * @brief Extract the rows of a single block (import mode "single block")
 * @param data  Parsed blocks
 * @param index Block index; clamped to the available range
 * @return That block's table, or an empty table if there are no blocks
 */
PlotData singleBlock(const PlotBlockData &data, int index);

/**
 * @brief Average a contiguous range of blocks row by row (import mode "average")
 * @param data  Parsed blocks
 * @param first First block of the range (inclusive)
 * @param last  Last block of the range (inclusive)
 * @param type  Which uncertainty to report as error bars
 * @return Means, error bars, and how many blocks contributed
 *
 * The last block of the range sets the expected shape; blocks of a different
 * shape are dropped and counted in BlockAverage::skippedBlocks rather than
 * silently reinterpreted.  Error bars need at least two contributing blocks,
 * and are computed in two passes so that a small spread on top of a large mean
 * does not lose its significant digits.
 *
 * Note that successive averaging windows are not strictly independent, so the
 * standard error is a lower bound on the true uncertainty.  BlockErrorType::MinMax
 * makes no statistical claim at all: it just marks the range the blocks covered,
 * which is why it is the one error type with an asymmetric result.
 */
BlockAverage averageBlocks(const PlotBlockData &data, int first, int last, BlockErrorType type);

/**
 * @brief Where the import dialog starts out for a given file
 */
struct AveImportDefaults {
    bool averageBlocks = false; ///< true: average all blocks; false: show the last block
    int xColumn        = 0;     ///< index of the column to put on the x axis
    QList<int> yColumns;        ///< indices of the columns to plot
};

/**
 * @brief Pick the import defaults that suit a parsed file
 * @param data Parsed blocks
 * @return Reduction mode and column roles to preselect in the dialog
 *
 * Histograms and correlation functions default to the block average, because
 * their time evolution is rarely what is wanted.  A file whose correlator
 * accumulates over the whole run defaults to the last block instead: there
 * every block is a successive estimate of the same quantity, so averaging them
 * would be statistically wrong.
 */
AveImportDefaults aveImportDefaults(const PlotBlockData &data);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
