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

#include <QFile>
#include <QIODevice>
#include <QRegularExpression>

// The tokenizer mirrors the parsing rules of the LAMMPS Input class, see
// lammps/src/input.cpp: Input::file() (trailing whitespace is trimmed
// *before* the trailing '&' check; the '&' is overwritten by the next
// physical line, so joins can happen mid-word and before comment stripping),
// Input::parse() ('#' outside of quotes starts a comment; text in single,
// double, or triple quotes is scanned contextually), and Input::substitute()
// ('\$' escapes, '${name}', and balanced '$(expr)' forms).  One documented
// deviation: a trailing '&' inside an open triple-quoted block is treated as
// string content, following the Commands_parse.rst rule that '&' does not
// function as a line continuation there.

namespace {

// scan for the closing quote of a single- or double-quoted section honoring
// backslash-escaped quotes; returns the index of the closing quote or -1
int findClosingQuote(const QString &text, int from, int end, QChar quote)
{
    int i = from;
    while (i < end) {
        const QChar c = text.at(i);
        if ((c == QLatin1Char('\\')) && (i + 1 < end) && (text.at(i + 1) == quote)) {
            i += 2;
            continue;
        }
        if (c == quote) return i;
        ++i;
    }
    return -1;
}

constexpr QChar C_AMP    = QLatin1Char('&');
constexpr QChar C_HASH   = QLatin1Char('#');
constexpr QChar C_SQUOTE = QLatin1Char('\'');
constexpr QChar C_DQUOTE = QLatin1Char('"');
constexpr QChar C_BSLASH = QLatin1Char('\\');
constexpr QChar C_DOLLAR = QLatin1Char('$');

// accelerator suffixes filtered from the completion lists
const QStringList &acceleratorSuffixes()
{
    static const QStringList suffixes = {QStringLiteral("/gpu"),     QStringLiteral("/intel"),
                                         QStringLiteral("/kk"),      QStringLiteral("/kk/device"),
                                         QStringLiteral("/kk/host"), QStringLiteral("/omp"),
                                         QStringLiteral("/opt")};
    return suffixes;
}

} // namespace

LineTokens tokenizeLine(const QString &text, int prevState)
{
    LineTokens result;
    const int state   = qMax(prevState, 0);
    const int inFlags = SyntaxState::flags(state);
    const int prevCmd = SyntaxState::cmdIndex(state);
    int argsUsed      = SyntaxState::argsUsed(state);
    const int len     = text.size();

    const bool contIn    = inFlags & SyntaxState::CONTINUE;
    const bool midIn     = inFlags & SyntaxState::MIDWORD;
    const bool commentIn = inFlags & SyntaxState::COMMENT;
    const bool inTriple  = inFlags & SyntaxState::TRIPLE;
    bool inSingle        = inFlags & SyntaxState::SINGLEQ;
    bool inDouble        = inFlags & SyntaxState::DOUBLEQ;

    // true while this physical line continues the logical line of the previous one
    const bool logicalContinues = SyntaxState::logicalContinues(state);

    // trailing whitespace is trimmed before checking for the '&' continuation
    int lastPrintable = len - 1;
    while ((lastPrintable >= 0) && text.at(lastPrintable).isSpace())
        --lastPrintable;

    // empty or whitespace-only line: terminates a pending continuation or open
    // quote, but an open triple-quoted block continues across it
    if (lastPrintable < 0) {
        if (inTriple) result.outState = SyntaxState::pack(SyntaxState::TRIPLE, prevCmd, argsUsed);
        return result;
    }

    bool continuation = (text.at(lastPrintable) == C_AMP);
    const int contPos = continuation ? lastPrintable : -1;
    const int scanEnd = continuation ? contPos : len;

    const auto addToken = [&result](int start, int length, TokType type, int argIndex) {
        if (length > 0) result.tokens.append({start, length, type, argIndex, false, false, false});
    };

    // continuation of a '... &' comment: the whole line stays inside the comment
    if (commentIn) {
        addToken(0, scanEnd, TokType::Comment, -1);
        if (continuation) {
            addToken(contPos, 1, TokType::Continuation, -1);
            result.outState =
                SyntaxState::pack(SyntaxState::COMMENT | SyntaxState::CONTINUE, prevCmd, argsUsed);
        }
        return result;
    }

    int i = 0;
    // argument index for a word starting exactly at position i (continues the
    // previous argument after a mid-word join or a closed quoted section)
    int pendingArg       = -1;
    bool pendingFragment = false;

    // resume an open triple-quoted block: content belongs to the current argument
    if (inTriple) {
        const int close = text.indexOf(QStringLiteral("\"\"\""), 0);
        if ((close < 0) || (close + 3 > scanEnd)) {
            // block stays open; a trailing '&' is string content here
            addToken(0, len, TokType::TripleString, argsUsed);
            result.outState = SyntaxState::pack(SyntaxState::TRIPLE, prevCmd, argsUsed);
            return result;
        }
        addToken(0, close + 3, TokType::TripleString, argsUsed);
        i          = close + 3;
        pendingArg = argsUsed;
    } else if (inSingle || inDouble) {
        // resume a quoted string that spans a '&' continuation
        const QChar quote = inSingle ? C_SQUOTE : C_DQUOTE;
        const int close   = findClosingQuote(text, 0, scanEnd, quote);
        if (close < 0) {
            addToken(0, scanEnd, TokType::String, argsUsed);
            if (continuation) {
                addToken(contPos, 1, TokType::Continuation, -1);
                const int flag = inSingle ? SyntaxState::SINGLEQ : SyntaxState::DOUBLEQ;
                result.outState =
                    SyntaxState::pack(flag | SyntaxState::CONTINUE, prevCmd, argsUsed);
            } else {
                result.unbalancedQuote = true;
            }
            return result;
        }
        addToken(0, close + 1, TokType::String, argsUsed);
        inSingle = inDouble = false;
        i                   = close + 1;
        pendingArg          = argsUsed;
    } else if (contIn && midIn && !text.at(0).isSpace()) {
        // mid-word join: the first word continues the previous argument
        pendingArg      = argsUsed;
        pendingFragment = true;
    }

    // start counting arguments at 0 on a fresh logical line
    if (!logicalContinues) argsUsed = -1;

    enum class Exit : quint8 { Normal, CommentHit, TripleOpen };
    Exit exit        = Exit::Normal;
    int lastWordFrom = -1; // first token index of the most recent word

    while (i < scanEnd) {
        if (text.at(i).isSpace()) {
            ++i;
            pendingArg      = -1;
            pendingFragment = false;
            continue;
        }
        if (text.at(i) == C_HASH) {
            addToken(i, scanEnd - i, TokType::Comment, -1);
            exit = Exit::CommentHit;
            break;
        }

        // start of a word; it may contain embedded quoted sections
        const int wordArg   = (pendingArg >= 0) ? pendingArg : argsUsed + 1;
        argsUsed            = qMax(argsUsed, wordArg);
        const int wordStart = i;
        const int firstTok  = result.tokens.size();
        bool sawQuote       = false;
        int subStart        = i;
        lastWordFrom        = firstTok;

        while (i < scanEnd) {
            const QChar c = text.at(i);
            if (c.isSpace()) break;
            if ((c == C_BSLASH) && (i + 1 < scanEnd) &&
                ((text.at(i + 1) == C_SQUOTE) || (text.at(i + 1) == C_DQUOTE))) {
                i += 2;
                continue;
            }
            if ((c == C_DQUOTE) && (i + 2 < scanEnd) && (text.at(i + 1) == C_DQUOTE) &&
                (text.at(i + 2) == C_DQUOTE) &&
                !((i + 3 < scanEnd) && (text.at(i + 3) == C_DQUOTE))) {
                addToken(subStart, i - subStart, TokType::Word, wordArg);
                sawQuote        = true;
                const int close = text.indexOf(QStringLiteral("\"\"\""), i + 3);
                if ((close < 0) || (close + 3 > scanEnd)) {
                    // multi-line triple-quoted block opens here; the rest of the
                    // physical line (including any trailing '&') is string content
                    addToken(i, len - i, TokType::TripleString, wordArg);
                    exit = Exit::TripleOpen;
                    i    = len;
                    break;
                }
                addToken(i, close + 3 - i, TokType::TripleString, wordArg);
                i        = close + 3;
                subStart = i;
                continue;
            }
            if ((c == C_SQUOTE) || (c == C_DQUOTE)) {
                addToken(subStart, i - subStart, TokType::Word, wordArg);
                sawQuote        = true;
                const int close = findClosingQuote(text, i + 1, scanEnd, c);
                if (close < 0) {
                    addToken(i, scanEnd - i, TokType::String, wordArg);
                    if (c == C_SQUOTE)
                        inSingle = true;
                    else
                        inDouble = true;
                    i        = scanEnd;
                    subStart = scanEnd;
                    break;
                }
                addToken(i, close + 1 - i, TokType::String, wordArg);
                i        = close + 1;
                subStart = i;
                continue;
            }
            ++i;
        }
        if (exit == Exit::TripleOpen) break;
        addToken(subStart, i - subStart, TokType::Word, wordArg);

        // annotate the word's tokens with lexical word-level information
        const QString word = text.mid(wordStart, i - wordStart);
        const bool subst   = word.contains(C_DOLLAR);
        const bool number  = !sawQuote && isNumberWord(word);
        for (int t = firstTok; t < result.tokens.size(); ++t) {
            result.tokens[t].hasSubst = subst;
            result.tokens[t].isNumber = number;
            result.tokens[t].fragment = pendingFragment;
        }
        pendingArg      = -1;
        pendingFragment = false;
    }

    // a mid-word join continues if the character right before the '&' is not whitespace
    const bool midOut =
        continuation && (contPos > 0) && !text.at(contPos - 1).isSpace() && (exit == Exit::Normal);
    if (midOut && (lastWordFrom >= 0) && !inSingle && !inDouble) {
        for (int t = lastWordFrom; t < result.tokens.size(); ++t)
            result.tokens[t].fragment = true;
    }

    if (exit == Exit::TripleOpen) {
        result.outState = SyntaxState::pack(SyntaxState::TRIPLE, logicalContinues ? prevCmd : -1,
                                            qMax(argsUsed, 0));
        return result;
    }

    int outFlags = 0;
    if (continuation) {
        addToken(contPos, 1, TokType::Continuation, -1);
        outFlags |= SyntaxState::CONTINUE;
        if (exit == Exit::CommentHit) outFlags |= SyntaxState::COMMENT;
        if (midOut && !inSingle && !inDouble) outFlags |= SyntaxState::MIDWORD;
        if (inSingle) outFlags |= SyntaxState::SINGLEQ;
        if (inDouble) outFlags |= SyntaxState::DOUBLEQ;
    } else if (inSingle || inDouble) {
        // open quote at the end of a completed logical line: LAMMPS rejects
        // this input; re-lex the next line from a clean state
        result.unbalancedQuote = true;
    }

    if (outFlags)
        result.outState =
            SyntaxState::pack(outFlags, logicalContinues ? prevCmd : -1, qMax(argsUsed, 0));
    return result;
}

QHash<int, QString> argumentTexts(const LineTokens &lt, const QString &line)
{
    QHash<int, QString> argText;
    for (const auto &tok : lt.tokens) {
        if ((tok.argIndex >= 0) && (tok.type != TokType::Comment) &&
            (tok.type != TokType::Continuation))
            argText[tok.argIndex] += line.mid(tok.start, tok.length);
    }
    return argText;
}

QVector<Token> findVarRefs(const QString &text)
{
    QVector<Token> refs;
    const int to = text.size();
    int i        = 0;
    while (i < to) {
        if (text.at(i) != C_DOLLAR) {
            ++i;
            continue;
        }
        // a '$' preceded by a backslash is escaped and not substituted
        if ((i > 0) && (text.at(i - 1) == C_BSLASH)) {
            ++i;
            continue;
        }
        int end = i + 1;
        if ((i + 1 < to) && (text.at(i + 1) == QLatin1Char('{'))) {
            const int close = text.indexOf(QLatin1Char('}'), i + 2);
            end             = ((close < 0) || (close >= to)) ? to : close + 1;
        } else if ((i + 1 < to) && (text.at(i + 1) == QLatin1Char('('))) {
            int depth = 0;
            int j     = i + 2;
            for (; j < to; ++j) {
                const QChar c = text.at(j);
                if (c == QLatin1Char('(')) ++depth;
                if (c == QLatin1Char(')')) {
                    if (depth == 0) break;
                    --depth;
                }
            }
            end = (j < to) ? j + 1 : to;
        } else if (i + 1 < to) {
            end = i + 2;
        }
        refs.append({i, end - i, TokType::VarRef, -1, false, false, false});
        i = end;
    }
    return refs;
}

bool isNumberWord(const QString &word)
{
    const int n = word.size();
    int i       = 0;
    if (n == 0) return false;
    if ((word.at(0) == QLatin1Char('+')) || (word.at(0) == QLatin1Char('-'))) i = 1;
    if (i >= n) return false;

    // integer, integer range, or type wildcard: digits, ':', and '*'
    bool intform  = true;
    bool hasDigit = false;
    bool hasStar  = false;
    for (int j = i; j < n; ++j) {
        const QChar c = word.at(j);
        if (c.isDigit())
            hasDigit = true;
        else if (c == QLatin1Char('*'))
            hasStar = true;
        else if (c != QLatin1Char(':')) {
            intform = false;
            break;
        }
    }
    if (intform && (hasDigit || hasStar)) return true;

    // floating point number with optional e/E/d/D exponent
    int j    = i;
    hasDigit = false;
    while ((j < n) && word.at(j).isDigit()) {
        ++j;
        hasDigit = true;
    }
    if ((j < n) && (word.at(j) == QLatin1Char('.'))) {
        ++j;
        while ((j < n) && word.at(j).isDigit()) {
            ++j;
            hasDigit = true;
        }
    }
    if (!hasDigit) return false;
    if (j < n) {
        const QChar e = word.at(j);
        if ((e != QLatin1Char('e')) && (e != QLatin1Char('E')) && (e != QLatin1Char('d')) &&
            (e != QLatin1Char('D')))
            return false;
        ++j;
        if ((j < n) && ((word.at(j) == QLatin1Char('+')) || (word.at(j) == QLatin1Char('-')))) ++j;
        if (j >= n) return false;
        while ((j < n) && word.at(j).isDigit())
            ++j;
    }
    return j == n;
}

// ----------------------------------------------------------------------------

LammpsSyntax::LammpsSyntax()
{
    // static keyword sets that LAMMPS does not expose through introspection
    setStyles(StyleCat::Variable,
              {QStringLiteral("delete"), QStringLiteral("atomfile"), QStringLiteral("file"),
               QStringLiteral("format"), QStringLiteral("getenv"), QStringLiteral("index"),
               QStringLiteral("internal"), QStringLiteral("loop"), QStringLiteral("python"),
               QStringLiteral("string"), QStringLiteral("timer"), QStringLiteral("uloop"),
               QStringLiteral("universe"), QStringLiteral("world"), QStringLiteral("equal"),
               QStringLiteral("vector"), QStringLiteral("atom")});
    setStyles(StyleCat::Units,
              {QStringLiteral("lj"), QStringLiteral("real"), QStringLiteral("metal"),
               QStringLiteral("si"), QStringLiteral("cgs"), QStringLiteral("electron"),
               QStringLiteral("micro"), QStringLiteral("nano")});
    setStyles(StyleCat::Extra,
              {QStringLiteral("extra/atom/types"), QStringLiteral("extra/bond/types"),
               QStringLiteral("extra/angle/types"), QStringLiteral("extra/dihedral/types"),
               QStringLiteral("extra/improper/types"), QStringLiteral("extra/bond/per/atom"),
               QStringLiteral("extra/angle/per/atom"), QStringLiteral("extra/dihedral/per/atom"),
               QStringLiteral("extra/improper/per/atom"),
               QStringLiteral("extra/special/per/atom")});
    setStyles(StyleCat::ImageKw,
              {QStringLiteral("atom"),    QStringLiteral("adiam"),     QStringLiteral("autobond"),
               QStringLiteral("bond"),    QStringLiteral("grid"),      QStringLiteral("line"),
               QStringLiteral("tri"),     QStringLiteral("ellipsoid"), QStringLiteral("body"),
               QStringLiteral("compute"), QStringLiteral("fix"),       QStringLiteral("size"),
               QStringLiteral("view"),    QStringLiteral("center"),    QStringLiteral("up"),
               QStringLiteral("zoom"),    QStringLiteral("box"),       QStringLiteral("axes"),
               QStringLiteral("region"),  QStringLiteral("subbox"),    QStringLiteral("shiny"),
               QStringLiteral("fsaa"),    QStringLiteral("ssao"),      QStringLiteral("depthcue"),
               QStringLiteral("defocus"), QStringLiteral("outline")});
    specialWords = {QStringLiteral("INF"),  QStringLiteral("EDGE"), QStringLiteral("NULL"),
                    QStringLiteral("SELF"), QStringLiteral("if"),   QStringLiteral("then"),
                    QStringLiteral("else"), QStringLiteral("elif")};
}

void LammpsSyntax::setStyles(StyleCat cat, const QStringList &names)
{
    const int idx = static_cast<int>(cat);
    if ((idx < 0) || (idx >= NUM_STYLE_CATS)) return;

    auto &fullset = styles[idx];
    fullset.clear();
    QStringList filtered;
    for (const auto &name : names) {
        if (name.isEmpty()) continue;
        fullset.insert(name);
        bool suffixed = false;
        for (const auto &suffix : acceleratorSuffixes()) {
            if (name.endsWith(suffix)) {
                suffixed = true;
                break;
            }
        }
        if (!suffixed) filtered << name;
    }
    filtered.sort();
    filtered.removeDuplicates();
    completions[idx] = filtered;
}

bool LammpsSyntax::loadCommandSpecs(const QString &path)
{
    QFile table(path);
    if (!table.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    return loadCommandSpecsFromString(QString::fromUtf8(table.readAll()));
}

bool LammpsSyntax::loadCommandSpecsFromString(const QString &text)
{
    static const QHash<QString, CmdCat> catmap   = {{QStringLiteral("lattice"), CmdCat::Lattice},
                                                    {QStringLiteral("output"), CmdCat::Output},
                                                    {QStringLiteral("modify"), CmdCat::Modify},
                                                    {QStringLiteral("particle"), CmdCat::Particle},
                                                    {QStringLiteral("run"), CmdCat::Run},
                                                    {QStringLiteral("setup"), CmdCat::Setup},
                                                    {QStringLiteral("special"), CmdCat::Special},
                                                    {QStringLiteral("other"), CmdCat::Other}};
    static const QHash<QString, ArgRole> rolemap = {
        {QStringLiteral("any"), ArgRole::Any},         {QStringLiteral("defid"), ArgRole::DefineId},
        {QStringLiteral("group"), ArgRole::GroupId},   {QStringLiteral("file"), ArgRole::File},
        {QStringLiteral("int"), ArgRole::Int},         {QStringLiteral("number"), ArgRole::Number},
        {QStringLiteral("keyword"), ArgRole::Keyword}, {QStringLiteral("label"), ArgRole::Label}};
    static const QHash<QString, StyleCat> stylemap = {
        {QStringLiteral("command"), StyleCat::Command},
        {QStringLiteral("fix"), StyleCat::Fix},
        {QStringLiteral("compute"), StyleCat::Compute},
        {QStringLiteral("dump"), StyleCat::Dump},
        {QStringLiteral("atom"), StyleCat::Atom},
        {QStringLiteral("pair"), StyleCat::Pair},
        {QStringLiteral("bond"), StyleCat::Bond},
        {QStringLiteral("angle"), StyleCat::Angle},
        {QStringLiteral("dihedral"), StyleCat::Dihedral},
        {QStringLiteral("improper"), StyleCat::Improper},
        {QStringLiteral("kspace"), StyleCat::Kspace},
        {QStringLiteral("region"), StyleCat::Region},
        {QStringLiteral("integrate"), StyleCat::Integrate},
        {QStringLiteral("minimize"), StyleCat::Minimize},
        {QStringLiteral("variable"), StyleCat::Variable},
        {QStringLiteral("units"), StyleCat::Units},
        {QStringLiteral("extra"), StyleCat::Extra},
        {QStringLiteral("color"), StyleCat::Color},
        {QStringLiteral("imagekw"), StyleCat::ImageKw}};
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));

    bool ok          = true;
    const auto lines = text.split(QLatin1Char('\n'));
    for (const auto &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList fields = line.split(whitespace, Qt::SkipEmptyParts);
        if ((fields.size() < 3) || (fields.size() > 4)) {
            ok = false;
            continue;
        }
        CommandSpec cs;
        cs.name = fields[0];
        if (!catmap.contains(fields[1])) {
            ok = false;
            continue;
        }
        cs.category  = catmap.value(fields[1]);
        bool numeric = false;
        cs.minArgs   = fields[2].toInt(&numeric);
        if (!numeric || (cs.minArgs < 0)) {
            ok = false;
            continue;
        }
        bool bad = false;
        if (fields.size() > 3) {
            const auto roles = fields[3].split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (QString role : roles) {
                if (role.endsWith(QLatin1Char('*'))) {
                    cs.repeatLast = true;
                    role.chop(1);
                }
                ArgSpec as;
                if (role.startsWith(QStringLiteral("style:"))) {
                    const QString cat = role.mid(6);
                    if (!stylemap.contains(cat)) {
                        bad = true;
                        break;
                    }
                    as.role = ArgRole::Style;
                    as.cat  = stylemap.value(cat);
                } else if (role.startsWith(QStringLiteral("substyle:"))) {
                    const QString cat = role.mid(9);
                    if (!stylemap.contains(cat)) {
                        bad = true;
                        break;
                    }
                    as.role = ArgRole::SubStyle;
                    as.cat  = stylemap.value(cat);
                } else if (rolemap.contains(role)) {
                    as.role = rolemap.value(role);
                } else {
                    bad = true;
                    break;
                }
                cs.args.append(as);
            }
        }
        if (bad) {
            ok = false;
            continue;
        }
        if (specIndex.contains(cs.name)) {
            specs[specIndex.value(cs.name)] = cs;
        } else if (specs.size() >= SyntaxState::CMD_MASK) {
            // spec indices are stored in the block states; more entries than
            // the command bit field can hold would silently alias states
            ok = false;
        } else {
            specIndex.insert(cs.name, specs.size());
            specs.append(cs);
        }
    }
    return ok;
}

bool LammpsSyntax::knownStyle(StyleCat cat, const QString &name) const
{
    const int idx = static_cast<int>(cat);
    if ((idx < 0) || (idx >= NUM_STYLE_CATS)) return false;
    // "none" is accepted for the force field and kspace style commands but is
    // not always enumerated by the library introspection
    if (name == QStringLiteral("none")) {
        switch (cat) {
            case StyleCat::Pair:
            case StyleCat::Bond:
            case StyleCat::Angle:
            case StyleCat::Dihedral:
            case StyleCat::Improper:
            case StyleCat::Kspace:
                return true;
            default:
                break;
        }
    }
    return styles[idx].contains(name);
}

CmdCat LammpsSyntax::commandCategory(const QString &cmd) const
{
    const int idx = specIndex.value(cmd, -1);
    if (idx < 0) return CmdCat::Other;
    return specs[idx].category;
}

const CommandSpec *LammpsSyntax::spec(int index) const
{
    if ((index < 0) || (index >= specs.size())) return nullptr;
    return &specs[index];
}

ArgSpec LammpsSyntax::argSpec(int cmdIndex, int argIndex) const
{
    const auto *cs = spec(cmdIndex);
    if (!cs || (argIndex < 1)) return {};
    const int pos = argIndex - 1;
    if (pos < cs->args.size()) return cs->args[pos];
    if (cs->repeatLast && !cs->args.isEmpty()) return cs->args.last();
    return {};
}

QStringList LammpsSyntax::completionList(StyleCat cat, bool withNone) const
{
    const int idx = static_cast<int>(cat);
    if ((idx < 0) || (idx >= NUM_STYLE_CATS)) return {};
    QStringList list = completions[idx];
    if (withNone) {
        list << QStringLiteral("none");
        list.sort();
        list.removeDuplicates();
    }
    return list;
}

CompletionTarget LammpsSyntax::completionTarget(int prevBlockState, const QString &line,
                                                int cursorCol) const
{
    CompletionTarget target;
    const int state      = qMax(prevBlockState, 0);
    const LineTokens lt  = tokenizeLine(line, state);
    const bool freshLine = !SyntaxState::logicalContinues(state);

    // full text and span of each argument, and the argument under the cursor
    QHash<int, QString> argText;
    QHash<int, int> argStart, argEnd;
    int wordArg = -1;
    for (const auto &tok : lt.tokens) {
        if ((tok.argIndex < 0) || (tok.type == TokType::Comment) ||
            (tok.type == TokType::Continuation))
            continue;
        argText[tok.argIndex] += line.mid(tok.start, tok.length);
        if (!argStart.contains(tok.argIndex)) argStart[tok.argIndex] = tok.start;
        argEnd[tok.argIndex] = tok.start + tok.length;
        if ((cursorCol >= tok.start) && (cursorCol <= tok.start + tok.length))
            wordArg = tok.argIndex;
    }
    if (wordArg < 0) return target; // cursor is not on a word

    target.wordStart   = argStart.value(wordArg);
    target.wordLength  = argEnd.value(wordArg) - target.wordStart;
    const QString word = argText.value(wordArg);

    if (freshLine && (wordArg == 0)) {
        target.kind = CompleterKind::Command;
        return target;
    }
    if (wordArg < 1) return target;

    // the active command: word 0 on a fresh logical line, carried in the
    // block state on continuation lines
    const int cmdIdx  = freshLine ? commandIndex(argText.value(0)) : SyntaxState::cmdIndex(state);
    const QString cmd = freshLine ? argText.value(0)
                                  : (spec(cmdIdx) ? spec(cmdIdx)->name : QString());

    switch (argSpec(cmdIdx, wordArg).role) {
        case ArgRole::Style:
            target.kind = CompleterKind::Style;
            target.cat  = argSpec(cmdIdx, wordArg).cat;
            return target;
        case ArgRole::GroupId:
            target.kind = CompleterKind::Group;
            return target;
        case ArgRole::File:
            target.kind = CompleterKind::File;
            return target;
        default:
            break;
    }

    // "pair_coeff * *" can read coefficients from a potential file
    if ((cmd == QStringLiteral("pair_coeff")) && (wordArg == 3) &&
        (argText.value(1) == QStringLiteral("*")) && (argText.value(2) == QStringLiteral("*"))) {
        target.kind = CompleterKind::File;
        return target;
    }
    // "extra/..." keywords of the read_data command
    if ((cmd == QStringLiteral("read_data")) && (wordArg >= 2) &&
        word.startsWith(QStringLiteral("ex"))) {
        target.kind = CompleterKind::Extra;
        return target;
    }
    // v_/c_/f_ references complete at any argument position
    if (word.startsWith(QStringLiteral("v_")))
        target.kind = CompleterKind::VarName;
    else if (word.startsWith(QStringLiteral("c_")) || word.startsWith(QStringLiteral("C_")))
        target.kind = CompleterKind::ComputeId;
    else if (word.startsWith(QStringLiteral("f_")) || word.startsWith(QStringLiteral("F_")))
        target.kind = CompleterKind::FixId;

    // sub-styles of hybrid styles complete from the category's style list;
    // only when a style-like word was started, since these positions can
    // also hold numeric or other sub-style arguments
    if ((target.kind == CompleterKind::None) && !word.isEmpty() && word.at(0).isLetter()) {
        const ArgSpec as = argSpec(cmdIdx, wordArg);
        if (as.role == ArgRole::SubStyle) {
            target.kind = CompleterKind::Style;
            target.cat  = as.cat;
        }
    }

    // dump image and dump_modify: complete color names in the color-value
    // positions of the keywords that take one, and the dump image keywords
    // in the keyword section of a dump image command
    if ((target.kind == CompleterKind::None) && !word.isEmpty() && word.at(0).isLetter()) {
        const QString prev     = argText.value(wordArg - 1);
        const QString prevprev = argText.value(wordArg - 2);
        if ((cmd == QStringLiteral("dump")) && (wordArg >= 8) &&
            (!argText.contains(3) || (argText.value(3) == QStringLiteral("image")) ||
             (argText.value(3) == QStringLiteral("movie")))) {
            // keyword = color ... (bond, line, ...), keyword = ID color ...
            // (compute, fix, region), or keyword = flag value color ...
            // (depthcue, outline)
            static const QSet<QString> colorAfter = {
                QStringLiteral("bond"), QStringLiteral("line"), QStringLiteral("tri"),
                QStringLiteral("ellipsoid"), QStringLiteral("body")};
            static const QSet<QString> colorSecond = {
                QStringLiteral("compute"), QStringLiteral("fix"), QStringLiteral("region")};
            static const QSet<QString> colorFourth = {QStringLiteral("depthcue"),
                                                      QStringLiteral("outline")};
            target.kind                            = CompleterKind::Style;
            target.cat = (colorAfter.contains(prev) || colorSecond.contains(prevprev) ||
                          colorFourth.contains(argText.value(wordArg - 3)))
                             ? StyleCat::Color
                             : StyleCat::ImageKw;
        } else if ((cmd == QStringLiteral("dump_modify")) && (wordArg >= 2)) {
            static const QSet<QString> colorAfter  = {QStringLiteral("backcolor"),
                                                      QStringLiteral("backcolor2"),
                                                      QStringLiteral("boxcolor")};
            static const QSet<QString> colorSecond = {QStringLiteral("acolor"),
                                                      QStringLiteral("bcolor")};
            if (colorAfter.contains(prev) || colorSecond.contains(prevprev)) {
                target.kind = CompleterKind::Style;
                target.cat  = StyleCat::Color;
            }
        }
    }
    return target;
}

// ----------------------------------------------------------------------------

void InputScanner::feedLine(int lineNo, const QString &text)
{
    const LineTokens lt = tokenizeLine(text, state);
    if (lt.unbalancedQuote) diags.append({Diag::UnbalancedQuote, lineNo});

    for (const auto &tok : lt.tokens) {
        if ((tok.argIndex < 0) || (tok.type == TokType::Comment) ||
            (tok.type == TokType::Continuation))
            continue;
        if (current.words.isEmpty()) current.firstLine = lineNo;
        // words are indexed by the absolute argument position within the logical
        // line; adjacent tokens of the same argument are joined
        while (current.words.size() <= tok.argIndex)
            current.words.append(Word{});
        auto &word = current.words[tok.argIndex];
        if (word.text.isEmpty()) word.line = lineNo;
        word.text += text.mid(tok.start, tok.length);
        if (tok.type != TokType::Word) word.quoted = true;
        current.lastLine = lineNo;
    }

    state = lt.outState;
    // the logical line is complete when no multi-line construct stays open
    if (!SyntaxState::logicalContinues(state)) flush();
}

void InputScanner::finish(int lastLineNo)
{
    const int flagbits = SyntaxState::flags(state);
    if (flagbits & SyntaxState::TRIPLE) diags.append({Diag::UnclosedTriple, lastLineNo});
    if (flagbits & SyntaxState::CONTINUE) diags.append({Diag::DanglingContinuation, lastLineNo});
    flush();
    state = 0;
}

void InputScanner::scan(const QString &buffer)
{
    const auto lines = buffer.split(QLatin1Char('\n'));
    int lineNo       = 0;
    for (const auto &line : lines)
        feedLine(++lineNo, line);
    finish(qMax(lineNo, 1));
}

void InputScanner::flush()
{
    if (!current.words.isEmpty()) {
        for (auto &word : current.words) {
            word.hasSubst = word.text.contains(C_DOLLAR);
            // strip one pair of surrounding matching quotes
            if (word.text.size() >= 2) {
                const QChar first = word.text.front();
                const QChar last  = word.text.back();
                if ((first == last) && ((first == C_SQUOTE) || (first == C_DQUOTE))) {
                    if ((word.text.size() >= 6) && word.text.startsWith(QStringLiteral("\"\"\"")) &&
                        word.text.endsWith(QStringLiteral("\"\"\"")))
                        word.text = word.text.mid(3, word.text.size() - 6);
                    else
                        word.text = word.text.mid(1, word.text.size() - 2);
                }
            }
        }
        cmds.append(current);
    }
    current = {};
}

// Local Variables:
// c-basic-offset: 4
// End:
