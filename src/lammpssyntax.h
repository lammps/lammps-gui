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

#ifndef LAMMPSSYNTAX_H
#define LAMMPSSYNTAX_H

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Style-list categories known to the syntax registry
 *
 * The first group mirrors the categories that can be enumerated through the
 * LAMMPS library interface (lammps_style_count() / lammps_style_name());
 * Variable, Units, Extra, and ImageKw are static keyword sets that LAMMPS
 * does not expose through introspection, and Color is injected from the
 * shared color name list (see lammpsImageColors()).
 */
enum class StyleCat : quint8 {
    Command,   ///< command names (input commands + registered command styles)
    Fix,       ///< fix styles
    Compute,   ///< compute styles
    Dump,      ///< dump styles
    Atom,      ///< atom styles
    Pair,      ///< pair styles
    Bond,      ///< bond styles
    Angle,     ///< angle styles
    Dihedral,  ///< dihedral styles
    Improper,  ///< improper styles
    Kspace,    ///< kspace styles
    Region,    ///< region styles
    Integrate, ///< integrator styles (run_style)
    Minimize,  ///< minimizer styles (min_style)
    Variable,  ///< variable command styles (static set)
    Units,     ///< units command arguments (static set)
    Extra,     ///< read_data / create_box "extra/..." keywords (static set)
    Color,     ///< color names accepted by the dump image command (injected)
    ImageKw,   ///< keywords of the dump image command (static set)
    None       ///< no style category (used in ArgSpec for non-style roles)
};

/// number of entries in StyleCat usable as array index (excludes None)
constexpr int NUM_STYLE_CATS = static_cast<int>(StyleCat::None);

/**
 * @brief Display category of a command word
 *
 * These are the historically grown highlight classes of the LAMMPS-GUI
 * editor; the mapping of command names to categories is read from the
 * command spec table and must be kept consistent with the colors users
 * are accustomed to.
 */
enum class CmdCat : quint8 {
    Lattice,  ///< system setup / lattice / geometry commands
    Output,   ///< file I/O and output commands
    Modify,   ///< *_modify commands
    Particle, ///< particle, force field, and definition commands
    Run,      ///< run-like commands
    Setup,    ///< settings commands
    Special,  ///< undo / flow control commands
    Other     ///< recognized command without an explicit category entry
};

/**
 * @brief Role of an argument position within a command
 */
enum class ArgRole : quint8 {
    Any,      ///< no specific role; only lexical highlighting applies
    DefineId, ///< an ID defined by this command (fix/compute/dump/region/variable ID)
    GroupId,  ///< reference to an atom group ID
    Style,    ///< a style name; the category is in ArgSpec::cat
    SubStyle, ///< may name a sub-style of a hybrid style; recognized when the
              ///< word is a member of the category's style set, like LAMMPS
              ///< itself separates hybrid sub-styles from their arguments
    File,     ///< a file name argument
    Int,      ///< an integer number is expected (informational in the spec
              ///< table for now; the checker uses its own vetted numeric list)
    Number,   ///< a floating point number is expected (informational, see Int)
    Keyword,  ///< a keyword-like argument
    Label     ///< reference to an ID defined elsewhere (unfix/jump/label targets)
};

/**
 * @brief Role and (for ArgRole::Style) style category of one argument position
 */
struct ArgSpec {
    ArgRole role = ArgRole::Any;   ///< role of this argument position
    StyleCat cat = StyleCat::None; ///< style category when role == ArgRole::Style
};

/**
 * @brief Per-command syntax information loaded from the command spec table
 */
struct CommandSpec {
    QString name;                    ///< command name
    CmdCat category = CmdCat::Other; ///< display category of the command word
    int minArgs     = 0;             ///< minimum number of required arguments
    QVector<ArgSpec> args;           ///< roles of argument positions 1, 2, ...
    bool repeatLast = false;         ///< repeat the last role for excess arguments
};

/**
 * @brief Type of a token produced by tokenizeLine() / findVarRefs()
 */
enum class TokType : quint8 {
    Word,         ///< unquoted (part of a) word
    String,       ///< single- or double-quoted string span
    TripleString, ///< triple-quoted string span
    Comment,      ///< comment from '#' to end of line
    Continuation, ///< trailing '&' line continuation character
    VarRef        ///< $x, ${name}, or $(expr) variable reference (findVarRefs() only)
};

/**
 * @brief One token within a physical line of a LAMMPS input
 *
 * Adjacent tokens with the same argIndex belong to the same argument
 * (words may contain embedded quoted sections).
 */
struct Token {
    int start     = 0;             ///< offset of the first character within the line
    int length    = 0;             ///< number of characters
    TokType type  = TokType::Word; ///< token type
    int argIndex  = -1;    ///< 0 = command word, > 0 argument position, -1 = not an argument
    bool isNumber = false; ///< word parses as a LAMMPS number, range, or type wildcard
    bool hasSubst = false; ///< word contains a '$' variable reference
    bool fragment = false; ///< word is split across a mid-word '&' continuation
};

/**
 * @brief Result of tokenizing one physical line
 */
struct LineTokens {
    QVector<Token> tokens;        ///< tokens in line order
    int outState         = 0;     ///< block state after this line (see SyntaxState)
    bool unbalancedQuote = false; ///< a single/double quote was left unclosed
};

/**
 * @brief Which completion word list applies at a completion location
 */
enum class CompleterKind : quint8 {
    None,      ///< no completion at this location
    Command,   ///< command names (word 0 of a logical line)
    Style,     ///< style names; the category is in CompletionTarget::cat
    Group,     ///< group IDs
    VarName,   ///< variable name references (v_...)
    ComputeId, ///< compute ID references (c_...)
    FixId,     ///< fix ID references (f_...)
    File,      ///< file names
    Extra      ///< read_data / create_box "extra/..." keywords
};

/**
 * @brief Result of LammpsSyntax::completionTarget()
 */
struct CompletionTarget {
    CompleterKind kind = CompleterKind::None; ///< which completer applies
    StyleCat cat       = StyleCat::None;      ///< style category when kind == Style
    int wordStart      = -1;                  ///< start column of the word under the cursor
    int wordLength     = 0;                   ///< length of the word under the cursor
};

/**
 * @brief Packing helpers for the per-block syntax state integer
 *
 * The state is stored in QSyntaxHighlighter block states and threaded
 * through tokenizeLine() so multi-line constructs (triple-quoted strings,
 * '&' line continuations, quoted strings spanning continuations) are
 * lexed consistently.  Layout: bits 0-5 flags, bits 6-18 command spec
 * index + 1 (0 = no spec), bits 19-26 number of arguments consumed so far.
 */
namespace SyntaxState {

constexpr int TRIPLE   = 1 << 0; ///< inside an open triple-quoted string
constexpr int CONTINUE = 1 << 1; ///< previous line ended with '&': logical line continues
constexpr int MIDWORD  = 1 << 2; ///< the '&' directly followed a word character
constexpr int COMMENT  = 1 << 3; ///< the continuation is inside a comment
constexpr int SINGLEQ  = 1 << 4; ///< inside an open single-quoted string
constexpr int DOUBLEQ  = 1 << 5; ///< inside an open double-quoted string

constexpr int FLAG_MASK = 0x3f;     ///< mask covering all flag bits
constexpr int CMD_SHIFT = 6;        ///< bit position of the command spec index
constexpr int CMD_MASK  = 0x1fff;   ///< 13 bits for the command spec index + 1
constexpr int ARG_SHIFT = 19;       ///< bit position of the argument counter
constexpr int ARG_MASK  = 0xff;     ///< 8 bits for the argument counter
constexpr int ARG_MAXX  = ARG_MASK; ///< saturation value of the argument counter

/// extract the flag bits from a block state
inline int flags(int state)
{
    return state & FLAG_MASK;
}

/// extract the command spec index from a block state (-1 if none)
inline int cmdIndex(int state)
{
    return ((state >> CMD_SHIFT) & CMD_MASK) - 1;
}

/// extract the number of arguments consumed so far from a block state
inline int argsUsed(int state)
{
    return (state >> ARG_SHIFT) & ARG_MASK;
}

/// combine flags, command spec index (-1 for none), and argument count into a block state
inline int pack(int flagbits, int cmdidx, int nargs)
{
    return (flagbits & FLAG_MASK) | (((cmdidx + 1) & CMD_MASK) << CMD_SHIFT) |
           ((qMin(nargs, ARG_MAXX) & ARG_MASK) << ARG_SHIFT);
}

/// replace the command spec index in a block state (-1 for none)
inline int withCommand(int state, int cmdidx)
{
    return (state & ~(CMD_MASK << CMD_SHIFT)) | (((cmdidx + 1) & CMD_MASK) << CMD_SHIFT);
}

/// replace the number of arguments consumed so far in a block state; used to
/// renumber the arguments when a command embeds a second command (the
/// "modify" section of write_dump, the "dump" section of rerun)
inline int withArgs(int state, int nargs)
{
    return (state & ~(ARG_MASK << ARG_SHIFT)) | ((qMin(nargs, ARG_MAXX) & ARG_MASK) << ARG_SHIFT);
}

/// true when a line with this previous-line state continues the logical line
/// (an open continuation, triple-quoted block, or quoted string)
inline bool logicalContinues(int state)
{
    return (flags(state) & (TRIPLE | CONTINUE | SINGLEQ | DOUBLEQ)) != 0;
}

} // namespace SyntaxState

/**
 * @brief Split one physical line of a LAMMPS input into tokens with spans
 *
 * Mirrors the parsing rules of the LAMMPS Input class (lammps/src/input.cpp):
 * trailing whitespace is trimmed before the '&' continuation check, '#'
 * starts a comment unless it appears inside a quoted string, single, double,
 * and triple quotes group text, and quoted strings may span '&' continuations.
 * Multi-line state (open triple quote, continuation, open quote) is carried
 * in the state integer, see SyntaxState.
 *
 * The command spec index bits of the returned state are preserved from
 * @p prevState while a logical line continues and reset to "none" when a
 * new logical line starts; callers that track the active command patch the
 * state with SyntaxState::withCommand() before storing it.
 *
 * @param text physical line (no trailing newline)
 * @param prevState block state after the previous line (-1 or 0 for none)
 * @return tokens with spans and the state after this line
 */
LineTokens tokenizeLine(const QString &text, int prevState = 0);

/**
 * @brief Find $x, ${name}, and $(expression) variable references
 *
 * Follows the LAMMPS substitution rules: a '$' preceded by a backslash is
 * escaped, '${' extends to the closing brace, '$(' extends to the balanced
 * closing parenthesis (an optional ':%format' suffix is part of the
 * expression), otherwise the single following character names the variable.
 * References are reported everywhere including inside quoted strings, since
 * commands like print, if, and python substitute their quoted arguments.
 *
 * @param text text to scan
 * @return VarRef tokens with spans in scan order
 */
QVector<Token> findVarRefs(const QString &text);

/**
 * @brief Collect the full text of each argument of a tokenized line
 *
 * Joins the tokens belonging to the same argument (words may consist of
 * several tokens when they contain quoted sections), skipping comment and
 * continuation tokens.
 *
 * @param lt result of tokenizeLine() for the line
 * @param line the tokenized line
 * @return map from argument index to the argument's full text
 */
QHash<int, QString> argumentTexts(const LineTokens &lt, const QString &line);

/**
 * @brief Check whether a word is a number the way the LAMMPS-GUI editor colors them
 *
 * Accepts integers, floating point numbers with optional e/E/d/D exponent,
 * and integer ranges / type wildcards built from digits, ':', and '*'.
 *
 * @param word word to test
 * @return true if the word is number-like
 */
bool isNumberWord(const QString &word);

/**
 * @brief Registry of LAMMPS syntax data for highlighting, completion, and checking
 *
 * Holds the sets of valid command and style names for each StyleCat plus the
 * per-command argument role table.  The name sets are populated by injection
 * (normally from LAMMPS library introspection right after the LAMMPS instance
 * is created; from literal lists in unit tests), so this class has no
 * dependency on the LAMMPS library.  The role table is loaded from the
 * command spec resource table; additional tables can be loaded on top and
 * override earlier entries.
 */
class LammpsSyntax {
public:
    /** @brief Constructor; seeds the static keyword sets */
    LammpsSyntax();

    /** @brief Destructor */
    ~LammpsSyntax() = default;

    LammpsSyntax(const LammpsSyntax &)            = delete;
    LammpsSyntax(LammpsSyntax &&)                 = delete;
    LammpsSyntax &operator=(const LammpsSyntax &) = delete;
    LammpsSyntax &operator=(LammpsSyntax &&)      = delete;

    /**
     * @brief Set the full (unfiltered) list of valid names for one category
     *
     * The unfiltered set is used for name validity checks; a sorted list with
     * accelerator-suffixed variants removed is derived for completions.
     *
     * @param cat category to populate
     * @param names full list of valid names for the category
     */
    void setStyles(StyleCat cat, const QStringList &names);

    /// convenience alias for setStyles(StyleCat::Command, names)
    void setCommands(const QStringList &names) { setStyles(StyleCat::Command, names); }

    /**
     * @brief Load a command spec table from a file or Qt resource
     * @param path file system path or Qt resource path of the table
     * @return true if the file could be read and contained no malformed entries
     */
    bool loadCommandSpecs(const QString &path);

    /**
     * @brief Load a command spec table from a string (tests, future overlays)
     *
     * Later entries override earlier entries with the same command name.
     * Malformed lines are skipped.
     *
     * @param text complete table text
     * @return true if no malformed entries were found
     */
    bool loadCommandSpecsFromString(const QString &text);

    /// true once command names have been populated; gates unknown-name checks
    bool isPopulated() const { return !styles[static_cast<int>(StyleCat::Command)].isEmpty(); }

    /// check whether a word is a known command name
    bool knownCommand(const QString &word) const { return knownStyle(StyleCat::Command, word); }

    /// check whether a name is in the full style set of a category
    bool knownStyle(StyleCat cat, const QString &name) const;

    /// check for the special argument words (INF, EDGE, NULL, SELF, if/then/else/elif)
    bool isSpecialWord(const QString &word) const { return specialWords.contains(word); }

    /// display category for a command word (CmdCat::Other if not in the table)
    CmdCat commandCategory(const QString &cmd) const;

    /// stable spec table index for a command (-1 if the command has no spec entry)
    int commandIndex(const QString &cmd) const { return specIndex.value(cmd, -1); }

    /// spec table entry by index (nullptr for invalid index)
    const CommandSpec *spec(int index) const;

    /// role of argument position @p argIndex (>= 1) of command spec @p cmdIndex
    ArgSpec argSpec(int cmdIndex, int argIndex) const;

    /**
     * @brief Sorted completion word list for a category
     *
     * Accelerator-suffixed style variants are filtered out.
     * @param cat category of the word list
     * @param withNone prepend the "none" style (pair/bond/... styles)
     */
    QStringList completionList(StyleCat cat, bool withNone) const;

    /**
     * @brief Determine which completion applies at a cursor position
     *
     * Tokenizes the line with the given previous block state, locates the
     * word under the cursor, and classifies it: the command completer on
     * word 0 of a fresh logical line, and per-argument completers from the
     * command spec table (style categories, group IDs, file names) plus the
     * v_/c_/f_ reference prefixes and the read_data extra keywords.  Because
     * the previous block state carries the active command across '&' line
     * continuations, completion works on continuation lines as well.
     *
     * @param prevBlockState block state after the previous line (-1 or 0 for none)
     * @param line physical line the cursor is on
     * @param cursorCol column of the cursor within the line
     * @return completer kind, style category, and the word under the cursor
     */
    CompletionTarget completionTarget(int prevBlockState, const QString &line, int cursorCol) const;

private:
    QSet<QString> styles[NUM_STYLE_CATS];    ///< full name sets per category
    QStringList completions[NUM_STYLE_CATS]; ///< sorted, suffix-filtered lists
    QSet<QString> specialWords;              ///< special argument words
    QVector<CommandSpec> specs;              ///< command spec table
    QHash<QString, int> specIndex;           ///< command name -> spec table index
};

/**
 * @brief Assemble logical LAMMPS commands from the physical lines of a buffer
 *
 * Feeds physical lines through tokenizeLine() while threading the multi-line
 * state, joins '&' continuations (including mid-word joins and quoted strings
 * spanning lines), and reports each completed logical command as a list of
 * words with their original line numbers.  Comment and empty lines produce no
 * commands.  Tokenizer-level problems (unbalanced quotes, an unclosed
 * triple-quoted block, or a dangling '&' on the final line) are collected as
 * diagnostics.  This is the input-scanning building block for whole-buffer
 * consumers such as the pre-run syntax checker; the highlighter works on the
 * per-block state directly instead.
 */
class InputScanner {
public:
    /// one whitespace-separated word of a logical command
    struct Word {
        QString text;          ///< word text; surrounding matching quotes stripped
        int line      = 0;     ///< line number of the word's first character
        bool quoted   = false; ///< word contains (or is) a quoted section
        bool hasSubst = false; ///< word contains a '$' variable reference
    };
    /// one logical command with continuations joined
    struct LogicalCommand {
        int firstLine = 0;   ///< line number of the first physical line
        int lastLine  = 0;   ///< line number of the last physical line
        QVector<Word> words; ///< words in command order; words[0] is the command
    };
    /// tokenizer-level problems found while scanning
    enum class Diag : quint8 {
        UnbalancedQuote,     ///< a single/double quote was left unclosed
        UnclosedTriple,      ///< a triple-quoted block was still open at the end
        DanglingContinuation ///< the final line ended with a '&' continuation
    };
    /// one diagnostic with the line it was detected on
    struct Diagnostic {
        Diag what = Diag::UnbalancedQuote; ///< kind of problem
        int line  = 0;                     ///< 1-based line number
    };

    /** @brief Constructor */
    InputScanner() = default;

    /** @brief Destructor */
    ~InputScanner() = default;

    InputScanner(const InputScanner &)            = default; ///< Copy constructor
    InputScanner(InputScanner &&)                 = default; ///< Move constructor
    InputScanner &operator=(const InputScanner &) = default; ///< Copy assignment
    InputScanner &operator=(InputScanner &&)      = default; ///< Move assignment

    /// scan a whole buffer (split on newlines, 1-based line numbers); results
    /// accumulate, so use a fresh scanner for every buffer
    void scan(const QString &buffer);

    /// completed logical commands in buffer order
    const QVector<LogicalCommand> &commands() const { return cmds; }

    /// diagnostics in detection order
    const QVector<Diagnostic> &diagnostics() const { return diags; }

private:
    /// feed the next physical line; line numbers must be fed in increasing order
    void feedLine(int lineNo, const QString &text);

    /// finalize after the last line: flush a pending command, emit end-of-input diagnostics
    void finish(int lastLineNo);

    /// append the assembled logical command (if any) to the results
    void flush();

    QVector<LogicalCommand> cmds; ///< completed logical commands
    QVector<Diagnostic> diags;    ///< collected diagnostics
    LogicalCommand current;       ///< logical command being assembled
    int state = 0;                ///< tokenizer state threaded between lines
};

#endif // LAMMPSSYNTAX_H

// Local Variables:
// c-basic-offset: 4
// End:
