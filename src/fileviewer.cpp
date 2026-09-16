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

#include "fileviewer.h"

#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"

#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QResizeEvent>
#include <QString>
#include <QStringList>
#include <QTextCursor>
#include <QTextStream>

FileViewer::FileViewer(const QString &_filename, LammpsGui *_lammpsgui, const QString &title,
                       QWidget *parent) :
    QPlainTextEdit(parent), fileName(_filename), lammpsgui(_lammpsgui)
{
    createMenuBar();
    // no menu entry of its own
    addShortcut(this, QKeySequence(Qt::CTRL | Qt::Key_Slash), this, &FileViewer::stopRun);

    // open and read file. Set editor to read-only.
    QFile file(fileName);
    QFileInfo finfo(file);
    QString content;
    QProcess decomp;
    QStringList args = {"-cdf", fileName};

    // lookup table mapping file extensions to decompression programs and extra args
    struct CompressionFormat {
        const char *extension;
        const char *program;
        const char *extraArg; // nullptr if none
    };
    static constexpr CompressionFormat compressionFormats[] = {
        {"gz", "gzip", nullptr}, {"bz2", "bzip2", nullptr},       {"zst", "zstd", nullptr},
        {"xz", "xz", nullptr},   {"lzma", "xz", "--format=lzma"}, {"lz4", "lz4", nullptr},
    };

    // match suffix with decompression program
    QString command;
    bool compressed = false;
    for (const auto &fmt : compressionFormats) {
        if (finfo.suffix() == fmt.extension) {
            command    = fmt.program;
            compressed = true;
            if (fmt.extraArg) args.insert(1, fmt.extraArg);
            break;
        }
    }

    // read compressed file from pipe
    if (compressed) {
        decomp.start(command, args, QIODevice::ReadOnly);
        if (decomp.waitForStarted()) {
            while (decomp.waitForReadyRead())
                content += decomp.readAll();
        } else {
            content = "\nCould not open compressed file %1 with decompression program %2\n";
            content = content.arg(fileName).arg(command);
        }
        decomp.close();
    } else if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
        // read plain text
        QTextStream in(&file);
        content = in.readAll();
        file.close();
    } else {
        // report the failure in the viewer instead of showing an empty window
        content = QString("\nCould not open file %1: %2\n").arg(fileName, file.errorString());
    }

    document()->setDefaultFont(monoFontFromSettings());

    document()->setPlainText(content);
    moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
    setReadOnly(true);
    setLineWrapMode(NoWrap);
    // a docked panel is sized by its dock area; this would be a floor under it
    if (!dockedLayout()) setMinimumSize(800, 500);
    setWindowIcon(QIcon(Cfg::MAIN_ICON));
    if (title.isEmpty())
        setWindowTitle("LAMMPS-GUI - Viewer - " + fileName);
    else
        setWindowTitle(title);

    applyWindowFlags(this);
}

// A QPlainTextEdit is its own window here, so the menu bar is a child widget
// sitting in reserved viewport margin rather than a window menu bar -- the same
// arrangement CodeEditor uses for its line number area.
void FileViewer::createMenuBar()
{
    menubar    = new QMenuBar(this);
    auto *file = new QMenu("&File", menubar);
    file->setObjectName(Cfg::VIEW_FILE_MENU);

    scopeShortcut(this,
                  addMenuAction(file, "&Close", ":/icons/window-close.svg", this, &QWidget::close),
                  QKeySequence(Qt::CTRL | Qt::Key_W));
    auto *quitAct =
        addMenuAction(file, "&Quit", ":/icons/application-exit.svg", this, &FileViewer::quit);
    scopeShortcut(this, quitAct, QKeySequence(Qt::CTRL | Qt::Key_Q));
    // without a main window there is nothing to quit; closing is all there is
    if (!lammpsgui) quitAct->setVisible(false);

    if (dockedLayout()) {
        // the main window shows this menu for us while the panel has the focus
        retireViewMenuBar(menubar);
        return;
    }
    menubar->addMenu(file);
    if (lammpsgui)
        for (auto *shared : lammpsgui->sharedMenus())
            menubar->addMenu(shared);
    setViewportMargins(0, menubar->sizeHint().height(), 0, 0);
}

void FileViewer::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    if (!menubar || menubar->isHidden()) return;
    const QRect cr = contentsRect();
    menubar->setGeometry(cr.left(), cr.top(), cr.width(), menubar->sizeHint().height());
}

// Docked, this widget inherits the main window's proportional font and
// QPlainTextEdit adopts it as the document font; see reassertMonoFont().
void FileViewer::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) reassertMonoFont(document());
}

void FileViewer::quit()
{
    if (lammpsgui) lammpsgui->quit();
}

void FileViewer::stopRun()
{
    if (lammpsgui) lammpsgui->stopRun();
}

// Local Variables:
// c-basic-offset: 4
// End:
