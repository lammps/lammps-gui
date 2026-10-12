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

#include "highlighter.h"

#include "helpers.h"

#include <QHash>
#include <QPalette>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace {

// c_/f_/v_/i_/d_ style references to computes, fixes, variables, and properties
bool isReferenceWord(const QString &word)
{
    static const char *prefixes[] = {"i2_", "d2_", "c_", "C_", "f_", "F_", "i_", "d_", "v_"};
    for (const auto *prefix : prefixes) {
        const QLatin1String pre(prefix);
        if ((word.size() > pre.size()) && word.startsWith(pre)) return true;
    }
    return false;
}

// relative luminance of a color as defined by WCAG 2
double wcagLuminance(const QColor &color)
{
    const auto linear = [](double channel) {
        return (channel <= 0.04045) ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
}

// WCAG 2 contrast ratio between two colors (1 .. 21)
double contrastRatio(const QColor &one, const QColor &two)
{
    const double lum1 = wcagLuminance(one);
    const double lum2 = wcagLuminance(two);
    return (std::max(lum1, lum2) + 0.05) / (std::min(lum1, lum2) + 0.05);
}

// colors with less contrast than this against the editor background get a chip
constexpr double MIN_COLOR_CONTRAST = 3.0;

} // namespace

Highlighter::Highlighter(const LammpsSyntax *_syntax, QTextDocument *parent) :
    QSyntaxHighlighter(parent), syntax(_syntax)
{
    // one palette table instead of duplicated per-theme assignments; the color
    // values are carried over unchanged from the historically grown scheme
    struct PaletteEntry {
        QColor light;
        QColor dark;
        int lightWeight;
        int darkWeight;
    };
    // must match the order of the Fmt enumeration
    static const PaletteEntry palette[] = {
        {Qt::darkGreen, QColorConstants::Svg::lightgreen, QFont::Bold, QFont::Bold},  // CmdLattice
        {Qt::darkYellow, QColorConstants::Yellow, QFont::Bold, QFont::Bold},          // CmdOutput
        {Qt::magenta, QColorConstants::Svg::lightcoral, QFont::Bold, QFont::Bold},    // CmdModify
        {Qt::darkRed, QColorConstants::Svg::indianred, QFont::Bold, QFont::Bold},     // CmdParticle
        {Qt::darkBlue, QColorConstants::Svg::lightskyblue, QFont::Bold, QFont::Bold}, // CmdRun
        {Qt::darkCyan, QColorConstants::Cyan, QFont::Bold, QFont::Bold},              // CmdSetup
        {Qt::darkMagenta, QColorConstants::Magenta, QFont::Bold, QFont::Bold},        // CmdSpecial
        {QColor(), QColor(), QFont::Bold, QFont::Bold},                               // CmdOther
        {Qt::blue, QColorConstants::Svg::dodgerblue, QFont::Normal, QFont::Normal},   // Number
        {Qt::darkGreen, QColorConstants::Green, QFont::Normal, QFont::Normal},        // String
        {Qt::red, QColorConstants::Red, QFont::Normal, QFont::Bold},                  // Comment
        {Qt::darkGray, QColorConstants::Svg::lightgray, QFont::Bold, QFont::Bold},    // Variable
        {Qt::darkMagenta, QColorConstants::Magenta, QFont::Bold, QFont::Bold},        // Special
        {QColorConstants::Svg::steelblue, QColorConstants::Svg::cornflowerblue, QFont::Bold,
         QFont::Bold}, // SubStyle
    };
    static_assert(sizeof(palette) / sizeof(palette[0]) == static_cast<int>(Fmt::Count),
                  "palette table must match the Fmt enumeration");

    const bool light = isLightTheme();
    for (int i = 0; i < static_cast<int>(Fmt::Count); ++i) {
        const QColor color = light ? palette[i].light : palette[i].dark;
        if (color.isValid()) formats[i].setForeground(color);
        formats[i].setFontWeight(light ? palette[i].lightWeight : palette[i].darkWeight);
    }
    unknownColor     = light ? QColor(Qt::red) : QColor(QColorConstants::Svg::orangered);
    editorBackground = QPalette().color(QPalette::Active, QPalette::Base);
}

const QTextCharFormat &Highlighter::colorFormat(const QString &name)
{
    const auto found = colorFormats.constFind(name);
    if (found != colorFormats.constEnd()) return *found;

    const QColor color(name);
    if (!color.isValid()) return *colorFormats.insert(name, formats[static_cast<int>(Fmt::String)]);
    QTextCharFormat fmt;
    fmt.setForeground(color);
    fmt.setFontWeight(QFont::Bold);
    // when the color does not have enough contrast against the editor
    // background, put it on the dark or light chip, whichever contrasts
    // better with the color itself
    if (contrastRatio(color, editorBackground) < MIN_COLOR_CONTRAST) {
        const QColor darkChip(32, 32, 32);
        const QColor lightChip(232, 232, 232);
        fmt.setBackground(contrastRatio(color, darkChip) >= contrastRatio(color, lightChip)
                              ? darkChip
                              : lightChip);
    }
    return *colorFormats.insert(name, fmt);
}

const QTextCharFormat &Highlighter::cmdFormat(CmdCat cat) const
{
    switch (cat) {
        case CmdCat::Lattice:
            return formats[static_cast<int>(Fmt::CmdLattice)];
        case CmdCat::Output:
            return formats[static_cast<int>(Fmt::CmdOutput)];
        case CmdCat::Modify:
            return formats[static_cast<int>(Fmt::CmdModify)];
        case CmdCat::Particle:
            return formats[static_cast<int>(Fmt::CmdParticle)];
        case CmdCat::Run:
            return formats[static_cast<int>(Fmt::CmdRun)];
        case CmdCat::Setup:
            return formats[static_cast<int>(Fmt::CmdSetup)];
        case CmdCat::Special:
            return formats[static_cast<int>(Fmt::CmdSpecial)];
        case CmdCat::Other:
        default:
            return formats[static_cast<int>(Fmt::CmdOther)];
    }
}

void Highlighter::setCursorPos(int blockNumber, int column)
{
    if ((blockNumber == cursorBlock) && (column == cursorColumn)) return;
    const int oldBlock = cursorBlock;
    cursorBlock        = blockNumber;
    cursorColumn       = column;
    if (!document()) return;

    // re-highlight the block the cursor left and the one it is in, so the
    // unknown-name suppression follows the cursor
    if ((oldBlock >= 0) && (oldBlock != blockNumber)) {
        const auto block = document()->findBlockByNumber(oldBlock);
        if (block.isValid()) rehighlightBlock(block);
    }
    const auto block = document()->findBlockByNumber(blockNumber);
    if (block.isValid()) rehighlightBlock(block);
}

void Highlighter::highlightBlock(const QString &text)
{
    const int prev       = qMax(previousBlockState(), 0);
    const LineTokens lt  = tokenizeLine(text, prev);
    const bool freshLine = !SyntaxState::logicalContinues(prev);

    // full text of each argument (a word may consist of several tokens)
    const QHash<int, QString> argText = argumentTexts(lt, text);

    // the active command: word 0 on a fresh logical line, carried in the block
    // state on continuation lines
    const int cmdIdx = freshLine ? syntax->commandIndex(argText.value(0))
                                 : SyntaxState::cmdIndex(prev);

    const bool checkNames = syntax->isPopulated();
    const bool cursorHere = (currentBlock().blockNumber() == cursorBlock);

    // dump image / dump_modify lines show color names in their actual color
    // and mark the dump image keywords.  On continuation lines the dump style
    // is not visible, so the keyword marking is not restricted there.
    const QString cmdName = freshLine
                                ? argText.value(0)
                                : (syntax->spec(cmdIdx) ? syntax->spec(cmdIdx)->name : QString());
    const auto isDumpish  = [](const QString &name) {
        return (name == QStringLiteral("dump")) || (name == QStringLiteral("dump_modify"));
    };
    const bool dumpish = isDumpish(cmdName);
    const bool imageKeywords =
        (cmdName == QStringLiteral("dump")) &&
        (!argText.contains(3) || (argText.value(3) == QStringLiteral("image")) ||
         (argText.value(3) == QStringLiteral("movie")));

    // "write_dump ... modify ..." embeds a dump_modify command and
    // "rerun ... dump ..." a read_dump command.  The keyword splitting off the
    // embedded section is shown in the color of the command it stands for and
    // the arguments after it are those of the embedded command.
    QString splitWord;     // keyword that starts the embedded command
    QString splitCommand;  // command the keyword stands for
    int splitFirstArg = 0; // earliest argument position of the keyword
    int splitBaseArg  = 0; // position the first embedded argument corresponds to
    if (cmdName == QStringLiteral("write_dump")) {
        splitWord     = QStringLiteral("modify");
        splitCommand  = QStringLiteral("dump_modify");
        splitFirstArg = 4; // group-ID, dump style, and file name come first
        splitBaseArg  = 2; // the dump ID of a dump_modify command is implicit
    } else if (cmdName == QStringLiteral("rerun")) {
        splitWord     = QStringLiteral("dump");
        splitCommand  = QStringLiteral("read_dump");
        splitFirstArg = 2; // at least one dump file name comes first
        splitBaseArg  = 3; // file name and time step of read_dump are implicit
    }

    // position of the splitting keyword (LAMMPS splits at its first
    // occurrence) and the spec of the command it embeds
    int splitArg = -1;
    int embedIdx = -1;
    if (!splitWord.isEmpty()) {
        for (auto it = argText.constBegin(); it != argText.constEnd(); ++it)
            if ((it.key() >= splitFirstArg) && (it.value() == splitWord) &&
                ((splitArg < 0) || (it.key() < splitArg)))
                splitArg = it.key();
        if (splitArg >= 0) embedIdx = syntax->commandIndex(splitCommand);
    }
    const bool embedDumpish = isDumpish(splitCommand);
    // argument position within the embedded command
    const auto embedArg = [&](int arg) {
        return arg - splitArg - 1 + splitBaseArg;
    };

    for (const auto &tok : lt.tokens) {
        QTextCharFormat fmt;
        bool unknown = false;

        switch (tok.type) {
            case TokType::Comment:
                fmt = formats[static_cast<int>(Fmt::Comment)];
                break;
            case TokType::Continuation:
                fmt = formats[static_cast<int>(Fmt::Special)];
                break;
            case TokType::String:
            case TokType::TripleString:
                fmt = formats[static_cast<int>(Fmt::String)];
                break;
            case TokType::Word: {
                const QString word = argText.value(tok.argIndex);
                if (tok.argIndex == 0) {
                    fmt = cmdFormat(syntax->commandCategory(word));
                    unknown =
                        checkNames && !tok.hasSubst && !tok.fragment && !syntax->knownCommand(word);
                } else if (tok.argIndex > 0) {
                    // after the splitting keyword the arguments are those of
                    // the embedded command and use its argument numbering
                    const bool embedded = (splitArg >= 0) && (tok.argIndex > splitArg);
                    const ArgSpec spec  = embedded
                                              ? syntax->argSpec(embedIdx, embedArg(tok.argIndex))
                                              : syntax->argSpec(cmdIdx, tok.argIndex);
                    switch (spec.role) {
                        case ArgRole::DefineId:
                        case ArgRole::Label:
                            // IDs get the same color whether defined here or
                            // referenced (unfix, *_modify, jump targets)
                            fmt = formats[static_cast<int>(Fmt::Number)];
                            break;
                        case ArgRole::GroupId:
                        case ArgRole::File:
                        case ArgRole::Keyword:
                            fmt = formats[static_cast<int>(Fmt::String)];
                            break;
                        case ArgRole::Style:
                            // style names use the same color as run-like commands
                            fmt     = formats[static_cast<int>(Fmt::CmdRun)];
                            unknown = checkNames && !tok.hasSubst && !tok.fragment &&
                                      !syntax->knownStyle(spec.cat, word);
                            break;
                        case ArgRole::SubStyle:
                            // a word in a sub-style position is a sub-style exactly
                            // when it is a known style of the category (the same
                            // membership test LAMMPS uses to parse hybrid style
                            // arguments); other words are arguments of the
                            // sub-styles and are never flagged as unknown
                            if (checkNames && syntax->knownStyle(spec.cat, word))
                                fmt = formats[static_cast<int>(Fmt::SubStyle)];
                            break;
                        default:
                            break;
                    }
                    // lexical classes override the role color
                    if (tok.isNumber)
                        fmt = formats[static_cast<int>(Fmt::Number)];
                    else if (syntax->isSpecialWord(word))
                        fmt = formats[static_cast<int>(Fmt::Special)];
                    else if (isReferenceWord(word))
                        fmt = formats[static_cast<int>(Fmt::Variable)];
                    // colors and keywords on dump image / dump_modify lines
                    if (embedded ? embedDumpish : dumpish) {
                        if (syntax->knownStyle(StyleCat::Color, word))
                            fmt = colorFormat(word);
                        else if (imageKeywords && (tok.argIndex >= 8) &&
                                 syntax->knownStyle(StyleCat::ImageKw, word))
                            fmt = formats[static_cast<int>(Fmt::String)];
                    }
                    // the keyword splitting off an embedded command
                    if (tok.argIndex == splitArg)
                        fmt = cmdFormat(syntax->commandCategory(splitCommand));
                }
                break;
            }
            default:
                break;
        }

        // no unknown-name marker on the word the cursor is on (partially typed)
        if (unknown && cursorHere && (cursorColumn >= tok.start) &&
            (cursorColumn <= tok.start + tok.length))
            unknown = false;
        if (unknown) {
            fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            fmt.setUnderlineColor(unknownColor);
        }
        setFormat(tok.start, tok.length, fmt);
    }

    // overlay $-variable references; they are substituted everywhere except in
    // comments (commands like print and if substitute their quoted arguments)
    const auto refs = findVarRefs(text);
    for (const auto &ref : refs) {
        bool inComment = false;
        for (const auto &tok : lt.tokens) {
            if ((tok.type == TokType::Comment) && (ref.start >= tok.start) &&
                (ref.start < tok.start + tok.length)) {
                inComment = true;
                break;
            }
        }
        if (!inComment) setFormat(ref.start, ref.length, formats[static_cast<int>(Fmt::Variable)]);
    }

    // store the block state; carry the active command into a continuation
    int out = lt.outState;
    if (freshLine && (out != 0) && (cmdIdx >= 0)) out = SyntaxState::withCommand(out, cmdIdx);
    // once the splitting keyword has been seen, the rest of the logical line
    // continues as the embedded command in its own argument numbering
    if ((out != 0) && (splitArg >= 0) && (embedIdx >= 0)) {
        out = SyntaxState::withCommand(out, embedIdx);
        out = SyntaxState::withArgs(out, embedArg(SyntaxState::argsUsed(out)));
    }
    setCurrentBlockState(out);
}

// Local Variables:
// c-basic-offset: 4
// End:
