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

#include "lammpssyntax.h"

#include <gtest/gtest.h>

#include <QMap>
#include <QString>
#include <QStringList>

namespace {

// reassemble the whitespace-separated words of a line from its tokens
QStringList tokenWords(const QString &line, int state = 0)
{
    const LineTokens lt = tokenizeLine(line, state);
    QMap<int, QString> byArg;
    for (const auto &tok : lt.tokens) {
        if (tok.argIndex < 0) continue;
        if ((tok.type == TokType::Comment) || (tok.type == TokType::Continuation)) continue;
        byArg[tok.argIndex] += line.mid(tok.start, tok.length);
    }
    QStringList words;
    for (const auto &word : byArg)
        words << word;
    return words;
}

// find the n-th token of the given type (or nullptr)
const Token *findToken(const LineTokens &lt, TokType type, int nth = 0)
{
    for (const auto &tok : lt.tokens) {
        if (tok.type == type) {
            if (nth == 0) return &tok;
            --nth;
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// tokenizer: basic lines

TEST(LammpsSyntaxTest, TokenizeSimpleCommand)
{
    const QString line  = QStringLiteral("fix 1 all nvt temp 300");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"fix", "1", "all", "nvt", "temp", "300"}));
    ASSERT_FALSE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.tokens[0].start, 0);
    EXPECT_EQ(lt.tokens[0].length, 3);
    EXPECT_EQ(lt.tokens[0].argIndex, 0);
    EXPECT_EQ(lt.tokens[1].argIndex, 1);
    EXPECT_TRUE(lt.tokens[1].isNumber);
    EXPECT_FALSE(lt.tokens[3].isNumber);
    EXPECT_EQ(lt.outState, 0);
    EXPECT_FALSE(lt.unbalancedQuote);
}

TEST(LammpsSyntaxTest, TokenizeLeadingWhitespace)
{
    const QString line  = QStringLiteral("  \t run 100");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"run", "100"}));
    EXPECT_EQ(lt.tokens[0].start, 4);
    EXPECT_EQ(lt.tokens[0].argIndex, 0);
}

TEST(LammpsSyntaxTest, TokenizeEmptyLine)
{
    LineTokens lt = tokenizeLine(QString());
    EXPECT_TRUE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.outState, 0);
    lt = tokenizeLine(QStringLiteral("   \t  "));
    EXPECT_TRUE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.outState, 0);
}

// ---------------------------------------------------------------------------
// tokenizer: comments

TEST(LammpsSyntaxTest, TokenizeCommentLine)
{
    const LineTokens lt = tokenizeLine(QStringLiteral("# a comment line"));
    ASSERT_EQ(lt.tokens.size(), 1);
    EXPECT_EQ(lt.tokens[0].type, TokType::Comment);
    EXPECT_EQ(lt.tokens[0].argIndex, -1);
    EXPECT_EQ(lt.outState, 0);
}

TEST(LammpsSyntaxTest, TokenizeTrailingComment)
{
    const QString line  = QStringLiteral("run 100 # do it");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"run", "100"}));
    const Token *comment = findToken(lt, TokType::Comment);
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->start, 8);
    EXPECT_EQ(comment->start + comment->length, line.size());
}

TEST(LammpsSyntaxTest, TokenizeHashInsideQuotesNotComment)
{
    const QString line  = QStringLiteral("print 'a # b' \"c # d\"");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(findToken(lt, TokType::Comment), nullptr);
    EXPECT_EQ(tokenWords(line), (QStringList{"print", "'a # b'", "\"c # d\""}));
}

// ---------------------------------------------------------------------------
// tokenizer: quotes

TEST(LammpsSyntaxTest, TokenizeSingleAndDoubleQuotes)
{
    const QString line  = QStringLiteral("print \"hello world\" 'single quoted'");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"print", "\"hello world\"", "'single quoted'"}));
    EXPECT_EQ(findToken(lt, TokType::String, 0)->argIndex, 1);
    EXPECT_EQ(findToken(lt, TokType::String, 1)->argIndex, 2);
    EXPECT_EQ(lt.outState, 0);
}

TEST(LammpsSyntaxTest, TokenizeEscapedQuotes)
{
    const QString line = QStringLiteral("print \"a \\\" b\" 'c \\' d'");
    EXPECT_EQ(tokenWords(line), (QStringList{"print", "\"a \\\" b\"", "'c \\' d'"}));
}

TEST(LammpsSyntaxTest, TokenizeEmbeddedQuoteSameArgument)
{
    const QString line  = QStringLiteral("print abc\"d e\"f");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"print", "abc\"d e\"f"}));
    // three tokens (Word, String, Word) share argument index 1
    int count = 0;
    for (const auto &tok : lt.tokens)
        if (tok.argIndex == 1) ++count;
    EXPECT_EQ(count, 3);
}

TEST(LammpsSyntaxTest, TokenizeUnbalancedQuote)
{
    LineTokens lt = tokenizeLine(QStringLiteral("print \"abc"));
    EXPECT_TRUE(lt.unbalancedQuote);
    EXPECT_EQ(lt.outState, 0); // self-heals for the next line
    lt = tokenizeLine(QStringLiteral("print 'abc"));
    EXPECT_TRUE(lt.unbalancedQuote);
    EXPECT_EQ(lt.outState, 0);
}

// ---------------------------------------------------------------------------
// tokenizer: triple quotes

TEST(LammpsSyntaxTest, TokenizeTripleQuoteSameLine)
{
    const QString line  = QStringLiteral("print \"\"\"a b\"\"\" tail");
    const LineTokens lt = tokenizeLine(line);
    EXPECT_EQ(tokenWords(line), (QStringList{"print", "\"\"\"a b\"\"\"", "tail"}));
    const Token *triple = findToken(lt, TokType::TripleString);
    ASSERT_NE(triple, nullptr);
    EXPECT_EQ(triple->argIndex, 1);
    EXPECT_EQ(lt.outState, 0);
}

TEST(LammpsSyntaxTest, TokenizeTripleQuoteSpansLines)
{
    // opening line
    LineTokens lt = tokenizeLine(QStringLiteral("print \"\"\"first line"));
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);
    EXPECT_EQ(SyntaxState::argsUsed(lt.outState), 1);

    // middle line: whole line is string content
    const int mid = lt.outState;
    lt            = tokenizeLine(QStringLiteral("middle content"), mid);
    ASSERT_EQ(lt.tokens.size(), 1);
    EXPECT_EQ(lt.tokens[0].type, TokType::TripleString);
    EXPECT_EQ(lt.tokens[0].argIndex, 1);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);

    // empty line inside the block preserves the state
    lt = tokenizeLine(QString(), lt.outState);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);

    // closing line with a word after the block
    lt                  = tokenizeLine(QStringLiteral("last\"\"\" tail"), lt.outState);
    const Token *triple = findToken(lt, TokType::TripleString);
    ASSERT_NE(triple, nullptr);
    EXPECT_EQ(triple->start, 0);
    EXPECT_EQ(triple->length, 7);
    EXPECT_EQ(lt.outState, 0);
    EXPECT_EQ(tokenWords(QStringLiteral("last\"\"\" tail"), mid),
              (QStringList{"last\"\"\"", "tail"}));
}

TEST(LammpsSyntaxTest, TokenizeFourQuotesNotTripleOpener)
{
    const LineTokens lt = tokenizeLine(QStringLiteral("print \"\"\"\"a\" x"));
    EXPECT_FALSE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);
}

TEST(LammpsSyntaxTest, TokenizeAmpersandInsideTripleIsContent)
{
    // per Commands_parse.rst a trailing '&' does not continue inside """ blocks
    LineTokens lt = tokenizeLine(QStringLiteral("print \"\"\"open &"));
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);
    EXPECT_FALSE(SyntaxState::flags(lt.outState) & SyntaxState::CONTINUE);
    EXPECT_EQ(findToken(lt, TokType::Continuation), nullptr);

    lt = tokenizeLine(QStringLiteral("content &"), lt.outState);
    EXPECT_EQ(findToken(lt, TokType::Continuation), nullptr);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::TRIPLE);
}

// ---------------------------------------------------------------------------
// tokenizer: '&' continuation

TEST(LammpsSyntaxTest, TokenizeContinuation)
{
    const QString line  = QStringLiteral("fix 1 all nvt &");
    const LineTokens lt = tokenizeLine(line);
    const Token *cont   = findToken(lt, TokType::Continuation);
    ASSERT_NE(cont, nullptr);
    EXPECT_EQ(cont->start, line.size() - 1);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::CONTINUE);
    EXPECT_FALSE(SyntaxState::flags(lt.outState) & SyntaxState::MIDWORD);
    EXPECT_EQ(SyntaxState::argsUsed(lt.outState), 3);
    EXPECT_EQ(SyntaxState::cmdIndex(lt.outState), -1); // caller patches the command
}

TEST(LammpsSyntaxTest, TokenizeContinuationWithTrailingWhitespace)
{
    // trailing whitespace after the '&' must not defeat the continuation
    const LineTokens lt = tokenizeLine(QStringLiteral("fix 1 all nvt &  \t "));
    EXPECT_NE(findToken(lt, TokType::Continuation), nullptr);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::CONTINUE);
}

TEST(LammpsSyntaxTest, TokenizeContinuationLineArguments)
{
    const int state     = SyntaxState::pack(SyntaxState::CONTINUE, 7, 3);
    const QString line  = QStringLiteral("  temp 300 300 100");
    const LineTokens lt = tokenizeLine(line, state);
    ASSERT_FALSE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.tokens[0].argIndex, 4); // continues after 3 consumed arguments
    EXPECT_EQ(lt.tokens[1].argIndex, 5);
    EXPECT_EQ(lt.outState, 0); // logical line complete
}

TEST(LammpsSyntaxTest, TokenizeMidwordJoin)
{
    LineTokens lt = tokenizeLine(QStringLiteral("pair_style lj/&"));
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::MIDWORD);
    const Token *frag = findToken(lt, TokType::Word, 1);
    ASSERT_NE(frag, nullptr);
    EXPECT_TRUE(frag->fragment);

    // continuation line starting without whitespace continues the same word
    const int patched = SyntaxState::withCommand(lt.outState, 3);
    EXPECT_EQ(SyntaxState::cmdIndex(patched), 3);
    lt = tokenizeLine(QStringLiteral("cut 2.5"), patched);
    ASSERT_GE(lt.tokens.size(), 2);
    EXPECT_EQ(lt.tokens[0].argIndex, 1);
    EXPECT_TRUE(lt.tokens[0].fragment);
    EXPECT_EQ(lt.tokens[1].argIndex, 2);
    EXPECT_TRUE(lt.tokens[1].isNumber);
    EXPECT_EQ(lt.outState, 0);
}

TEST(LammpsSyntaxTest, TokenizeMidwordStateWithLeadingWhitespaceStartsNewArg)
{
    const int state     = SyntaxState::pack(SyntaxState::CONTINUE | SyntaxState::MIDWORD, -1, 1);
    const LineTokens lt = tokenizeLine(QStringLiteral("  cut 2.5"), state);
    ASSERT_FALSE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.tokens[0].argIndex, 2);
    EXPECT_FALSE(lt.tokens[0].fragment);
}

TEST(LammpsSyntaxTest, TokenizeCommentContinuation)
{
    LineTokens lt = tokenizeLine(QStringLiteral("# comment with continuation &"));
    EXPECT_NE(findToken(lt, TokType::Comment), nullptr);
    EXPECT_NE(findToken(lt, TokType::Continuation), nullptr);
    const int flags = SyntaxState::flags(lt.outState);
    EXPECT_TRUE(flags & SyntaxState::COMMENT);
    EXPECT_TRUE(flags & SyntaxState::CONTINUE);

    // the continued line stays inside the comment
    lt = tokenizeLine(QStringLiteral("this is still a comment"), lt.outState);
    ASSERT_EQ(lt.tokens.size(), 1);
    EXPECT_EQ(lt.tokens[0].type, TokType::Comment);
    EXPECT_EQ(lt.outState, 0);
}

TEST(LammpsSyntaxTest, TokenizeQuoteSpansContinuation)
{
    // documented pattern: a quoted string spanning lines via '&'
    LineTokens lt = tokenizeLine(QStringLiteral("variable a string \"red green blue &"));
    EXPECT_FALSE(lt.unbalancedQuote);
    const int flags = SyntaxState::flags(lt.outState);
    EXPECT_TRUE(flags & SyntaxState::CONTINUE);
    EXPECT_TRUE(flags & SyntaxState::DOUBLEQ);
    EXPECT_EQ(SyntaxState::argsUsed(lt.outState), 3);

    lt = tokenizeLine(QStringLiteral("  purple orange cyan\""), lt.outState);
    ASSERT_FALSE(lt.tokens.isEmpty());
    EXPECT_EQ(lt.tokens[0].type, TokType::String);
    EXPECT_EQ(lt.tokens[0].argIndex, 3);
    EXPECT_EQ(lt.outState, 0);
    EXPECT_FALSE(lt.unbalancedQuote);
}

TEST(LammpsSyntaxTest, TokenizeLoneAmpersand)
{
    const LineTokens lt = tokenizeLine(QStringLiteral("&"));
    ASSERT_EQ(lt.tokens.size(), 1);
    EXPECT_EQ(lt.tokens[0].type, TokType::Continuation);
    EXPECT_TRUE(SyntaxState::flags(lt.outState) & SyntaxState::CONTINUE);
}

// ---------------------------------------------------------------------------
// variable references

TEST(LammpsSyntaxTest, FindVarRefsForms)
{
    const QString text = QStringLiteral("run $x ${steps} $(v_a+1) $( (a+b)/2 :%.6g) end");
    const auto refs    = findVarRefs(text);
    ASSERT_EQ(refs.size(), 4);
    EXPECT_EQ(text.mid(refs[0].start, refs[0].length), QStringLiteral("$x"));
    EXPECT_EQ(text.mid(refs[1].start, refs[1].length), QStringLiteral("${steps}"));
    EXPECT_EQ(text.mid(refs[2].start, refs[2].length), QStringLiteral("$(v_a+1)"));
    EXPECT_EQ(text.mid(refs[3].start, refs[3].length), QStringLiteral("$( (a+b)/2 :%.6g)"));
    for (const auto &ref : refs)
        EXPECT_EQ(ref.type, TokType::VarRef);
}

TEST(LammpsSyntaxTest, FindVarRefsEscapedAndEdge)
{
    // escaped '\$' is not a reference
    EXPECT_TRUE(findVarRefs(QStringLiteral("print \\$x")).isEmpty());
    // '$' as the last character
    const auto refs = findVarRefs(QStringLiteral("odd$"));
    ASSERT_EQ(refs.size(), 1);
    EXPECT_EQ(refs[0].length, 1);
    // unclosed brace extends to the end of the scan range
    const auto refs2 = findVarRefs(QStringLiteral("a ${open"));
    ASSERT_EQ(refs2.size(), 1);
    EXPECT_EQ(refs2[0].start, 2);
    EXPECT_EQ(refs2[0].length, 6);
}

TEST(LammpsSyntaxTest, FindVarRefsInsideQuotes)
{
    const QString text = QStringLiteral("print \"Volume = $v\"");
    const auto refs    = findVarRefs(text);
    ASSERT_EQ(refs.size(), 1);
    EXPECT_EQ(text.mid(refs[0].start, refs[0].length), QStringLiteral("$v"));
}

// ---------------------------------------------------------------------------
// number recognition

TEST(LammpsSyntaxTest, IsNumberWordAccepts)
{
    for (const auto *word : {"1", "-5", "+3", "0", "1.5", ".5", "1.", "1e5", "1.5e-3", "2E+4",
                             "1.5d0", "2:5", "1:10:2", "*", "2*", "*5", "2*5", "-1.5"})
        EXPECT_TRUE(isNumberWord(QString::fromLatin1(word))) << word;
}

TEST(LammpsSyntaxTest, IsNumberWordRejects)
{
    for (const auto *word :
         {"", "abc", "1a", "e5", "1e", "1e+", "-", "+", "1.5.3", ":", "nvt", "lj/cut", "1.5f"})
        EXPECT_FALSE(isNumberWord(QString::fromLatin1(word))) << word;
}

// ---------------------------------------------------------------------------
// state packing

TEST(LammpsSyntaxTest, SyntaxStateRoundTrip)
{
    const int state = SyntaxState::pack(SyntaxState::CONTINUE | SyntaxState::MIDWORD, 1234, 200);
    EXPECT_EQ(SyntaxState::flags(state), SyntaxState::CONTINUE | SyntaxState::MIDWORD);
    EXPECT_EQ(SyntaxState::cmdIndex(state), 1234);
    EXPECT_EQ(SyntaxState::argsUsed(state), 200);

    // no command
    EXPECT_EQ(SyntaxState::cmdIndex(SyntaxState::pack(0, -1, 0)), -1);
    // argument counter saturates
    EXPECT_EQ(SyntaxState::argsUsed(SyntaxState::pack(0, 0, 9999)), SyntaxState::ARG_MAXX);
    // negative previous state (initial QSyntaxHighlighter value) is treated as empty
    EXPECT_EQ(SyntaxState::flags(qMax(-1, 0)), 0);
}

TEST(LammpsSyntaxTest, SyntaxStateWithCommand)
{
    const int state   = SyntaxState::pack(SyntaxState::CONTINUE, -1, 3);
    const int patched = SyntaxState::withCommand(state, 42);
    EXPECT_EQ(SyntaxState::cmdIndex(patched), 42);
    EXPECT_EQ(SyntaxState::flags(patched), SyntaxState::CONTINUE);
    EXPECT_EQ(SyntaxState::argsUsed(patched), 3);
}

TEST(LammpsSyntaxTest, SyntaxStateWithArgs)
{
    const int state   = SyntaxState::pack(SyntaxState::CONTINUE, 42, 7);
    const int patched = SyntaxState::withArgs(state, 2);
    EXPECT_EQ(SyntaxState::argsUsed(patched), 2);
    EXPECT_EQ(SyntaxState::cmdIndex(patched), 42);
    EXPECT_EQ(SyntaxState::flags(patched), SyntaxState::CONTINUE);
    // the argument counter saturates like in pack()
    EXPECT_EQ(SyntaxState::argsUsed(SyntaxState::withArgs(state, 9999)), SyntaxState::ARG_MAXX);
}

// ---------------------------------------------------------------------------
// registry

TEST(LammpsSyntaxTest, RegistryStyles)
{
    LammpsSyntax syntax;
    EXPECT_FALSE(syntax.isPopulated());
    syntax.setStyles(StyleCat::Fix, {"nvt", "nve", "npt", "nvt/omp", "nvt/kk/device"});
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Fix, "nvt"));
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Fix, "nvt/omp")); // full set keeps suffixed names
    EXPECT_FALSE(syntax.knownStyle(StyleCat::Fix, "bogus"));
    // completion list is sorted and suffix-filtered
    EXPECT_EQ(syntax.completionList(StyleCat::Fix, false), (QStringList{"npt", "nve", "nvt"}));
    // "none" is prepended in sort order and not duplicated
    syntax.setStyles(StyleCat::Pair, {"zero", "lj/cut", "none"});
    const QStringList pairs = syntax.completionList(StyleCat::Pair, true);
    EXPECT_EQ(pairs, (QStringList{"lj/cut", "none", "zero"}));
}

TEST(LammpsSyntaxTest, RegistryNoneIsValidForForceFieldStyles)
{
    const LammpsSyntax syntax;
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Pair, "none"));
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Kspace, "none"));
    EXPECT_FALSE(syntax.knownStyle(StyleCat::Fix, "none"));
}

TEST(LammpsSyntaxTest, RegistryCommandsAndSpecialWords)
{
    LammpsSyntax syntax;
    syntax.setCommands({"run", "fix", "variable"});
    EXPECT_TRUE(syntax.isPopulated());
    EXPECT_TRUE(syntax.knownCommand("run"));
    EXPECT_FALSE(syntax.knownCommand("walk"));
    EXPECT_TRUE(syntax.isSpecialWord("INF"));
    EXPECT_TRUE(syntax.isSpecialWord("then"));
    EXPECT_FALSE(syntax.isSpecialWord("inf"));
    // static sets are seeded by the constructor
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Variable, "equal"));
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Units, "metal"));
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Extra, "extra/bond/types"));
}

TEST(LammpsSyntaxTest, RegistrySpecParsing)
{
    LammpsSyntax syntax;
    const QString table = QStringLiteral("# comment line\n"
                                         "fix particle 3 defid,group,style:fix\n"
                                         "boundary lattice 3 keyword*\n"
                                         "clear setup 0\n"
                                         "broken nosuchcategory 1 any\n");
    EXPECT_FALSE(syntax.loadCommandSpecsFromString(table)); // one malformed line

    const int fixIdx = syntax.commandIndex("fix");
    ASSERT_GE(fixIdx, 0);
    EXPECT_EQ(syntax.commandCategory("fix"), CmdCat::Particle);
    EXPECT_EQ(syntax.spec(fixIdx)->minArgs, 3);
    EXPECT_EQ(syntax.argSpec(fixIdx, 1).role, ArgRole::DefineId);
    EXPECT_EQ(syntax.argSpec(fixIdx, 2).role, ArgRole::GroupId);
    EXPECT_EQ(syntax.argSpec(fixIdx, 3).role, ArgRole::Style);
    EXPECT_EQ(syntax.argSpec(fixIdx, 3).cat, StyleCat::Fix);
    // past the spec without repeatLast: Any
    EXPECT_EQ(syntax.argSpec(fixIdx, 4).role, ArgRole::Any);
    // argument position 0 or invalid index: Any
    EXPECT_EQ(syntax.argSpec(fixIdx, 0).role, ArgRole::Any);
    EXPECT_EQ(syntax.argSpec(-1, 1).role, ArgRole::Any);

    // repeatLast
    const int bndIdx = syntax.commandIndex("boundary");
    ASSERT_GE(bndIdx, 0);
    EXPECT_EQ(syntax.argSpec(bndIdx, 9).role, ArgRole::Keyword);

    // command without roles
    const int clearIdx = syntax.commandIndex("clear");
    ASSERT_GE(clearIdx, 0);
    EXPECT_EQ(syntax.argSpec(clearIdx, 1).role, ArgRole::Any);

    // malformed line was skipped
    EXPECT_EQ(syntax.commandIndex("broken"), -1);
    EXPECT_EQ(syntax.commandCategory("broken"), CmdCat::Other);
}

TEST(LammpsSyntaxTest, RegistrySpecOverride)
{
    LammpsSyntax syntax;
    EXPECT_TRUE(syntax.loadCommandSpecsFromString(QStringLiteral("fix particle 3 defid\n")));
    const int before = syntax.commandIndex("fix");
    EXPECT_TRUE(syntax.loadCommandSpecsFromString(QStringLiteral("fix special 4 defid,group\n")));
    // the index is stable, the entry is replaced
    EXPECT_EQ(syntax.commandIndex("fix"), before);
    EXPECT_EQ(syntax.commandCategory("fix"), CmdCat::Special);
    EXPECT_EQ(syntax.spec(before)->minArgs, 4);
}

// ---------------------------------------------------------------------------
// shipped command spec table

#ifdef COMMAND_SPECS_TABLE
TEST(LammpsSyntaxTest, ShippedSpecTableLoads)
{
    LammpsSyntax syntax;
    ASSERT_TRUE(syntax.loadCommandSpecs(QStringLiteral(COMMAND_SPECS_TABLE)));

    // structural commands
    const int fixIdx = syntax.commandIndex("fix");
    ASSERT_GE(fixIdx, 0);
    EXPECT_EQ(syntax.spec(fixIdx)->minArgs, 3);
    EXPECT_EQ(syntax.argSpec(fixIdx, 3).cat, StyleCat::Fix);
    const int dumpIdx = syntax.commandIndex("dump");
    ASSERT_GE(dumpIdx, 0);
    EXPECT_EQ(syntax.spec(dumpIdx)->minArgs, 5);
    EXPECT_EQ(syntax.argSpec(dumpIdx, 3).cat, StyleCat::Dump);
    const int regIdx = syntax.commandIndex("region");
    ASSERT_GE(regIdx, 0);
    EXPECT_EQ(syntax.argSpec(regIdx, 2).cat, StyleCat::Region);
    const int varIdx = syntax.commandIndex("variable");
    ASSERT_GE(varIdx, 0);
    EXPECT_EQ(syntax.argSpec(varIdx, 2).cat, StyleCat::Variable);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("pair_style"), 1).cat, StyleCat::Pair);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("units"), 1).cat, StyleCat::Units);

    // hybrid sub-style positions
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("pair_style"), 2).role, ArgRole::SubStyle);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("pair_style"), 7).role, ArgRole::SubStyle);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("pair_coeff"), 3).role, ArgRole::SubStyle);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("pair_coeff"), 3).cat, StyleCat::Pair);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("bond_coeff"), 2).role, ArgRole::SubStyle);
    EXPECT_EQ(syntax.argSpec(syntax.commandIndex("bond_coeff"), 2).cat, StyleCat::Bond);

    // display categories are preserved from the historically grown scheme
    EXPECT_EQ(syntax.commandCategory("lattice"), CmdCat::Lattice);
    EXPECT_EQ(syntax.commandCategory("thermo_style"), CmdCat::Output);
    EXPECT_EQ(syntax.commandCategory("include"), CmdCat::Output);
    EXPECT_EQ(syntax.commandCategory("fix_modify"), CmdCat::Modify);
    EXPECT_EQ(syntax.commandCategory("pair_style"), CmdCat::Particle);
    EXPECT_EQ(syntax.commandCategory("minimize"), CmdCat::Run);
    EXPECT_EQ(syntax.commandCategory("neighbor"), CmdCat::Setup);
    EXPECT_EQ(syntax.commandCategory("unfix"), CmdCat::Special);
    EXPECT_EQ(syntax.commandCategory("no_such_command"), CmdCat::Other);
}
#endif

// ---------------------------------------------------------------------------
// completion targets

namespace {
LammpsSyntax &completionSyntax()
{
    static LammpsSyntax syntax;
    static bool loaded = false;
    if (!loaded) {
        syntax.loadCommandSpecsFromString(
            QStringLiteral("fix particle 3 defid,group,style:fix\n"
                           "pair_style particle 1 style:pair,substyle:pair*\n"
                           "pair_coeff particle 2 keyword,keyword,substyle:pair*\n"
                           "dump particle 5 defid,group,style:dump,int,file\n"
                           "dump_modify modify 2 label,keyword\n"
                           "read_data output 1 file\n"
                           "units lattice 1 style:units\n"));
        loaded = true;
    }
    return syntax;
}
} // namespace

TEST(LammpsSyntaxTest, CompletionTargetPositions)
{
    const auto &syntax = completionSyntax();

    // word 0 of a fresh line: command completion with the word span
    auto target = syntax.completionTarget(0, QStringLiteral("fi"), 2);
    EXPECT_EQ(target.kind, CompleterKind::Command);
    EXPECT_EQ(target.wordStart, 0);
    EXPECT_EQ(target.wordLength, 2);

    // style positions with their category
    target = syntax.completionTarget(0, QStringLiteral("pair_style lj"), 13);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Pair);
    target = syntax.completionTarget(0, QStringLiteral("fix 1 all nv"), 12);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Fix);
    target = syntax.completionTarget(0, QStringLiteral("units me"), 8);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Units);

    // group-ID and file positions
    target = syntax.completionTarget(0, QStringLiteral("fix 1 al"), 8);
    EXPECT_EQ(target.kind, CompleterKind::Group);
    target = syntax.completionTarget(0, QStringLiteral("read_data da"), 12);
    EXPECT_EQ(target.kind, CompleterKind::File);

    // reference prefixes at any later argument position
    target = syntax.completionTarget(0, QStringLiteral("fix 1 all nvt xyz v_ab"), 22);
    EXPECT_EQ(target.kind, CompleterKind::VarName);
    target = syntax.completionTarget(0, QStringLiteral("fix 1 all nvt xyz c_ab"), 22);
    EXPECT_EQ(target.kind, CompleterKind::ComputeId);

    // special cases: pair_coeff * * potential file, read_data extra keywords
    target = syntax.completionTarget(0, QStringLiteral("pair_coeff * * SiC.te"), 21);
    EXPECT_EQ(target.kind, CompleterKind::File);
    target = syntax.completionTarget(0, QStringLiteral("read_data data.lmp ext"), 22);
    EXPECT_EQ(target.kind, CompleterKind::Extra);

    // cursor on whitespace or a comment: nothing to complete
    target = syntax.completionTarget(0, QStringLiteral("fix  1"), 4);
    EXPECT_EQ(target.kind, CompleterKind::None);
    target = syntax.completionTarget(0, QStringLiteral("# fix"), 4);
    EXPECT_EQ(target.kind, CompleterKind::None);
}

TEST(LammpsSyntaxTest, CompletionTargetSubStyles)
{
    const auto &syntax = completionSyntax();

    // sub-style positions of hybrid styles complete from the category list
    auto target = syntax.completionTarget(0, QStringLiteral("pair_style hybrid lj"), 20);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Pair);
    target = syntax.completionTarget(0, QStringLiteral("pair_coeff 1 2 lj"), 17);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Pair);

    // numeric arguments in the same positions offer nothing
    target = syntax.completionTarget(0, QStringLiteral("pair_coeff 1 2 2."), 17);
    EXPECT_EQ(target.kind, CompleterKind::None);
    // variable references keep their reference completion
    target = syntax.completionTarget(0, QStringLiteral("pair_coeff 1 2 v_x"), 18);
    EXPECT_EQ(target.kind, CompleterKind::VarName);
    // the "pair_coeff * * <potential file>" use keeps file completion
    target = syntax.completionTarget(0, QStringLiteral("pair_coeff * * Si"), 17);
    EXPECT_EQ(target.kind, CompleterKind::File);
}

TEST(LammpsSyntaxTest, CompletionTargetDumpImage)
{
    const auto &syntax = completionSyntax();

    // keyword section of a dump image command
    const QString kwline = QStringLiteral("dump 1 all image 100 f.png type type zo");
    auto target          = syntax.completionTarget(0, kwline, kwline.size());
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::ImageKw);

    // color-value positions of dump image keywords
    const QString bondline = QStringLiteral("dump 1 all image 100 f.png type type bond re");
    target                 = syntax.completionTarget(0, bondline, bondline.size());
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Color);

    // color-value positions of dump_modify keywords
    const QString modline = QStringLiteral("dump_modify 1 backcolor wh");
    target                = syntax.completionTarget(0, modline, modline.size());
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Color);
    const QString acolorline = QStringLiteral("dump_modify 1 acolor 1 re");
    target                   = syntax.completionTarget(0, acolorline, acolorline.size());
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Color);

    // no image keyword completion for other dump styles
    const QString atomline = QStringLiteral("dump 1 all atom 100 f.dump xx yy zo");
    target                 = syntax.completionTarget(0, atomline, atomline.size());
    EXPECT_EQ(target.kind, CompleterKind::None);
}

TEST(LammpsSyntaxTest, RegistryImageKeywordsAndColors)
{
    LammpsSyntax syntax;
    // the dump image keywords are seeded by the constructor
    EXPECT_TRUE(syntax.knownStyle(StyleCat::ImageKw, "zoom"));
    EXPECT_TRUE(syntax.knownStyle(StyleCat::ImageKw, "ssao"));
    EXPECT_FALSE(syntax.knownStyle(StyleCat::ImageKw, "banana"));
    // color names are injected
    EXPECT_FALSE(syntax.knownStyle(StyleCat::Color, "red"));
    syntax.setStyles(StyleCat::Color, {"red", "white", "dodgerblue"});
    EXPECT_TRUE(syntax.knownStyle(StyleCat::Color, "red"));
    EXPECT_FALSE(syntax.knownStyle(StyleCat::Color, "vermilion"));
}

TEST(LammpsSyntaxTest, CompletionTargetOnContinuationLines)
{
    const auto &syntax = completionSyntax();

    // the style slot of a fix continues on the second physical line
    const LineTokens first = tokenizeLine(QStringLiteral("fix 1 all &"));
    const int state        = SyntaxState::withCommand(first.outState, syntax.commandIndex("fix"));
    auto target            = syntax.completionTarget(state, QStringLiteral("nv"), 2);
    EXPECT_EQ(target.kind, CompleterKind::Style);
    EXPECT_EQ(target.cat, StyleCat::Fix);

    // without the continuation state the same word is a command position
    target = syntax.completionTarget(0, QStringLiteral("nv"), 2);
    EXPECT_EQ(target.kind, CompleterKind::Command);
}

// ---------------------------------------------------------------------------
// InputScanner

TEST(LammpsSyntaxTest, ScannerBasics)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("units lj\nrun 100\n"));
    ASSERT_EQ(scanner.commands().size(), 2);
    EXPECT_EQ(scanner.commands()[0].words[0].text, QStringLiteral("units"));
    EXPECT_EQ(scanner.commands()[0].words[1].text, QStringLiteral("lj"));
    EXPECT_EQ(scanner.commands()[0].firstLine, 1);
    EXPECT_EQ(scanner.commands()[1].firstLine, 2);
    EXPECT_TRUE(scanner.diagnostics().isEmpty());
}

TEST(LammpsSyntaxTest, ScannerJoinsContinuations)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("fix 1 all nvt &\n  temp 300 300 100\nrun 50\n"));
    ASSERT_EQ(scanner.commands().size(), 2);
    const auto &fix = scanner.commands()[0];
    ASSERT_EQ(fix.words.size(), 8);
    EXPECT_EQ(fix.words[3].text, QStringLiteral("nvt"));
    EXPECT_EQ(fix.words[4].text, QStringLiteral("temp"));
    EXPECT_EQ(fix.words[4].line, 2);
    EXPECT_EQ(fix.firstLine, 1);
    EXPECT_EQ(fix.lastLine, 2);
}

TEST(LammpsSyntaxTest, ScannerJoinsMidWord)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("pair_style lj/&\ncut 2.5\n"));
    ASSERT_EQ(scanner.commands().size(), 1);
    const auto &cmd = scanner.commands()[0];
    ASSERT_EQ(cmd.words.size(), 3);
    EXPECT_EQ(cmd.words[1].text, QStringLiteral("lj/cut"));
    EXPECT_EQ(cmd.words[2].text, QStringLiteral("2.5"));
}

TEST(LammpsSyntaxTest, ScannerStripsQuotes)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("print \"hello world\"\nprint 'single'\n"));
    ASSERT_EQ(scanner.commands().size(), 2);
    EXPECT_EQ(scanner.commands()[0].words[1].text, QStringLiteral("hello world"));
    EXPECT_TRUE(scanner.commands()[0].words[1].quoted);
    EXPECT_EQ(scanner.commands()[1].words[1].text, QStringLiteral("single"));
}

TEST(LammpsSyntaxTest, ScannerJoinsQuotedContinuation)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("variable a string \"red green blue &\n  purple orange cyan\"\n"));
    ASSERT_EQ(scanner.commands().size(), 1);
    const auto &cmd = scanner.commands()[0];
    ASSERT_EQ(cmd.words.size(), 4);
    EXPECT_EQ(cmd.words[3].text, QStringLiteral("red green blue   purple orange cyan"));
    EXPECT_TRUE(cmd.words[3].quoted);
    EXPECT_TRUE(scanner.diagnostics().isEmpty());
}

TEST(LammpsSyntaxTest, ScannerSkipsCommentsAndEmptyLines)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("# header\n\n   \nrun 100 # trailing\n# tail\n"));
    ASSERT_EQ(scanner.commands().size(), 1);
    EXPECT_EQ(scanner.commands()[0].firstLine, 4);
    ASSERT_EQ(scanner.commands()[0].words.size(), 2);
}

TEST(LammpsSyntaxTest, ScannerSubstitutionFlag)
{
    InputScanner scanner;
    scanner.scan(QStringLiteral("run ${steps}\n"));
    ASSERT_EQ(scanner.commands().size(), 1);
    EXPECT_TRUE(scanner.commands()[0].words[1].hasSubst);
    EXPECT_FALSE(scanner.commands()[0].words[0].hasSubst);
}

TEST(LammpsSyntaxTest, ScannerDiagnostics)
{
    {
        InputScanner scanner;
        scanner.scan(QStringLiteral("print \"abc\nrun 100\n"));
        ASSERT_FALSE(scanner.diagnostics().isEmpty());
        EXPECT_EQ(scanner.diagnostics()[0].what, InputScanner::Diag::UnbalancedQuote);
        EXPECT_EQ(scanner.diagnostics()[0].line, 1);
        // the scanner recovers: the next command is still seen
        EXPECT_EQ(scanner.commands().size(), 2);
    }
    {
        InputScanner scanner;
        scanner.scan(QStringLiteral("print \"\"\"unclosed\n"));
        ASSERT_FALSE(scanner.diagnostics().isEmpty());
        EXPECT_EQ(scanner.diagnostics()[0].what, InputScanner::Diag::UnclosedTriple);
    }
    {
        InputScanner scanner;
        scanner.scan(QStringLiteral("run 100 &"));
        ASSERT_FALSE(scanner.diagnostics().isEmpty());
        EXPECT_EQ(scanner.diagnostics()[0].what, InputScanner::Diag::DanglingContinuation);
        EXPECT_EQ(scanner.diagnostics()[0].line, 1);
        // the partial command is still flushed
        ASSERT_EQ(scanner.commands().size(), 1);
        EXPECT_EQ(scanner.commands()[0].words[0].text, QStringLiteral("run"));
    }
}

// Local Variables:
// c-basic-offset: 4
// End:
