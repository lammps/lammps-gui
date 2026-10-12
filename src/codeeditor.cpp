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

#include "codeeditor.h"
#include "constants.h"
#include "fileviewer.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "lammpssyntax.h"
#include "lammpswrapper.h"
#include "linenumberarea.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QRect>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QToolTip>
#include <QUrl>
#include <QVariant>
#include <QWidget>

CodeEditor::CodeEditor(QWidget *parent) :
    QPlainTextEdit(parent), currentComp(nullptr), commandComp(new QCompleter(this)),
    fixComp(new QCompleter(this)), computeComp(new QCompleter(this)),
    dumpComp(new QCompleter(this)), atomComp(new QCompleter(this)), pairComp(new QCompleter(this)),
    bondComp(new QCompleter(this)), angleComp(new QCompleter(this)),
    dihedralComp(new QCompleter(this)), improperComp(new QCompleter(this)),
    kspaceComp(new QCompleter(this)), regionComp(new QCompleter(this)),
    integrateComp(new QCompleter(this)), minimizeComp(new QCompleter(this)),
    variableComp(new QCompleter(this)), unitsComp(new QCompleter(this)),
    groupComp(new QCompleter(this)), varnameComp(new QCompleter(this)),
    fixidComp(new QCompleter(this)), compidComp(new QCompleter(this)),
    fileComp(new QCompleter(this)), extraComp(new QCompleter(this)),
    colorComp(new QCompleter(this)), imagekwComp(new QCompleter(this)), highlight(NO_HIGHLIGHT),
    highlighterror(false), reformatOnReturn(false), automaticCompletion(true), docver("")
{
    // owned by this widget, not by the main window: a QShortcut parented to the
    // main window is a sibling of this editor, and when the main window deletes
    // its children the two race -- whichever goes first leaves the other with a
    // dangling pointer.  Qt::WindowShortcut resolves to the containing window
    // either way, so the scope is unchanged.
    helpAction = new QShortcut(QKeySequence::fromString("Ctrl+?"), this);
    connect(helpAction, &QShortcut::activated, this, &CodeEditor::getHelp);

    // set up each completer with consistent settings
    auto setupCompleter = [this](QCompleter *completer) {
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
        completer->setWidget(this);
        completer->setMaxVisibleItems(16);
        completer->setWrapAround(false);
        connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this,
                &CodeEditor::insertCompletedCommand);
    };

    for (auto *c :
         {commandComp,   fixComp,      computeComp,  dumpComp,     atomComp,   pairComp,
          bondComp,      angleComp,    dihedralComp, improperComp, kspaceComp, regionComp,
          integrateComp, minimizeComp, variableComp, unitsComp,    groupComp,  varnameComp,
          fixidComp,     compidComp,   fileComp,     extraComp,    colorComp,  imagekwComp})
        setupCompleter(c);

    // initialize help system
    QFile help_index(":/help_index.table");
    if (help_index.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!help_index.atEnd()) {
            auto line  = QString(help_index.readLine());
            auto words = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (words.size() > 2) {

                if (words.at(1) == "pair_style") {
                    pairMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "bond_style") {
                    bondMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "angle_style") {
                    angleMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "dihedral_style") {
                    dihedralMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "improper_style") {
                    improperMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "fix") {
                    fixMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "compute") {
                    computeMap[words.at(2)] = words.at(0);
                } else if (words.at(1) == "kspace_style") {
                    cmdMap["kspace_style"] = "kspace_style.html";
                }
                // ignoring: dump, fix_modify ATC
            } else if (words.size() == 2) {
                cmdMap[words.at(1)] = words.at(0);
            } else {
                fprintf(stderr, "unhandled help item: %s\n", qPrintable(line.trimmed()));
            }
        }
        help_index.close();
    }

    setBackgroundRole(QPalette::Light);
    lineNumberArea = new LineNumberArea(this);
    lineNumberArea->setBackgroundRole(QPalette::Dark);
    lineNumberArea->setAutoFillBackground(true);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    updateLineNumberAreaWidth(0);
    setCursorWidth(2);
}

// every child (helpAction, lineNumberArea, the completers) is a Qt child of
// this widget and is deleted by Qt's parent-child ownership
CodeEditor::~CodeEditor() = default;

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max    = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + (fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 2));
    return space;
}

void CodeEditor::setFont(const QFont &newfont)
{
    lineNumberArea->setFont(newfont);
    document()->setDefaultFont(newfont);
}

void CodeEditor::setCursor(int block)
{
    // move cursor to given position
    auto cursor = textCursor();
    auto bl     = document()->findBlockByNumber(block);
    if (bl.isValid()) {
        cursor.setPosition(bl.position());
        setTextCursor(cursor);
    }
}

void CodeEditor::setHighlight(int block, bool error)
{
    // a separate error flag: encoding the error state in the sign of the
    // block number cannot represent an error on block 0
    highlight      = block;
    highlighterror = error;

    // also reset the cursor
    setCursor(block);

    // update graphics
    repaint();
}

// reformat line

QString CodeEditor::reformatLine(const QString &line)
{
    auto words = splitLine(line);
    QString newtext;
    QSettings settings;
    settings.beginGroup(Keys::GROUP_REFORMAT);
    int cmdsize  = settings.value(Keys::COMMAND, "16").toInt();
    int typesize = settings.value(Keys::TYPE, "4").toInt();
    int idsize   = settings.value(Keys::ID, "4").toInt();
    int namesize = settings.value(Keys::NAME, "8").toInt();
    settings.endGroup();

    bool rebuildGroupComp     = false;
    bool rebuildVarNameComp   = false;
    bool rebuildComputeIDComp = false;
    bool rebuildFixIDComp     = false;

    if (!words.isEmpty()) {
        // commented line. do nothing
        if (words[0][0] == '#') return line;

        // start with LAMMPS command plus padding if another word follows
        newtext = words[0];
        if (words.size() > 1) {
            for (int i = words[0].size() + 1; i < cmdsize; ++i)
                newtext += ' ';
            // new/updated group command -> update completer
            if (words[0] == "group") rebuildGroupComp = true;
            // new/updated variable command -> update completer
            if (words[0] == "variable") rebuildVarNameComp = true;
            // new/updated compute command -> update completer
            if (words[0] == "compute") rebuildComputeIDComp = true;
            // new/updated fix command -> update completer
            if (words[0] == "fix") rebuildFixIDComp = true;
        }

        // append remaining words with just a single blank added.
        for (int i = 1; i < words.size(); ++i) {
            newtext += ' ';
            newtext += words[i];

            // special cases

            if (i < 3) {
                // additional space for types or type ranges
                if (words[0] == "pair_coeff")
                    for (int j = words[i].size(); j < typesize; ++j)
                        newtext += ' ';

                // pad 4 for IDs and 8 for groups
                if ((words[0] == "fix") || (words[0] == "compute") || (words[0] == "dump")) {
                    if (i == 1) {
                        for (int j = words[i].size(); j < idsize; ++j)
                            newtext += ' ';
                    } else if (i == 2) {
                        for (int j = words[i].size(); j < namesize; ++j)
                            newtext += ' ';
                    }
                }
            }

            if (i < 2) {
                if ((words[0] == "bond_coeff") || (words[0] == "angle_coeff") ||
                    (words[0] == "dihedral_coeff") || (words[0] == "improper_coeff") ||
                    (words[0] == "mass"))
                    for (int j = words[i].size(); j < typesize; ++j)
                        newtext += ' ';
            }
        }
    }
    if (rebuildGroupComp) setGroupList();
    if (rebuildVarNameComp) setVarNameList();
    if (rebuildComputeIDComp) setComputeIDList();
    if (rebuildFixIDComp) setFixIDList();
    return newtext;
}

#define COMPLETER_INIT_FUNC(keyword, Type)                                   \
    void CodeEditor::set##Type##List(const QStringList &words)               \
    {                                                                        \
        keyword##Comp->setModel(new QStringListModel(words, keyword##Comp)); \
    }

COMPLETER_INIT_FUNC(command, Command)
COMPLETER_INIT_FUNC(fix, Fix)
COMPLETER_INIT_FUNC(compute, Compute)
COMPLETER_INIT_FUNC(dump, Dump)
COMPLETER_INIT_FUNC(atom, Atom)
COMPLETER_INIT_FUNC(pair, Pair)
COMPLETER_INIT_FUNC(bond, Bond)
COMPLETER_INIT_FUNC(angle, Angle)
COMPLETER_INIT_FUNC(dihedral, Dihedral)
COMPLETER_INIT_FUNC(improper, Improper)
COMPLETER_INIT_FUNC(kspace, Kspace)
COMPLETER_INIT_FUNC(region, Region)
COMPLETER_INIT_FUNC(integrate, Integrate)
COMPLETER_INIT_FUNC(minimize, Minimize)
COMPLETER_INIT_FUNC(variable, Variable)
COMPLETER_INIT_FUNC(units, Units)
COMPLETER_INIT_FUNC(extra, Extra)
COMPLETER_INIT_FUNC(color, Color)
COMPLETER_INIT_FUNC(imagekw, ImageKw)

#undef COMPLETER_INIT_FUNC

// build completer for groups by parsing through edit buffer

namespace {

// collect the IDs defined by all logical commands with the given name; the
// InputScanner joins '&' continuations and skips commented-out definitions
QStringList scanDefinedIds(const QString &buffer, const QString &command)
{
    QStringList ids;
    InputScanner scanner;
    scanner.scan(buffer);
    for (const auto &cmd : scanner.commands()) {
        if ((cmd.words.size() > 1) && (cmd.words[0].text == command)) {
            const QString &id = cmd.words[1].text;
            if (!id.isEmpty() && !ids.contains(id)) ids << id;
        }
    }
    return ids;
}

} // namespace

void CodeEditor::setGroupList()
{
    auto groups = scanDefinedIds(document()->toPlainText(), QStringLiteral("group"));
    groups.sort();
    groups.prepend(QStringLiteral("all"));
    groupComp->setModel(new QStringListModel(groups, groupComp));
}

void CodeEditor::setVarNameList()
{
    QStringList vars;

    // variable "gui_run" is always defined by LAMMPS-GUI
    vars << QString("${gui_run}");
    vars << QString("v_gui_run");

    LammpsWrapper *lammps = &qobject_cast<LammpsGui *>(parent())->lammps;
    int nvar              = lammps->idCount("variable");
    for (int i = 0; i < nvar; ++i) {
        const QString name = lammps->variableInfo(i);
        if (!name.isEmpty()) {
            if (name.size() == 1) vars << QString("$%1").arg(name);
            vars << QString("${%1}").arg(name);
            vars << QString("v_%1").arg(name);
        }
    }

    for (const auto &name : scanDefinedIds(document()->toPlainText(), QStringLiteral("variable"))) {
        QString w = QString("$%1").arg(name);
        if ((name.size() == 1) && !vars.contains(w)) vars << w;
        w = QString("${%1}").arg(name);
        if (!vars.contains(w)) vars << w;
        w = QString("v_%1").arg(name);
        if (!vars.contains(w)) vars << w;
    }
    vars.sort();
    varnameComp->setModel(new QStringListModel(vars, varnameComp));
}

void CodeEditor::setComputeIDList()
{
    QStringList compid;
    for (const auto &name : scanDefinedIds(document()->toPlainText(), QStringLiteral("compute"))) {
        compid << QString("c_%1").arg(name);
        compid << QString("C_%1").arg(name);
    }
    compid.sort();
    compidComp->setModel(new QStringListModel(compid, compidComp));
}

void CodeEditor::setFixIDList()
{
    QStringList fixid;
    for (const auto &name : scanDefinedIds(document()->toPlainText(), QStringLiteral("fix"))) {
        fixid << QString("f_%1").arg(name);
        fixid << QString("F_%1").arg(name);
    }
    fixid.sort();
    fixidComp->setModel(new QStringListModel(fixid, fixidComp));
}

void CodeEditor::setFileList()
{
    QStringList files;
    QDir dir(".");
    for (const auto &file : dir.entryInfoList(QDir::Files))
        files << file.fileName();
    files.sort();
    fileComp->setModel(new QStringListModel(files, fileComp));
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    const auto key = event->key();

    if (currentComp && currentComp->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
        switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                event->ignore();
                return; // let the completer do default behavior
            default:
                break;
        }
    }

    // reformat current line and consume key event
    if (key == Qt::Key_Tab) {
        reformatCurrentLine();
        return;
    }

    // run command completion and consume key event
    if (key == Qt::Key_Backtab) {
        runCompletion();
        return;
    }

    // automatically reformat when hitting the return or enter key; the flag is
    // maintained through setReformatOnReturn() when the preferences change --
    // re-reading QSettings here would both override the setter and cost a
    // settings lookup on every keystroke
    if (reformatOnReturn && ((key == Qt::Key_Return) || (key == Qt::Key_Enter))) {
        reformatCurrentLine();
    }

    // process key event in parent class
    QPlainTextEdit::keyPressEvent(event);

    // if enabled, try pop up completion automatically after 2 characters
    if (automaticCompletion) {
        auto cursor = textCursor();
        auto line   = cursor.block().text();
        if (line.isEmpty()) return;

        // QTextCursor::WordUnderCursor is unusable here since it recognizes '/' as word boundary.
        // Work around it by manually searching for the location of the beginning of the word.
        int begin = qMin(cursor.positionInBlock(), line.length() - 1);

        while (begin >= 0) {
            if (line[begin].isSpace()) break;
            --begin;
        }
        if (((cursor.positionInBlock() - begin) > 2) ||
            ((line.length() > begin + 1) && (line[begin + 1] == '$')))
            runCompletion();
        if (currentComp && currentComp->popup()->isVisible() &&
            ((cursor.positionInBlock() - begin) < 2)) {
            currentComp->popup()->hide();
        }
    }
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void CodeEditor::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void CodeEditor::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
    cut();
    QPlainTextEdit::dragLeaveEvent(event);
}

bool CodeEditor::canInsertFromMimeData(const QMimeData *source) const
{
    return source->hasUrls() || source->hasText();
}

void CodeEditor::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->accept();
        auto file = event->mimeData()->urls()[0].toLocalFile();
        auto *gui = qobject_cast<LammpsGui *>(parent());
        if (gui) {
            moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
            gui->openFile(file);
        }
        // properly handle drop event in base class, but set editor
        // buffer readonly to prevent undesired changes
        setReadOnly(true);
        QPlainTextEdit::dropEvent(event);
        setReadOnly(false);
    } else if (event->mimeData()->hasText()) {
        event->accept();
        // cut selected text to clipboard before we reposition
        // the cursor and re-insert the text with drag-n-drop
        cut();
        cursorForPosition(event->position().toPoint()).insertText(event->mimeData()->text());
        // properly handle drop event in base class, but set editor
        // buffer readonly to prevent undesired changes
        setReadOnly(true);
        QPlainTextEdit::dropEvent(event);
        setReadOnly(false);
    } else
        event->ignore();
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::setVariableOverrides(const QList<VariableEntry> &vars)
{
    variableOverrides.clear();
    for (const auto &var : vars)
        if (isOverridden(var)) variableOverrides.insert(var.name, var);
    viewport()->update();
}

namespace {
// an override marker is only valid while the definition line still assigns
// the value the override was based on: as soon as the line is edited the
// script value wins and the marker is stale
bool isMarkedOverride(const QHash<QString, VariableEntry> &overrides,
                      const IndexVariableMatch &match)
{
    if (!match.valid) return false;
    const auto entry = overrides.constFind(match.name);
    return (entry != overrides.constEnd()) && (entry->scriptValue == match.value);
}
} // namespace

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);
    if (variableOverrides.isEmpty()) return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().color(QPalette::Highlight), 1.0));
    painter.setBrush(Qt::NoBrush);

    const QPointF offset = contentOffset();
    QTextBlock block     = firstVisibleBlock();
    while (block.isValid()) {
        const QRectF geom = blockBoundingGeometry(block).translated(offset);
        if (geom.top() > event->rect().bottom()) break;
        if (block.isVisible() && (geom.bottom() >= event->rect().top())) {
            const auto match = matchIndexVariable(block.text());
            if (isMarkedOverride(variableOverrides, match)) {
                // frame the value text; with line wrapping enabled it may
                // span multiple text lines of the same block
                const auto *layout = block.layout();
                const int start    = match.valueStart;
                const int end      = match.valueStart + match.valueLength;
                const auto first   = layout->lineForTextPosition(start);
                const auto last    = layout->lineForTextPosition(end > start ? end - 1 : start);
                if (first.isValid() && last.isValid()) {
                    for (int i = first.lineNumber(); i <= last.lineNumber(); ++i) {
                        const auto line = layout->lineAt(i);
                        const qreal x1  = line.cursorToX(qMax(start, line.textStart()));
                        const qreal x2 =
                            line.cursorToX(qMin(end, line.textStart() + line.textLength()));
                        const QRectF frame(geom.left() + x1 - 2.0, geom.top() + line.y() + 0.5,
                                           x2 - x1 + 4.0, line.height() - 1.0);
                        painter.drawRoundedRect(frame, 2.0, 2.0);
                    }
                }
            }
        }
        block = block.next();
    }
}

bool CodeEditor::event(QEvent *event)
{
    if ((event->type() == QEvent::ToolTip) && !variableOverrides.isEmpty()) {
        auto *helpEvent   = static_cast<QHelpEvent *>(event);
        const auto cursor = cursorForPosition(helpEvent->pos());
        const auto match  = matchIndexVariable(cursor.block().text());
        const int pos     = cursor.positionInBlock();
        if (isMarkedOverride(variableOverrides, match) && (pos >= match.valueStart) &&
            (pos <= match.valueStart + match.valueLength)) {
            QToolTip::showText(helpEvent->globalPos(),
                               QString("Value is overridden from the Set Variables dialog: %1")
                                   .arg(variableOverrides.value(match.name).value),
                               this);
            return true;
        }
        QToolTip::hideText();
    }
    return QPlainTextEdit::event(event);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    QTextBlock block = firstVisibleBlock();
    int blockNumber  = block.blockNumber();

    int top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1) + " ";
            if ((highlight == NO_HIGHLIGHT) || (blockNumber != highlight)) {
                painter.setPen(palette().color(QPalette::WindowText));
            } else {
                number = QString(">") + QString::number(blockNumber + 1) + "<";
                if (highlighterror)
                    painter.fillRect(0, top, lineNumberArea->width(), fontMetrics().height(),
                                     Qt::darkRed);
                else
                    painter.fillRect(0, top, lineNumberArea->width(), fontMetrics().height(),
                                     Qt::darkGreen);

                painter.setPen(Qt::white);
            }
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    // reposition the cursor here, but only if there is no active selection
    if (!textCursor().hasSelection()) setTextCursor(cursorForPosition(event->pos()));

    QString page, help;
    findHelp(page, help);

    auto *menu = createStandardContextMenu();
    menu->addSeparator();
    auto *gui = qobject_cast<LammpsGui *>(parent());
    if (textCursor().hasSelection()) {
        addMenuAction(menu, "Comment out selection", ":/icons/comment-out.svg", this,
                      &CodeEditor::commentSelection);
        addMenuAction(menu, "Uncomment selection", ":/icons/uncomment.svg", this,
                      &CodeEditor::uncommentSelection);
    } else {
        addMenuAction(menu, "Comment out line", ":/icons/comment-out.svg", this,
                      &CodeEditor::commentLine);
        addMenuAction(menu, "Uncomment line", ":/icons/uncomment.svg", this,
                      &CodeEditor::uncommentLine);
    }
    menu->addSeparator();
    LammpsWrapper *lammps = &gui->lammps;
    if (lammps->isRunning()) {
        addMenuAction(menu, "Stop LAMMPS", ":/icons/process-stop.svg", gui, &LammpsGui::stopRun);
    } else {
        addMenuAction(menu, "Run LAMMPS from Editor Buffer", ":/icons/system-run.svg", gui,
                      &LammpsGui::runBuffer);
        addMenuAction(menu, "Run LAMMPS from File", ":/icons/run-file.svg", gui,
                      &LammpsGui::runFile);
    }
    menu->addSeparator();

    // offer the Set Variables dialog when the line defines an index variable
    if (matchIndexVariable(textCursor().block().text()).valid) {
        addMenuAction(menu, "Set Variables...", ":/icons/preferences-desktop.svg", gui,
                      &LammpsGui::editVariables);
        menu->addSeparator();
    }

    // print augmented context menu if an entry was found
    if (!help.isEmpty()) {
        addMenuAction(menu, QString("Display available completions for '%1'").arg(help),
                      ":/icons/expand-text.svg", this, &CodeEditor::runCompletion);
        menu->addSeparator();
    }

    if (!page.isEmpty()) {
        addMenuAction(menu, QString("Reformat '%1' command").arg(help),
                      ":/icons/format-indent-less-3.svg", this, &CodeEditor::reformatCurrentLine);

        menu->addSeparator();
        addMenuAction(menu, QString("View Documentation for '%1'").arg(help),
                      ":/icons/system-help.svg", this, &CodeEditor::openHelp)
            ->setData(page);
        // if we link to help with specific styles (fix, compute, pair, bond, ...)
        // also link to the docs for the primary command
        auto words = help.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 1) {
            help = words.at(0);
            page = words.at(0);
            page += ".html";
            addMenuAction(menu, QString("View Documentation for '%1'").arg(help),
                          ":/icons/system-help.svg", this, &CodeEditor::openHelp)
                ->setData(page);
        }
    }

    // check if word under cursor is file
    {
        auto cursor = textCursor();
        auto line   = cursor.block().text();
        if (!line.isEmpty()) {
            // QTextCursor::WordUnderCursor is unusable here since it recognizes '/' as word
            // boundary. Work around it by manually searching for the location of the beginning of
            // the word.
            int begin = qMin(cursor.positionInBlock(), line.length() - 1);

            while (begin >= 0) {
                if (line[begin].isSpace()) break;
                --begin;
            }
            int end = begin + 1;
            while (end < line.length()) {
                if (line[end].isSpace()) break;
                ++end;
            }

            QString word = line.mid(begin, end - begin).trimmed();
            QFileInfo fi(word);
            if (fi.exists() && fi.isFile()) {
                // check if file is a LAMMPS restart
                if (isRestartFile(word)) {
                    addMenuAction(menu, QString("Inspect restart file '%1'").arg(word),
                                  ":/icons/document-open.svg", this, &CodeEditor::inspectFile)
                        ->setData(word);
                } else {
                    addMenuAction(menu, QString("View file '%1'").arg(word),
                                  ":/icons/document-open.svg", this, &CodeEditor::viewFile)
                        ->setData(word);
                }
            }
        }
    }

    addMenuAction(menu, QString("LAMMPS Commands Overview"), ":/icons/help-browser.svg", this,
                  &CodeEditor::openHelp)
        ->setData(QString("/Commands_all.html"));

    addMenuAction(menu, QString("LAMMPS Manual"), ":/icons/help-browser.svg", this,
                  &CodeEditor::openHelp)
        ->setData(QString());

    addMenuAction(menu, QString("LAMMPS Tutorial"), ":/icons/tutorial-logo.png", this,
                  &CodeEditor::openUrl)
        ->setData(QString("https://lammpstutorials.github.io/"));

    menu->exec(event->globalPos());
    delete menu;
}

void CodeEditor::reformatCurrentLine()
{
    auto cursor  = textCursor();
    auto text    = cursor.block().text();
    auto newtext = reformatLine(text);

    // perform edit but only if text has changed
    if (QString::compare(text, newtext)) {
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
        cursor.insertText(newtext);
        cursor.endEditBlock();
    }
}

void CodeEditor::commentLine()
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("#");
}

void CodeEditor::commentSelection()
{
    auto cursor = textCursor();
    auto text   = cursor.selection().toPlainText();
    auto lines  = text.split('\n');
    QString newtext;
    for (const auto &line : lines) {
        newtext.append('#');
        newtext.append(line);
        newtext.append('\n');
    }
    if (newtext.isEmpty()) newtext = "#\n";
    cursor.insertText(newtext);
    setTextCursor(cursor);
}

void CodeEditor::uncommentSelection()
{
    auto cursor = textCursor();
    auto text   = cursor.selection().toPlainText();
    auto lines  = text.split('\n');
    QString newtext;
    for (const auto &line : lines) {
        QString newline;
        bool start = true;
        for (auto letter : line) {
            if (start && (letter == '#')) {
                start = false;
                continue;
            }
            if (start && !letter.isSpace()) start = false;
            newline.append(letter);
        }
        newtext.append(newline);
        newtext.append('\n');
    }
    cursor.insertText(newtext);
    setTextCursor(cursor);
}

void CodeEditor::uncommentLine()
{
    auto cursor = textCursor();
    auto text   = cursor.block().text();
    QString newtext;
    bool start = true;
    for (auto letter : text) {
        if (start && (letter == '#')) {
            start = false;
            continue;
        }
        if (start && !letter.isSpace()) start = false;
        newtext.append(letter);
    }

    // perform edit but only if text has changed
    if (QString::compare(text, newtext)) {
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
        cursor.insertText(newtext);
        cursor.endEditBlock();
    }
}

// Pop up (or hide) the completion list of currentComp for the given prefix,
// hiding the popup of a previously active completer. Shared by all completion
// contexts past the first word in CodeEditor::runCompletion().
void CodeEditor::popupCompletion(const QString &prefix, QAbstractItemView *oldPopup)
{
    currentComp->setCompletionPrefix(prefix);
    if (oldPopup && (oldPopup != currentComp->popup())) oldPopup->hide();
    auto *popup = currentComp->popup();
    // if the word is already a complete command, remove an existing popup
    if (prefix == currentComp->currentCompletion()) {
        if (popup->isVisible()) popup->hide();
        return;
    }
    QRect cr = cursorRect();
    cr.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width());
    popup->setAlternatingRowColors(true);
    currentComp->complete(cr);
}

void CodeEditor::runCompletion()
{
    QAbstractItemView *oldPopup = nullptr;
    if (currentComp) oldPopup = currentComp->popup();
    if (!syntax) return;

    const auto cursor = textCursor();
    const auto block  = cursor.block();
    const auto line   = block.text();
    // no completion possible on empty lines
    if (line.trimmed().isEmpty()) return;

    // classify the word under the cursor with the syntax engine; the block
    // state of the previous line carries the active command across '&' line
    // continuations.  Falls back to fresh-line classification when the block
    // has not been highlighted yet (state -1).
    const int prevState = block.previous().isValid() ? block.previous().userState() : 0;
    const auto target   = syntax->completionTarget(prevState, line, cursor.positionInBlock());
    if (target.kind == CompleterKind::None) return;
    const auto word = line.mid(target.wordStart, target.wordLength);

    if (target.kind == CompleterKind::Command) {
        currentComp = commandComp;
        currentComp->setCompletionPrefix(word);
        if (oldPopup && (oldPopup != currentComp->popup())) oldPopup->hide();
        auto *popup = currentComp->popup();
        // if the command is already a complete command, remove existing popup
        if (word == currentComp->currentCompletion()) {
            if (popup->isVisible()) {
                popup->hide();
                currentComp = nullptr;
            }
            return;
        }
        QRect cr = cursorRect();
        cr.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width());
        popup->setAlternatingRowColors(true);
        currentComp->complete(cr);
        return;
    }

    QCompleter *comp = nullptr;
    switch (target.kind) {
        case CompleterKind::Style:
            switch (target.cat) {
                case StyleCat::Fix:
                    comp = fixComp;
                    break;
                case StyleCat::Compute:
                    comp = computeComp;
                    break;
                case StyleCat::Dump:
                    comp = dumpComp;
                    break;
                case StyleCat::Atom:
                    comp = atomComp;
                    break;
                case StyleCat::Pair:
                    comp = pairComp;
                    break;
                case StyleCat::Bond:
                    comp = bondComp;
                    break;
                case StyleCat::Angle:
                    comp = angleComp;
                    break;
                case StyleCat::Dihedral:
                    comp = dihedralComp;
                    break;
                case StyleCat::Improper:
                    comp = improperComp;
                    break;
                case StyleCat::Kspace:
                    comp = kspaceComp;
                    break;
                case StyleCat::Region:
                    comp = regionComp;
                    break;
                case StyleCat::Integrate:
                    comp = integrateComp;
                    break;
                case StyleCat::Minimize:
                    comp = minimizeComp;
                    break;
                case StyleCat::Variable:
                    comp = variableComp;
                    break;
                case StyleCat::Units:
                    comp = unitsComp;
                    break;
                case StyleCat::Extra:
                    comp = extraComp;
                    break;
                case StyleCat::Color:
                    comp = colorComp;
                    break;
                case StyleCat::ImageKw:
                    comp = imagekwComp;
                    break;
                default:
                    break;
            }
            break;
        case CompleterKind::Group:
            comp = groupComp;
            break;
        case CompleterKind::VarName:
            comp = varnameComp;
            break;
        case CompleterKind::ComputeId:
            comp = compidComp;
            break;
        case CompleterKind::FixId:
            comp = fixidComp;
            break;
        case CompleterKind::Extra:
            comp = extraComp;
            break;
        case CompleterKind::File:
            // no file name completion when the word already contains a path
            if (word.contains('/')) {
                if (oldPopup && oldPopup->isVisible()) oldPopup->hide();
                return;
            }
            comp = fileComp;
            break;
        default:
            break;
    }
    if (!comp) return;
    currentComp = comp;
    popupCompletion(word, oldPopup);
}

void CodeEditor::insertCompletedCommand(const QString &completion)
{
    auto *completer = qobject_cast<QCompleter *>(sender());
    if (completer->widget() != this) return;

    // select the entire word (non-space text) under the cursor
    // we need to do it in this complicated way, since QTextCursor does not recognize
    // special characters as part of a word.
    auto cursor = textCursor();
    auto line   = cursor.block().text();
    int begin   = qMin(cursor.positionInBlock(), line.length() - 1);

    while (begin >= 0) {
        if (line[begin].isSpace()) break;
        --begin;
    }

    int end = begin + 1;
    while (end < line.length()) {
        if (line[end].isSpace()) break;
        ++end;
    }

    cursor.setPosition(cursor.position() - cursor.positionInBlock() + begin + 1);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, end - begin - 1);
    cursor.insertText(completion);
    setTextCursor(cursor);
}

void CodeEditor::setDocver()
{
    LammpsWrapper *lammps = &qobject_cast<LammpsGui *>(parent())->lammps;
    docver                = "/";
    {
        QString git_branch = static_cast<const char *>(lammps->extractGlobal("git_branch"));
        if ((git_branch == "stable") || (git_branch == "maintenance")) {
            docver = "/stable/";
        } else if (git_branch == "release") {
            docver = "/";
        } else {
            docver = "/latest/";
        }
    }
}

void CodeEditor::getHelp()
{
    QString page, help;
    findHelp(page, help);
    if (docver.isEmpty()) setDocver();
    if (!page.isEmpty())
        QDesktopServices::openUrl(QUrl(QString("%1%2%3").arg(Cfg::DOCS_URL, docver, page)));
}

void CodeEditor::findHelp(QString &page, QString &help)
{
    help.clear();
    page.clear();

    // tokenize the line under the cursor; the block state of the previous
    // line resolves the command of '&' continuation lines
    const auto block    = textCursor().block();
    const QString text  = block.text();
    const int prevState = block.previous().isValid() ? qMax(block.previous().userState(), 0) : 0;
    const LineTokens lt = tokenizeLine(text, prevState);

    const QHash<int, QString> argText = argumentTexts(lt, text);

    const bool freshLine = !SyntaxState::logicalContinues(prevState);
    int cmdIdx           = -1;
    QString cmd;
    if (freshLine) {
        cmd = argText.value(0);
        if (syntax) cmdIdx = syntax->commandIndex(cmd);
    } else if (syntax) {
        cmdIdx         = SyntaxState::cmdIndex(prevState);
        const auto *cs = syntax->spec(cmdIdx);
        if (cs) cmd = cs->name;
    }
    if (cmd.isEmpty()) return;

    // when the command has a style-name argument with a dedicated doc page,
    // prefer that page over the command page
    const auto *cs = syntax ? syntax->spec(cmdIdx) : nullptr;
    if (cs) {
        for (int i = 0; i < cs->args.size(); ++i) {
            if (cs->args[i].role != ArgRole::Style) continue;
            const QString styleword = argText.value(i + 1);
            if (styleword.isEmpty()) break;
            const QMap<QString, QString> *map = nullptr;
            switch (cs->args[i].cat) {
                case StyleCat::Pair:
                    map = &pairMap;
                    break;
                case StyleCat::Bond:
                    map = &bondMap;
                    break;
                case StyleCat::Angle:
                    map = &angleMap;
                    break;
                case StyleCat::Dihedral:
                    map = &dihedralMap;
                    break;
                case StyleCat::Improper:
                    map = &improperMap;
                    break;
                case StyleCat::Fix:
                    map = &fixMap;
                    break;
                case StyleCat::Compute:
                    map = &computeMap;
                    break;
                default:
                    break;
            }
            if (map && map->contains(styleword)) {
                page = map->value(styleword);
                help = QString("%1 %2").arg(cmd, styleword);
                return;
            }
            break; // only the first style position selects a page
        }
    }

    // fall back to the command page
    help = cmd;
    page = cmdMap.value(cmd, QString());
}

void CodeEditor::openHelp()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (docver.isEmpty()) setDocver();
    QDesktopServices::openUrl(
        QUrl(QString("%1%2%3").arg(Cfg::DOCS_URL, docver, act->data().toString())));
}

void CodeEditor::openUrl()
{
    auto *act = qobject_cast<QAction *>(sender());
    QDesktopServices::openUrl(QUrl(act->data().toString()));
}

// forward requests to view or inspect files to the corresponding LammpsGui methods

void CodeEditor::viewFile()
{
    auto *act     = qobject_cast<QAction *>(sender());
    auto *guimain = qobject_cast<LammpsGui *>(parent());
    guimain->viewFile(act->data().toString());
}

void CodeEditor::inspectFile()
{
    auto *act     = qobject_cast<QAction *>(sender());
    auto *guimain = qobject_cast<LammpsGui *>(parent());
    guimain->inspectFile(act->data().toString());
}

// Local Variables:
// c-basic-offset: 4
// End:
