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

#include "logwindow.h"

#include "constants.h"
#include "flagwarnings.h"
#include "helpers.h"
#include "lammpsgui.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QSpacerItem>
#include <QString>
#include <QTextStream>

namespace {
constexpr auto YAML_REGEX = R"(^(keywords:.*$|data:$|---$|\.\.\.$|  - \[.*\]$))";
QRegularExpression is_yaml(YAML_REGEX, QRegularExpression::MultilineOption);
} // namespace

LogWindow::LogWindow(const QString &_filename, LammpsGui *_lammpsgui, QWidget *parent) :
    QPlainTextEdit(parent), filename(_filename), lammpsgui(_lammpsgui), warnings(nullptr)
{
    QSettings settings;
    // in a docked layout the dock area decides the size, and the remembered
    // one belongs to a free-floating window, so it is neither read nor written
    if (!dockedLayout())
        resize(settings.value(Keys::LOGX, 500).toInt(), settings.value(Keys::LOGY, 320).toInt());

    document()->setDefaultFont(monoFontFromSettings());

    summary = new QLabel(FlagWarnings::summaryText(0, 0));
    summary->setMargin(1);

    auto *frame = new QFrame;
    frame->setAutoFillBackground(true);
    frame->setFrameStyle(QFrame::Box | QFrame::Plain);
    frame->setLineWidth(2);

    auto *button = new QPushButton(QIcon(":/icons/warning.svg"), "");
    button->setToolTip("Jump to next warning");
    connect(button, &QPushButton::released, this, &LogWindow::nextWarning);

    auto *spacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    auto *panel  = new QHBoxLayout(frame);
    auto *grid   = new QGridLayout(this);

    panel->addWidget(summary);
    panel->addWidget(button);
    panel->setStretchFactor(summary, 10);
    panel->setStretchFactor(button, 1);

    grid->addItem(spacer, 0, 0, 1, 3);
    grid->addWidget(frame, 1, 1, 1, 1);
    grid->setColumnStretch(0, 5);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 5);

    warnings = new FlagWarnings(summary, document());

    createActions();
    createMenuBar();
    applyWindowFlags(this);
}

// Every shortcut of this window is bound exactly once, on an action owned by
// the widget and scoped to it with Qt::WidgetWithChildrenShortcut. That keeps
// the binding alive whether or not the context menu is open (it used to live
// on the menu, which exists only while the menu is up, so Ctrl+W had to be
// caught by an event filter instead), and it keeps the shortcut from reaching
// past this widget: several of these sequences -- Ctrl+S, Ctrl+N, Ctrl+/,
// Ctrl+Return -- are also main window accelerators, and once this window is
// docked into the main window rather than being a window of its own, a
// window-scoped binding here would be an ambiguous overload of that one.
void LogWindow::createActions()
{
    // the slot parameter is generic so that inherited members such as
    // QWidget::close() can be connected as well as this window's own slots
    auto add = [this](QAction *&act, const QString &text, const QString &icon,
                      const QKeySequence &keys, auto slot) {
        act = new QAction(QIcon(icon), text, this);
        scopeShortcut(this, act, keys);
        connect(act, &QAction::triggered, this, slot);
    };

    // menu texts follow the shared convention of the other view File menus
    add(saveAsAct, "&Save Log to File ...", ":/icons/document-save-as.svg",
        QKeySequence(Qt::CTRL | Qt::Key_S), &LogWindow::saveAs);
    add(yamlAct, "&Export YAML Data to File ...", ":/icons/yaml-file-icon.svg",
        QKeySequence(Qt::CTRL | Qt::Key_Y), &LogWindow::extractYaml);
    add(nextWarnAct, "&Jump to next warning or error", ":/icons/warning.svg",
        QKeySequence(Qt::CTRL | Qt::Key_N), &LogWindow::nextWarning);
    add(closeAct, "&Close", ":/icons/window-close.svg", QKeySequence(Qt::CTRL | Qt::Key_W),
        &LogWindow::close);
    add(quitAct, "&Quit", ":/icons/application-exit.svg", QKeySequence(Qt::CTRL | Qt::Key_Q),
        &LogWindow::quit);

    // only shown when the cursor sits on a line with an error URL
    add(urlAct, "Open &URL in Web Browser", ":/icons/help-browser.svg", QKeySequence(),
        &LogWindow::openErrorUrl);

    // These two have no menu entry.  They stay plain shortcuts rather than
    // hidden actions because Qt disables the shortcut of an invisible action.
    addShortcut(this, QKeySequence(Qt::CTRL | Qt::Key_Slash), this, &LogWindow::stopRun);
    addShortcut(this, QKeySequence(Qt::CTRL | Qt::Key_Return), this, &LogWindow::runBuffer);
}

// The window is a QPlainTextEdit, so its menu bar is a child widget in reserved
// viewport margin rather than a window menu bar -- the same arrangement
// CodeEditor uses for its line number area.  The entries are the actions the
// context menu shows, so there is one object per command either way.
void LogWindow::createMenuBar()
{
    menubar    = new QMenuBar(this);
    auto *file = new QMenu("&File", menubar);
    file->setObjectName(Cfg::VIEW_FILE_MENU);
    file->addAction(saveAsAct);
    file->addAction(yamlAct);
    file->addAction(nextWarnAct);
    file->addSeparator();
    file->addAction(closeAct);
    file->addAction(quitAct);

    if (installViewMenuBar(menubar, file, lammpsgui ? lammpsgui->sharedMenus() : QList<QMenu *>()))
        setViewportMargins(0, menubar->sizeHint().height(), 0, 0);
}

void LogWindow::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    layoutViewMenuBar(this, menubar);
}

// warnings and summary are Qt-parented and cleaned up by their parents
LogWindow::~LogWindow() = default;

void LogWindow::reset(const QString &_filename)
{
    filename = _filename;
    // clear() rehighlights the now empty document, so the counters must be
    // cleared afterwards to not carry the previous run's totals into the new one
    clear();
    if (warnings) warnings->reset();
}

void LogWindow::closeEvent(QCloseEvent *event)
{
    if (!isMaximized() && !dockedLayout()) {
        QSettings settings;
        settings.setValue(Keys::LOGX, width());
        settings.setValue(Keys::LOGY, height());
    }
    QPlainTextEdit::closeEvent(event);
}

// Docked, this widget inherits the main window's proportional font and
// QPlainTextEdit adopts it as the document font; see reassertMonoFont().
void LogWindow::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) reassertMonoFont(document());
}

void LogWindow::quit()
{
    if (lammpsgui) lammpsgui->quit();
}

void LogWindow::stopRun()
{
    if (lammpsgui) lammpsgui->stopRun();
}

void LogWindow::runBuffer()
{
    if (lammpsgui) lammpsgui->runBuffer();
}

void LogWindow::nextWarning()
{
    // the highlighter's own notion of a warning, so the search finds exactly
    // what is highlighted and counted
    const QRegularExpression &regex = FlagWarnings::warningPattern();

    if (warnings->getNWarnings() > 0) {
        // wrap around search
        if (!find(regex)) {
            moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
            find(regex);
        }
        // move cursor to unselect
        moveCursor(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
}

void LogWindow::saveAs()
{
    const QString defaultname =
        QDir::current().absoluteFilePath(defaultFileStem(filename) + ".log");
    QString logFileName =
        QFileDialog::getSaveFileName(this, "Save Log to File", defaultname, Cfg::FILTER_LOG);
    if (logFileName.isEmpty()) return;
    logFileName = ensureFileSuffix(logFileName, "log");

    QFileInfo path(logFileName);
    QFile file(path.absoluteFilePath());

    if (!file.open(QIODevice::WriteOnly | QFile::Text)) {
        warning(this, "LogWindow Warning", "Cannot save to file " + logFileName + ":",
                file.errorString());
        return;
    }

    QTextStream out(&file);
    QString text = toPlainText();
    out << text;
    if (!text.endsWith('\n')) out << "\n"; // add final newline if missing
    file.close();
}

bool LogWindow::checkYaml()
{
    return document()->find(is_yaml).isNull() == false;
}

void LogWindow::extractYaml()
{
    // ignore if no YAML format lines in buffer
    if (!checkYaml()) return;

    const QString defaultname =
        QDir::current().absoluteFilePath(defaultFileStem(filename) + ".yaml");
    QString yamlFileName =
        QFileDialog::getSaveFileName(this, "Save YAML data to File", defaultname, Cfg::FILTER_YAML);
    // cannot save without filename
    if (yamlFileName.isEmpty()) return;
    yamlFileName = ensureFileSuffix(yamlFileName, "yaml");

    QFileInfo path(yamlFileName);
    QFile file(path.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QFile::Text)) {
        warning(this, "LogWindow Warning", "Cannot save to file " + yamlFileName + ":",
                file.errorString());
        return;
    }

    QTextStream out(&file);
    for (auto block = document()->begin(); block != document()->end(); block = block.next()) {
        auto line = block.text();
        if (is_yaml.match(line).hasMatch()) out << line << '\n';
    }
    file.close();
}

void LogWindow::openErrorUrl()
{
    if (!errorurl.isEmpty()) QDesktopServices::openUrl(QUrl(errorurl));
}

void LogWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
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

        auto text = cursor.selectedText();
        auto url  = FlagWarnings::errorUrlPattern().match(text);
        if (url.hasMatch()) {
            errorurl = url.captured(1);
            if (!errorurl.isEmpty()) {
                QDesktopServices::openUrl(QUrl(errorurl));
                return;
            }
        }
    }
    // forward event to parent class for all unhandled cases
    QPlainTextEdit::mouseDoubleClickEvent(event);
}

void LogWindow::contextMenuEvent(QContextMenuEvent *event)
{
    // reposition the cursor here, but only if there is no active selection
    if (!textCursor().hasSelection()) setTextCursor(cursorForPosition(event->pos()));

    // show augmented context menu; the entries are the window's own actions, so
    // their shortcuts keep working after the menu is gone
    auto *menu = createStandardContextMenu();
    menu->addSeparator();
    menu->addAction(saveAsAct);
    // only show export-to-yaml entry if there is YAML format content.
    if (checkYaml()) menu->addAction(yamlAct);

    // process line of text where the cursor is
    auto text = textCursor().block().text().replace('\t', ' ').trimmed();
    auto url  = FlagWarnings::errorUrlPattern().match(text);
    if (url.hasMatch()) {
        errorurl = url.captured(1);
        menu->addAction(urlAct);
    }
    menu->addAction(nextWarnAct);
    menu->addSeparator();
    menu->addAction(closeAct);
    menu->addAction(quitAct);
    menu->exec(event->globalPos());
    delete menu;
}

// Local Variables:
// c-basic-offset: 4
// End:
