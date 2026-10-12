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

#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include "lammpssyntax.h"

#include <QColor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @brief Syntax highlighter for LAMMPS input scripts
 *
 * This class extends QSyntaxHighlighter to provide syntax highlighting for
 * LAMMPS input files in the CodeEditor.  Lines are lexed with the shared
 * syntax engine (tokenizeLine()), so multi-line constructs -- '&' line
 * continuations (including mid-word joins), triple-quoted strings, and
 * quoted strings spanning continuations -- are tracked through the
 * QSyntaxHighlighter block states and continuation lines are highlighted in
 * the context of their logical command.  Command words keep the historically
 * grown per-category colors; arguments are colored by their role from the
 * LammpsSyntax command spec table plus lexical classes (numbers, strings,
 * comments, variable references, special words).  Keywords that embed a
 * second command in a command line -- the "modify" of `write_dump` and the
 * "dump" of `rerun` -- are shown in the color of the command they stand for
 * (`dump_modify` and `read_dump`, respectively) and the arguments after
 * them are colored as arguments of that command.  Command and style names
 * that are unknown to the populated syntax registry are flagged with a wavy
 * underline, except for the word currently being edited at the cursor.
 */
class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param syntax Syntax registry with name sets and command specs (not owned, may not be null)
     * @param parent Parent text document to highlight
     */
    Highlighter(const LammpsSyntax *syntax, QTextDocument *parent);

    /**
     * @brief Destructor
     */
    ~Highlighter() override = default;

    Highlighter()                               = delete;
    Highlighter(const Highlighter &)            = delete;
    Highlighter(Highlighter &&)                 = delete;
    Highlighter &operator=(const Highlighter &) = delete;
    Highlighter &operator=(Highlighter &&)      = delete;

public slots:
    /**
     * @brief Track the editing cursor to suppress unknown-name marking there
     *
     * The unknown-name underline is not shown for the word the cursor is on,
     * so a partially typed name is not flagged.  Re-highlights the previously
     * active and the new block.
     *
     * @param blockNumber block (line) number of the cursor position
     * @param column column of the cursor within the block
     */
    void setCursorPos(int blockNumber, int column);

protected:
    /**
     * @brief Highlight a single block (line) of text
     * @param text The text to highlight
     */
    void highlightBlock(const QString &text) override;

private:
    /// format table slots
    enum class Fmt : quint8 {
        CmdLattice,  ///< command word: system setup / lattice category
        CmdOutput,   ///< command word: output category
        CmdModify,   ///< command word: *_modify category
        CmdParticle, ///< command word: particle / force field category
        CmdRun,      ///< command word: run category
        CmdSetup,    ///< command word: settings category
        CmdSpecial,  ///< command word: undo / flow control category
        CmdOther,    ///< command word: recognized but uncategorized
        Number,      ///< numbers and IDs (defined or referenced)
        String,      ///< quoted strings and string-like arguments
        Comment,     ///< comments
        Variable,    ///< $-references and c_/f_/v_ style references
        Special,     ///< special keywords and the '&' continuation marker
        SubStyle,    ///< sub-styles of hybrid styles
        Count        ///< number of format slots
    };

    /// format for a command word of the given category
    const QTextCharFormat &cmdFormat(CmdCat cat) const;

    /// format rendering a color name in its own color (cached; colors with
    /// too little WCAG contrast against the editor background get a dark or
    /// light chip background, whichever contrasts better with the color)
    const QTextCharFormat &colorFormat(const QString &name);

    QTextCharFormat formats[static_cast<int>(Fmt::Count)]; ///< format table
    QHash<QString, QTextCharFormat> colorFormats;          ///< cache for color name formats
    QColor unknownColor;                                   ///< underline color for unknown names
    QColor editorBackground;                               ///< editor background at construction
    const LammpsSyntax *syntax;                            ///< syntax registry (not owned)
    int cursorBlock  = -1;                                 ///< block number of the tracked cursor
    int cursorColumn = -1;                                 ///< column of the tracked cursor
};
#endif
// Local Variables:
// c-basic-offset: 4
// End:
