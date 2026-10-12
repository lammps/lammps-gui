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
#include "lammpssyntax.h"

#include <gtest/gtest.h>

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

// Fixture providing the offscreen QApplication needed for widget-free
// QSyntaxHighlighter testing on a plain QTextDocument.
class HighlighterTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc     = 1;
            static char *argv[] = {(char *)"test_highlighter"};
            app                 = new QApplication(argc, argv);
        }
    }

    // seed a small synthetic registry: commands, specs, and a few styles
    static void seed(LammpsSyntax &syntax)
    {
        syntax.loadCommandSpecsFromString(
            QStringLiteral("fix particle 3 defid,group,style:fix\n"
                           "pair_style particle 1 style:pair,substyle:pair*\n"
                           "pair_coeff particle 2 keyword,keyword,substyle:pair*\n"
                           "dump particle 5 defid,group,style:dump,int,file\n"
                           "dump_modify modify 2 label,keyword\n"
                           "write_dump output 3 group,style:dump,file\n"
                           // the keyword role at position 3 is not in the
                           // shipped table; it makes the argument renumbering
                           // of the "rerun ... dump ..." section observable
                           "read_dump output 2 file,int,keyword\n"
                           "rerun run 1 file\n"
                           "units lattice 1 style:units\n"
                           "run run 1 int\n"
                           "unfix special 1 label\n"
                           "print output 1 any\n"));
        syntax.setCommands({"fix", "pair_style", "pair_coeff", "dump", "dump_modify", "write_dump",
                            "read_dump", "rerun", "units", "run", "unfix", "print", "clear"});
        syntax.setStyles(StyleCat::Fix, {"nvt", "nve"});
        syntax.setStyles(StyleCat::Pair, {"lj/cut", "zero", "hybrid"});
        syntax.setStyles(StyleCat::Dump, {"atom", "image", "movie"});
        syntax.setStyles(StyleCat::Color, {"red", "white", "dodgerblue"});
    }

    // final character format at a column of a block
    static QTextCharFormat formatAt(const QTextDocument &doc, int blockNum, int col)
    {
        const auto block = doc.findBlockByNumber(blockNum);
        if (!block.isValid() || !block.layout()) return {};
        const auto ranges = block.layout()->formats();
        for (const auto &range : ranges) {
            if ((col >= range.start) && (col < range.start + range.length)) return range.format;
        }
        return {};
    }

    static bool hasWave(const QTextCharFormat &fmt)
    {
        return fmt.underlineStyle() == QTextCharFormat::WaveUnderline;
    }

    static QApplication *app;
};

QApplication *HighlighterTest::app = nullptr;

TEST_F(HighlighterTest, CommandCategoryColors)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("fix 1 all nvt\nunits lj\nrun 100\nunfix 1"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    const auto fixFmt   = formatAt(doc, 0, 0);
    const auto unitsFmt = formatAt(doc, 1, 0);
    const auto runFmt   = formatAt(doc, 2, 0);
    const auto unfixFmt = formatAt(doc, 3, 0);
    // command words are bold and category colors are distinct
    EXPECT_EQ(fixFmt.fontWeight(), QFont::Bold);
    EXPECT_NE(fixFmt.foreground(), unitsFmt.foreground());
    EXPECT_NE(unitsFmt.foreground(), runFmt.foreground());
    EXPECT_NE(runFmt.foreground(), unfixFmt.foreground());
    // known commands carry no unknown marker
    EXPECT_FALSE(hasWave(fixFmt));
    EXPECT_FALSE(hasWave(runFmt));
}

TEST_F(HighlighterTest, ArgumentRoleColors)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("fix 1 all nvt\nrun 100\nprint 'hello'"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // the fix ID (DefineId role) shares the number color
    EXPECT_EQ(formatAt(doc, 0, 4).foreground(), formatAt(doc, 1, 4).foreground());
    // the group ID (GroupId role) shares the string color
    EXPECT_EQ(formatAt(doc, 0, 6).foreground(), formatAt(doc, 2, 6).foreground());
    // the fix style name (Style role) shares the run-command color
    EXPECT_EQ(formatAt(doc, 0, 10).foreground(), formatAt(doc, 1, 0).foreground());
    EXPECT_FALSE(hasWave(formatAt(doc, 0, 10)));
}

TEST_F(HighlighterTest, ContinuationLineRoleColors)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    // the same style name once on a single line, once on a continuation line
    doc.setPlainText(QStringLiteral("pair_style lj/cut\npair_style &\nlj/cut 2.5\nrun 100"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    const auto direct = formatAt(doc, 0, 11);
    const auto contd  = formatAt(doc, 2, 0);
    EXPECT_EQ(direct.foreground(), contd.foreground());
    EXPECT_FALSE(hasWave(contd));
    // the trailing number on the continuation line is colored as a number
    EXPECT_EQ(formatAt(doc, 2, 7).foreground(), formatAt(doc, 3, 4).foreground());
}

TEST_F(HighlighterTest, MidwordFragmentsNotMarkedUnknown)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("pair_style lj/&\ncut 2.5"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // neither fragment of the split style name is flagged unknown
    EXPECT_FALSE(hasWave(formatAt(doc, 0, 11)));
    EXPECT_FALSE(hasWave(formatAt(doc, 1, 0)));
    // the '&' is marked in the special format (bold)
    EXPECT_EQ(formatAt(doc, 0, 14).fontWeight(), QFont::Bold);
}

TEST_F(HighlighterTest, UnknownNamesGetWaveUnderline)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("fix 1 all nvtx\nbogus 1 2\nfix 2 all ${sty}"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // unknown fix style and unknown command are marked
    EXPECT_TRUE(hasWave(formatAt(doc, 0, 10)));
    EXPECT_TRUE(hasWave(formatAt(doc, 1, 0)));
    // a style position containing a $-substitution is never marked
    EXPECT_FALSE(hasWave(formatAt(doc, 2, 10)));
}

TEST_F(HighlighterTest, CursorSuppressesUnknownMarker)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("fix 1 all nvtx"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();
    ASSERT_TRUE(hasWave(formatAt(doc, 0, 10)));

    // cursor inside (or right at the end of) the word: marker disappears
    hl.setCursorPos(0, 12);
    EXPECT_FALSE(hasWave(formatAt(doc, 0, 10)));
    hl.setCursorPos(0, 14);
    EXPECT_FALSE(hasWave(formatAt(doc, 0, 10)));
    // cursor elsewhere: marker returns
    hl.setCursorPos(0, 2);
    EXPECT_TRUE(hasWave(formatAt(doc, 0, 10)));
}

TEST_F(HighlighterTest, NoRegistryNoUnknownMarking)
{
    LammpsSyntax syntax; // spec table only, no command/style names
    syntax.loadCommandSpecsFromString(QStringLiteral("fix particle 3 defid,group,style:fix\n"));
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("fix 1 all bogus\nxyzzy 1 2"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // colors still apply (bold category format on the command word) ...
    EXPECT_EQ(formatAt(doc, 0, 0).fontWeight(), QFont::Bold);
    // ... but nothing is flagged unknown
    for (int block = 0; block < 2; ++block)
        for (int col = 0; col < 15; ++col)
            EXPECT_FALSE(hasWave(formatAt(doc, block, col))) << block << ":" << col;
}

TEST_F(HighlighterTest, CommentContinuation)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("# note with continuation &\nstill a comment\nrun 100"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // the continued line is formatted like the comment line
    EXPECT_EQ(formatAt(doc, 1, 0), formatAt(doc, 0, 0));
    // the line after the comment ends is a command again
    EXPECT_NE(formatAt(doc, 2, 0), formatAt(doc, 0, 0));
}

TEST_F(HighlighterTest, TripleQuoteBlockAndEditResync)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("print \"\"\"block\ninside text\nend\"\"\"\nrun 100"));
    // a document without a layout does not emit contentsChange for edits, so
    // force layout creation to exercise the edit-triggered rehighlight cascade
    (void)doc.documentLayout();
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // inside the block: string format; after the block: command format
    const auto insideFmt = formatAt(doc, 1, 0);
    EXPECT_EQ(insideFmt, formatAt(doc, 0, 6)); // same as the opening """ span
    const auto runFmt = formatAt(doc, 3, 0);
    EXPECT_NE(runFmt, insideFmt);

    // remove the opening triple quote: the following blocks must re-resolve
    // (this desynchronized with the previous member-variable implementation)
    QTextCursor cursor(&doc);
    cursor.setPosition(6);
    for (int i = 0; i < 3; ++i)
        cursor.deleteChar();

    // line 1 is now ordinary words, not a string
    EXPECT_NE(formatAt(doc, 1, 0), insideFmt);
    // line 2 now opens a block ("end""" ends with an unclosed triple quote),
    // so line 3 has become string content
    EXPECT_EQ(formatAt(doc, 3, 0), insideFmt);
}

TEST_F(HighlighterTest, HybridSubStyles)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("pair_style hybrid lj/cut 2.5 zero 3.0\n"
                                    "pair_coeff 1 2 lj/cut 1.0 1.0\n"
                                    "pair_style hybrid nosuch 1.0\n"
                                    "run 100"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // the two sub-styles share one format that differs from the parent style
    // format and from the number format
    const auto subFmt = formatAt(doc, 0, 18);                          // lj/cut
    EXPECT_EQ(formatAt(doc, 0, 29).foreground(), subFmt.foreground()); // zero
    EXPECT_NE(formatAt(doc, 0, 11).foreground(), subFmt.foreground()); // hybrid (Style role)
    EXPECT_NE(formatAt(doc, 0, 25).foreground(), subFmt.foreground()); // 2.5 (number)
    // the sub-style of a pair_coeff command uses the same format
    EXPECT_EQ(formatAt(doc, 1, 15).foreground(), subFmt.foreground());
    // words that are not a known style are arguments: no sub-style color and,
    // unlike style positions, no unknown marker either
    EXPECT_NE(formatAt(doc, 2, 18).foreground(), subFmt.foreground());
    EXPECT_FALSE(hasWave(formatAt(doc, 2, 18)));
}

TEST_F(HighlighterTest, DumpImageColorsAndKeywords)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(
        QStringLiteral("dump 2 all image 100 i.png type type zoom 1.6 box yes 0.02\n"
                       "dump_modify 2 backcolor white acolor 1 red\n"
                       "dump 3 all atom 100 f.dump\n"
                       "dump 4 all image 100 j.png type type depthcue yes 0.5 auto auto "
                       "outline yes 2 dodgerblue defocus yes 0.4 auto"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // dump image keywords are marked (same color as strings)
    QTextDocument doc2;
    doc2.setPlainText(QStringLiteral("print 'x'"));
    Highlighter hl2(&syntax, &doc2);
    hl2.rehighlight();
    EXPECT_EQ(formatAt(doc, 0, 37).foreground(), formatAt(doc2, 0, 6).foreground()); // zoom
    EXPECT_EQ(formatAt(doc, 0, 46).foreground(), formatAt(doc2, 0, 6).foreground()); // box
    EXPECT_EQ(formatAt(doc, 3, 38).foreground(), formatAt(doc2, 0, 6).foreground()); // depthcue
    EXPECT_EQ(formatAt(doc, 3, 65).foreground(), formatAt(doc2, 0, 6).foreground()); // outline
    EXPECT_EQ(formatAt(doc, 3, 90).foreground(), formatAt(doc2, 0, 6).foreground()); // defocus
    // the outline color value renders in its own color
    EXPECT_EQ(formatAt(doc, 3, 78).foreground().color(), QColor(QStringLiteral("dodgerblue")));

    // color names render in their own color; low-contrast ones get a chip
    const auto whiteFmt = formatAt(doc, 1, 24);
    EXPECT_EQ(whiteFmt.foreground().color(), QColor(QStringLiteral("white")));
    EXPECT_NE(whiteFmt.background(), QBrush()); // chip background set on a light theme
    const auto redFmt = formatAt(doc, 1, 39);
    EXPECT_EQ(redFmt.foreground().color(), QColor(QStringLiteral("red")));
}

TEST_F(HighlighterTest, EmbeddedCommandSections)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(
        QStringLiteral("write_dump all image out.png type type modify backcolor white\n"
                       "dump_modify 2 backcolor white\n"
                       "rerun dump.file first 100 dump x y z\n"
                       "read_dump dump.file 100 x y z\n"
                       "write_dump all image out.png type type modify &\n"
                       "backcolor white\n"
                       "print modify"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // the "modify" keyword of write_dump is colored like the dump_modify
    // command word and thus differs from the write_dump command word
    const auto modifyFmt = formatAt(doc, 0, 39);
    EXPECT_EQ(modifyFmt.foreground(), formatAt(doc, 1, 0).foreground());
    EXPECT_EQ(modifyFmt.fontWeight(), QFont::Bold);
    EXPECT_NE(modifyFmt.foreground(), formatAt(doc, 0, 0).foreground());

    // the arguments after it are dump_modify arguments: the first one is the
    // keyword position of dump_modify (its dump ID is implicit here) and
    // color names render in their own color as on a dump_modify line
    EXPECT_EQ(formatAt(doc, 0, 46), formatAt(doc, 1, 14)); // backcolor
    EXPECT_EQ(formatAt(doc, 0, 56), formatAt(doc, 1, 24)); // white
    // ... and the section continues across a '&' line continuation
    EXPECT_EQ(formatAt(doc, 5, 0), formatAt(doc, 1, 14));  // backcolor
    EXPECT_EQ(formatAt(doc, 5, 10), formatAt(doc, 1, 24)); // white

    // the "dump" keyword of rerun is colored like the read_dump command word
    const auto dumpFmt = formatAt(doc, 2, 26);
    EXPECT_EQ(dumpFmt.foreground(), formatAt(doc, 3, 0).foreground());
    EXPECT_NE(dumpFmt.foreground(), formatAt(doc, 2, 0).foreground());

    // the arguments after it are read_dump arguments starting at the first
    // field, since the file name and time step of read_dump are implicit here
    EXPECT_EQ(formatAt(doc, 2, 31), formatAt(doc, 3, 24)); // x
    EXPECT_NE(formatAt(doc, 2, 33), formatAt(doc, 3, 24)); // y (no role)

    // the keywords are only special in their own commands
    EXPECT_NE(formatAt(doc, 6, 6).foreground(), modifyFmt.foreground());
}

TEST_F(HighlighterTest, VarRefOverlayInsideStrings)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("print \"T = $t\"\nrun $x\n# not $here"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    const auto strFmt = formatAt(doc, 0, 7);  // inside the string
    const auto refFmt = formatAt(doc, 0, 11); // the $t reference
    EXPECT_NE(refFmt, strFmt);
    // the same reference format is used outside of strings
    EXPECT_EQ(refFmt.foreground(), formatAt(doc, 1, 4).foreground());
    // no reference formatting inside comments
    EXPECT_EQ(formatAt(doc, 2, 6), formatAt(doc, 2, 0));
}

TEST_F(HighlighterTest, ReferencesAndSpecialWords)
{
    LammpsSyntax syntax;
    seed(syntax);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("print c_thermo_temp\nfix 1 all nvt\nrun 100\n"
                                    "print INF"));
    Highlighter hl(&syntax, &doc);
    hl.rehighlight();

    // c_... references at any argument position get the variable format;
    // compare against the $-reference format
    QTextDocument doc2;
    doc2.setPlainText(QStringLiteral("run $x"));
    Highlighter hl2(&syntax, &doc2);
    hl2.rehighlight();
    EXPECT_EQ(formatAt(doc, 0, 6).foreground(), formatAt(doc2, 0, 4).foreground());

    // special keywords are bold
    EXPECT_EQ(formatAt(doc, 3, 6).fontWeight(), QFont::Bold);
}

// Local Variables:
// c-basic-offset: 4
// End:
