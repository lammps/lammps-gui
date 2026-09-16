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

#include "commandwindow.h"

#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "shellaliases.h"
#include "shellprompt.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#if !defined(Q_OS_WIN32)
#include <csignal>
#include <unistd.h>
#endif

namespace {

// A pipe is a stream with no record of where one command's output ends, so each
// command is followed by a line that says so.  It carries the exit status and
// the working directory as well, which is how the prompt learns about cd,
// pushd/popd, or a directory change made inside a sourced script -- parsing the
// typed line would miss all of those.
//
// This is only the start of the mark: startShell() completes it with a suffix
// unique to the session.  A fixed string could be printed by a command -- a
// grep over this very source file, say -- and would pass for the real thing,
// which frees the prompt while the command is still running.
constexpr auto SENTINEL = "__LGUI_DONE_";

// The commands the panel adds are the shell's half of a viewer: each prints one
// of these per file and the panel picks them out of the stream, the way it picks
// out the sentinel.  Letting the shell say which files are meant is the whole
// point -- globs, braces, quoting, ~ and $VAR then expand exactly as they would
// for any other command, and they work in a pipeline or a loop like any other.
//
// The line reads "<mark><what>_<file>", and what follows the first underscore is
// the file, spaces and all, exactly as with the sentinel -- whose session suffix
// this start of a mark is completed with as well.
constexpr auto OPENMARK = "__LGUI_OPEN_";

// The commands themselves, and the word each puts in its marker.  None of them
// is called "view": that name belongs to vim's read-only mode on most Unix
// systems, and taking it would mean never being defined at all.
struct Viewer {
    const char *name; ///< command the shell gets
    const char *what; ///< what the panel should do with the files it names
};
constexpr Viewer VIEWERS[] = {{"open", "show"}, {"edit", "edit"}, {"plot", "plot"}};

// Each of them is defined under a second name as well, this one prefixed, which
// is what is left when the short one turns out to be taken.  It is longer and
// nothing else is likely to be called that, which is the whole point: unlike the
// short names it is defined whatever else is on the system, so there is always a
// way to reach the viewers.
constexpr auto VIEWERPREFIX = "gui-";

// Shells do not agree on how to name the exit status, the working directory or
// the prompt, so the few things this window has to say to one are chosen by
// family rather than assumed to be POSIX.
enum class ShellKind { Posix, Zsh, Csh, Cmd };

ShellKind shellKind(const QString &program)
{
    const QString name = QFileInfo(program).fileName().toLower();
#if defined(Q_OS_WIN32)
    if (name.startsWith("cmd")) return ShellKind::Cmd;
#endif
    // csh and tcsh; no other common shell name ends in "csh"
    if (name.endsWith("csh")) return ShellKind::Csh;
    if (name.endsWith("zsh")) return ShellKind::Zsh;
    return ShellKind::Posix;
}

QStringList shellArgs(const QString &program)
{
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            return {};
        case ShellKind::Csh:
        case ShellKind::Zsh:
            // neither runs its line editor when what it reads is not a terminal,
            // so unlike bash they need no argument to keep it out of the way
            return {"-i"};
        default: {
            QStringList args;
            // keep bash from running its line editor over input that is a pipe,
            // which would echo every line back wrapped in terminal escapes
            if (QFileInfo(program).fileName().contains("bash")) args << "--noediting";
            args << "-i";
            return args;
        }
    }
}

// Silence the shell's own prompt -- this window supplies one -- and whatever
// else an interactive shell does that suits a terminal and not a pipe.
QString shellInit(const QString &program)
{
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            // cmd.exe otherwise echoes every line it is fed
            return QStringLiteral("@echo off");
        case ShellKind::Csh:
            // "unset edit" is what stops csh echoing the line back
            return QStringLiteral("set prompt = \"\" ; set prompt2 = \"\" ; unset edit");
        case ShellKind::Zsh:
            // zsh does not take "set +H" for history expansion, and it has its
            // own places to print from: a prompt on the right, a mark where
            // output did not end in a newline, and the precmd/preexec hooks a
            // theme uses instead of PROMPT_COMMAND
            return QStringLiteral("PS1='' ; PS2='' ; RPROMPT='' ; PROMPT_EOL_MARK='' ;"
                                  " precmd() { : } ; preexec() { : } ;"
                                  " unsetopt banghist 2>/dev/null");
        default:
            // PROMPT_COMMAND is where a distribution hides the escape sequence
            // that sets a terminal's title; history expansion would turn a "!"
            // in an ordinary command line into an error
            return QStringLiteral("PS1='' ; PS2='' ; unset PROMPT_COMMAND 2>/dev/null ;"
                                  " set +H 2>/dev/null");
    }
}

// COLUMNS and LINES are where a program looks for the size of its output when it
// cannot ask a terminal for one.  Nothing about them needs a terminal, but they
// are normally set by one, so without this everything that formats to a width
// falls back to its built-in 80 columns however wide the panel is.
QString sizeCommand(const QString &program, int cols, int rows)
{
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            // cmd.exe takes the size from the console it is attached to
            return {};
        case ShellKind::Csh:
            return QStringLiteral("setenv COLUMNS %1 ; setenv LINES %2").arg(cols).arg(rows);
        default:
            return QStringLiteral("export COLUMNS=%1 LINES=%2").arg(cols).arg(rows);
    }
}

// Quote a definition for the shell that will read it: everything inside single
// quotes is literal, and the only thing that cannot appear there is a single
// quote, which is closed, escaped and reopened in the usual way.
QString singleQuoted(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1String("'"), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

// csh takes the name and the body as two words, everything else writes an
// assignment; cmd.exe has no aliases at all, only doskey macros, which behave
// differently enough not to pretend otherwise.
QString aliasCommand(const QString &program, const ShellAlias &alias)
{
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            return {};
        case ShellKind::Csh:
            return QStringLiteral("alias %1 %2").arg(alias.first, singleQuoted(alias.second));
        default:
            return QStringLiteral("alias %1=%2").arg(alias.first, singleQuoted(alias.second));
    }
}

// Define one of the panel's commands in the shell, under its short name only
// when that name is free.  The test is left to the shell on purpose: "command
// -v" answers for aliases, functions and builtins as well as for $PATH, in the
// environment the user actually has, which nothing on this side could do as
// well.  macOS has an "open" of its own that does much the same job and GNU
// plotutils installs a "plot", so this is not a formality -- where the name is
// taken the command that was there keeps working and only the prefixed name is
// added, which is defined either way.
QStringList viewerCommands(const QString &program, const Viewer &viewer, const QString &mark)
{
    const QString name     = QString::fromLatin1(viewer.name);
    const QString what     = QString::fromLatin1(viewer.what);
    const QString prefixed = QLatin1String(VIEWERPREFIX) + name;
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            // no functions, and doskey macros do not behave enough like one
            return {};
        case ShellKind::Csh: {
            // csh has aliases rather than functions, and an alias body is one
            // line, so there is no loop to be had here -- but none is needed:
            // printf repeats its format until the arguments run out.  \!* is
            // how a csh alias passes them on, and "which" is its "command -v".
            const QString body = QStringLiteral("'printf \"%1%2_%s\\n\" \\!*'").arg(mark, what);
            return {QStringLiteral("alias %1 %2").arg(prefixed, body),
                    QStringLiteral("which %1 >& /dev/null || alias %1 %2").arg(name, body)};
        }
        default: {
            // What the commands do is written once, in a function of its own,
            // because the prefixed name has to be given to the shell as an alias
            // rather than as a function: a "-" in a function name is something
            // bash and zsh accept and dash rejects outright, while all of them
            // take it in an alias.  The short name stays a function, so that it
            // is what it always was where it is defined at all.
            const QString helper = QStringLiteral("__lgui_") + what;
            return {QStringLiteral("%1() { if [ \"$#\" -gt 0 ]; then "
                                   "printf '%2%3_%s\\n' \"$@\"; fi; }")
                        .arg(helper, mark, what),
                    QStringLiteral("alias %1='%2'").arg(prefixed, helper),
                    QStringLiteral("command -v %1 >/dev/null 2>&1 || %1() { %2 \"$@\"; }")
                        .arg(name, helper)};
        }
    }
}

QString sentinelCommand(const QString &program, const QString &mark)
{
    switch (shellKind(program)) {
        case ShellKind::Cmd:
            return QStringLiteral("echo %1%%errorlevel%%_%%CD%%").arg(mark);
        case ShellKind::Csh:
            // echo is a builtin and sets a status of its own, so the one being
            // reported has to be put aside before the first of them runs
            return QStringLiteral("set _lgstatus = $status ; echo \"\" ;"
                                  " echo \"%1${_lgstatus}_${cwd}\"")
                .arg(mark);
        default:
            // the leading newline puts the sentinel on a line of its own even
            // when the command's output did not end with one; PWD last so a
            // path with spaces in it survives being read to the end of the line
            return QStringLiteral("printf '\\n%1%s_%s\\n' \"$?\" \"$PWD\"").arg(mark);
    }
}

// The search path for the shell and for the completion list, which must agree
// on it.  On macOS an app bundle launched from the Finder inherits a minimal
// PATH without the package manager locations, so those are appended -- the same
// ones findExe() falls back to.
QString shellSearchPath()
{
    QString path = qEnvironmentVariable("PATH");
#if defined(Q_OS_MACOS)
    const QStringList parts = path.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const auto &dir : {QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/usr/local/bin"),
                            QStringLiteral("/opt/local/bin")}) {
        if (!parts.contains(dir) && QFileInfo::exists(dir)) path += QDir::listSeparator() + dir;
    }
#endif
    return path;
}

// A shell prompt writes the home directory as "~", which is shorter and is how
// the path is usually written down anyway.  Only what is shown is shortened: the
// directory the panel tracks stays the one the shell reported, because that is
// the one that has to reach the file system.
QString abbreviateHome(const QString &dir)
{
#if defined(Q_OS_WIN32)
    // cmd.exe does not know "~", so a prompt that showed it would be offering a
    // path that cannot be typed back
    return dir;
#else
    const QString home = QDir::homePath();
    if (home.isEmpty()) return dir;
    if (dir == home) return QStringLiteral("~");
    if (dir.startsWith(home + QLatin1Char('/'))) return QStringLiteral("~") + dir.mid(home.size());
    return dir;
#endif
}

// An interactive shell without a terminal complains whenever it would otherwise
// hand one to a job -- when a command ends, and loudly when one is killed.  The
// message says nothing about the command and there is no terminal to be had, so
// it is dropped rather than shown after every job.
bool isShellJobControlNoise(const QString &line)
{
    return line.contains("Inappropriate ioctl for device") ||
           line.contains("no job control in this shell");
}

// Where the word being typed starts: after the last blank, or after a separator
// that ends one command and begins another.  Both the matching and the command/
// argument decision hang off this, so they cannot disagree about which word the
// cursor is in.
int wordStart(const QString &text)
{
    int i = text.size();
    while (i > 0) {
        const QChar c = text.at(i - 1);
        if (c.isSpace() || (c == u';') || (c == u'|') || (c == u'&') || (c == u'(')) break;
        --i;
    }
    return i;
}

// A word is a command rather than an argument when nothing but a separator
// stands in front of it -- so the word after "a ; b", "a | b" or "a && b" is
// completed like the first word of the line, which is what it is.
bool inCommandPosition(const QString &text)
{
    int i = wordStart(text);
    while ((i > 0) && text.at(i - 1).isSpace())
        --i;
    if (i == 0) return true;
    const QChar prev = text.at(i - 1);
    return (prev == u';') || (prev == u'|') || (prev == u'&') || (prev == u'(');
}

// QLineEdit hands its completer the whole line.  That is right for the first
// word -- a command name, or a line typed before, both of which are matched
// whole -- and wrong for everything after it, where what is being typed is a
// single argument.  Narrow both ends to the word the cursor is in: what gets
// matched, and what gets replaced when a completion is taken.
class WordCompleter : public QCompleter {
public:
    explicit WordCompleter(QObject *parent) : QCompleter(parent) {}

    QStringList splitPath(const QString &path) const override
    {
        return {path.mid(wordStart(path))};
    }

    QString pathFromIndex(const QModelIndex &index) const override
    {
        // the prefix is the whole line, which is where the part in front of the
        // word being completed has to come from
        const QString word = QCompleter::pathFromIndex(index);
        const QString line = completionPrefix();
        return line.left(wordStart(line)) + word;
    }
};

} // namespace

QString CommandWindow::preferredShell()
{
    QSettings settings;
    const QString configured = settings.value(Keys::SHELL, QString()).toString();
    if (!configured.isEmpty()) return configured;

#if defined(Q_OS_WIN32)
    const QString comspec = qEnvironmentVariable("COMSPEC");
    return comspec.isEmpty() ? QStringLiteral("cmd.exe") : comspec;
#else
    const QString shell = qEnvironmentVariable("SHELL");
    if (!shell.isEmpty()) return shell;
    // pushd/popd are not POSIX, so prefer a shell that has them
    if (QFileInfo::exists("/bin/bash")) return QStringLiteral("/bin/bash");
    return QStringLiteral("/bin/sh");
#endif
}

QStringList CommandWindow::availableShells()
{
    QStringList shells;
    // The same shell is usually listed more than once -- /etc/shells names it
    // under /bin and /usr/bin, which are one directory on current systems, and
    // $SHELL may spell it either way.  Each *name* is offered once, and the
    // first path to claim it wins.
    QSet<QString> seen;
    const auto addShell = [&shells, &seen](const QString &path) {
        QString base = QFileInfo(path).fileName();
#if defined(Q_OS_WIN32)
        base = base.toLower();
#endif
        if (base.isEmpty() || seen.contains(base)) return;
        if (!QFileInfo::exists(path)) return;
        seen.insert(base);
        shells << path;
    };

#if defined(Q_OS_WIN32)
    addShell(qEnvironmentVariable("COMSPEC")); // cmd.exe
    // PowerShell (7 as pwsh.exe, the bundled one as powershell.exe) and any
    // bash on the search path
    for (const auto &name :
         {QStringLiteral("pwsh.exe"), QStringLiteral("powershell.exe"), QStringLiteral("bash.exe")})
        addShell(QStandardPaths::findExecutable(name));
    // Git for Windows carries a bash.exe but does not put it on the path
    for (const auto &guess : {QStringLiteral("C:/Program Files/Git/bin/bash.exe"),
                              QStringLiteral("C:/Program Files (x86)/Git/bin/bash.exe")})
        addShell(guess);
#else
    // The user's own shell goes first, under the exact spelling $SHELL uses:
    // it is the default selection, so it is the name its duplicates yield to.
    // It may also be missing from /etc/shells altogether (e.g. from a package
    // that did not register it).
    addShell(qEnvironmentVariable("SHELL"));
    // the administrative list of login shells; entries that are there to *deny*
    // a login are not shells, and neither are entries that do not exist
    QFile file(QStringLiteral("/etc/shells"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!file.atEnd()) {
            const QString line = QString::fromLocal8Bit(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(u'#') || line.contains("nologin")) continue;
            // the list also admits terminal multiplexers as login shells, and
            // those need the one thing this window does not have: a terminal
            const QString base = QFileInfo(line).fileName();
            if ((base == QLatin1String("tmux")) || (base == QLatin1String("screen"))) continue;
            addShell(line);
        }
    }
#endif
    if (shells.isEmpty()) shells << preferredShell();
    shells.sort();
    return shells;
}

CommandWindow::CommandWindow(LammpsGui *_lammpsgui, QWidget *parent) :
    QWidget(parent), lammpsgui(_lammpsgui), scrollback(new QPlainTextEdit), prompt(new ShellPrompt),
    cwdlabel(new QLabel), completer(new WordCompleter(this)), commands(new QStringListModel(this)),
    filenames(new QStringListModel(this)), workingdir(QDir::currentPath())
{
    scrollback->setReadOnly(true);
    scrollback->setLineWrapMode(QPlainTextEdit::NoWrap);
    scrollback->setMaximumBlockCount(Cfg::COMMAND_SCROLLBACK_LINES);
    // set on the widget, not only on the document: docked, this becomes a child
    // of the main window and would otherwise inherit its proportional font,
    // which QPlainTextEdit then adopts for the document as well
    scrollback->setFont(monoFontFromSettings());

    prompt->setFont(monoFontFromSettings());
    prompt->setPlaceholderText("enter a command");
    prompt->setToolTip("Commands run in the foreground and hold the prompt until they\n"
                       "finish, as they would in a terminal.  Start a graphical or\n"
                       "long-running program with a trailing \"&\" to keep the prompt\n"
                       "free -- there is no job control here to background it after\n"
                       "the fact with Ctrl-Z.");
    // whoever hands the focus to this window -- the dock raising its tab, the
    // View menu, a click on a part of it that takes no focus itself -- means the
    // input line, not the container
    setFocusProxy(prompt);
    prompt->installEventFilter(this);
    scrollback->installEventFilter(this);
    scrollback->viewport()->installEventFilter(this);
    connect(prompt, &QLineEdit::returnPressed, this, &CommandWindow::submit);
    connect(prompt, &QLineEdit::textEdited, this, &CommandWindow::updateCompleter);
    // Tab completion starts from a line that was not necessarily typed -- a line
    // recalled from the history is set, not edited, so textEdited says nothing
    // about it -- and the list to complete from has to be chosen for it too
    connect(prompt, &ShellPrompt::completing, this, &CommandWindow::updateCompleter);

    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseSensitive);
    completer->setModel(commands);
    prompt->setCompleter(completer);

    cwdlabel->setFont(monoFontFromSettings());
    cwdlabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // the directory gives way to the input line rather than the other way round;
    // what bounds it is the elision in updatePrompt(), so its size hint stays
    // honest and it does not collapse to nothing
    prompt->setMinimumWidth(Cfg::COMMAND_MIN_PROMPT_WIDTH);

    killbutton = new QPushButton(QIcon(":/icons/skull.svg"), "");
    killbutton->setToolTip("Kill the running command");
    killbutton->setEnabled(false);
    styleToolButtons(toolButtonSize(killbutton), {killbutton});
    connect(killbutton, &QPushButton::released, this, &CommandWindow::killCommand);

    auto *promptrow = new QHBoxLayout;
    promptrow->addWidget(cwdlabel);
    promptrow->addWidget(prompt, 10);
    promptrow->addWidget(killbutton);

    auto *top = new QVBoxLayout;
    top->addWidget(scrollback, 10);
    top->addLayout(promptrow);
    createMenuBar();
    top->setMenuBar(menubar);
    setLayout(top);

    QSettings settings;
    history    = settings.value(Keys::CMDHISTORY).toStringList();
    historypos = history.size();

    applyWindowFlags(this);
    resize(Cfg::COMMAND_DEFAULT_WIDTH, Cfg::COMMAND_DEFAULT_HEIGHT);
    startShell();
}

CommandWindow::~CommandWindow()
{
    QSettings settings;
    // keep the tail; a history file that grows without bound helps nobody
    while (history.size() > Cfg::COMMAND_HISTORY_MAX)
        history.removeFirst();
    settings.setValue(Keys::CMDHISTORY, history);

    if (shell && shell->state() != QProcess::NotRunning) {
        shell->closeWriteChannel();
        if (!shell->waitForFinished(Cfg::COMMAND_EXIT_TIMEOUT)) {
            shell->kill();
            // reap it, or the QProcess child is destroyed with it still running
            shell->waitForFinished(Cfg::COMMAND_EXIT_TIMEOUT);
        }
    }
}

void CommandWindow::createMenuBar()
{
    menubar    = new QMenuBar;
    auto *file = new QMenu("&File", menubar);
    file->setObjectName(Cfg::VIEW_FILE_MENU);

    addMenuAction(file, "&Interrupt Command", ":/icons/process-stop.svg", this,
                  &CommandWindow::interrupt);
    addMenuAction(file, "&Kill Command", ":/icons/skull.svg", this, &CommandWindow::killCommand);
    addMenuAction(file, "&Restart Shell", ":/icons/system-restart.svg", this,
                  &CommandWindow::restartShell);
    addMenuAction(file, "C&lear Output", ":/icons/edit-delete.svg", this,
                  &CommandWindow::clearScrollback);
    addMenuAction(file, "Change to Input &Directory", ":/icons/document-open.svg", this,
                  &CommandWindow::changeToInputDirectory);
    file->addSeparator();
    addMenuAction(file, "Command &Aliases...", ":/icons/preferences-desktop.svg", this,
                  &CommandWindow::editAliases);
    file->addSeparator();
    scopeShortcut(this,
                  addMenuAction(file, "&Close", ":/icons/window-close.svg", this, &QWidget::close),
                  QKeySequence(Qt::CTRL | Qt::Key_W));
    auto *quitAct =
        addMenuAction(file, "&Quit", ":/icons/application-exit.svg", this, &CommandWindow::quit);
    scopeShortcut(this, quitAct, QKeySequence(Qt::CTRL | Qt::Key_Q));
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
}

void CommandWindow::startShell()
{
    if (shell) {
        // stop it reporting its own death: we are the ones ending it
        shell->disconnect(this);
        // and end it before it is deleted: QProcess objects to being destroyed
        // while its process is still running, and this one is
        shell->kill();
        shell->waitForFinished(Cfg::COMMAND_EXIT_TIMEOUT);
        delete shell;
    }
    // Nothing known about the old shell holds for the new one: it has to be
    // told the panel size again, and a partial line or a file reported by a
    // command the old shell was still running must not be taken for its own.
    running     = false;
    priming     = false;
    termcols    = 0;
    termrows    = 0;
    sizepending = true;
    pending.clear();
    pendingopen.clear();
    pendingsetup.clear();
    shell = new QProcess(this);
    // one stream, so what the command wrote to stderr appears where it happened
    shell->setProcessChannelMode(QProcess::MergedChannels);
    shell->setWorkingDirectory(workingdir);

    auto env = QProcessEnvironment::systemEnvironment();
    // Not a terminal: say so, so that a program wanting more than a stream of
    // bytes reports that its terminal is insufficient instead of writing escape
    // sequences into a scrollback that cannot interpret them.
    env.insert("TERM", "dumb");
    // a no-op everywhere but on a Finder-launched macOS app; see shellSearchPath()
    env.insert("PATH", shellSearchPath());
    // a child of the shell block-buffers when it is not on a tty, which would
    // hold a script's output back until it exits
    env.insert("PYTHONUNBUFFERED", "1");
    shell->setProcessEnvironment(env);

    connect(shell, &QProcess::readyReadStandardOutput, this, &CommandWindow::readOutput);
    connect(shell, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &CommandWindow::shellFinished);

    // An interactive shell is what reads the user's rc file, and that is where
    // aliases and shell functions live -- a non-interactive one skips it, and
    // the guard most rc files open with would bail out even if it did not.
    shellprogram          = preferredShell();
    const QString program = shellprogram;
    // marks of this session's own, so that no output can pass for one
    const QString nonce = QString::number(QRandomGenerator::global()->generate(), 16)
                              .rightJustified(8, QLatin1Char('0'));
    sentinel = QLatin1String(SENTINEL) + nonce + QLatin1Char('_');
    openmark = QLatin1String(OPENMARK) + nonce + QLatin1Char('_');
#if !defined(Q_OS_WIN32)
    // put the shell in a session of its own, so a signal can be sent to it and
    // to whatever it is running rather than to this application
    shell->setChildProcessModifier([]() {
        setsid();
    });
#endif
    shell->start(program, shellArgs(program));
    if (!shell->waitForStarted(Cfg::COMMAND_START_TIMEOUT)) {
        appendOutput(QString("Cannot run \"%1\": %2\n").arg(program, shell->errorString()));
        return;
    }

    appendOutput(QString("%1\n").arg(program));

    // Everything the shell says before the first sentinel is its own start-up
    // noise -- the prompt it prints because it is interactive, and its complaint
    // about having no terminal to put a job in the foreground of.
    priming = true;
    shell->write(qPrintable(shellInit(program) + "\n"));
    // define what the start-up file could not, because it is guarded by a test
    // for a terminal, and what a program only formats that way for one
    for (const auto &alias : ShellAliases::aliases()) {
        const QString command = aliasCommand(program, alias);
        if (!command.isEmpty()) shell->write(qPrintable(command + "\n"));
    }
    // give the shell the commands that show a file in this application -- but
    // only when there is an application to show it in; standalone there is
    // nowhere for them to go, and a command that silently does nothing is worse
    // than no command at all
    if (lammpsgui) {
        for (const auto &viewer : VIEWERS) {
            for (const auto &command : viewerCommands(program, viewer, openmark))
                shell->write(qPrintable(command + "\n"));
        }
    }
    // ask where we are, so the prompt is right before anything is typed
    shell->write(qPrintable(sentinelCommand(program, sentinel) + "\n"));
}

void CommandWindow::editAliases()
{
    const auto before = ShellAliases::aliases();
    ShellAliases dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    const auto after = ShellAliases::aliases();
    if (!shell || (shell->state() != QProcess::Running)) return;

    // Apply the change to the shell that is already running, so the table means
    // the same thing whether it was edited before the window was opened or
    // after.  Only what the table itself gave the shell is withdrawn again; an
    // alias that came from the start-up file is left alone.
    QStringList commands;
    for (const auto &alias : before) {
        const auto same = [&alias](const ShellAlias &other) {
            return other.first == alias.first;
        };
        if (std::none_of(after.begin(), after.end(), same))
            commands << QStringLiteral("unalias %1 2>/dev/null").arg(alias.first);
    }
    for (const auto &alias : after) {
        const QString command = aliasCommand(shellprogram, alias);
        if (!command.isEmpty()) commands << command;
    }
    if (commands.isEmpty()) return;

    // a line written now would be read by whatever is running, not by the shell
    if (running || priming) {
        pendingsetup += commands;
        return;
    }
    for (const auto &command : commands)
        shell->write(qPrintable(command + "\n"));
}

void CommandWindow::sendTerminalSize()
{
    if (!shell || (shell->state() != QProcess::Running)) return;

    const QFontMetricsF metrics(scrollback->font());
    const qreal charwidth  = metrics.horizontalAdvance(QLatin1Char('0'));
    const qreal lineheight = metrics.lineSpacing();
    if ((charwidth <= 0.0) || (lineheight <= 0.0)) return;

    const int cols = int(scrollback->viewport()->width() / charwidth);
    const int rows = int(scrollback->viewport()->height() / lineheight);
    if ((cols < Cfg::COMMAND_MIN_COLUMNS) || (rows < 1)) return;
    if ((cols == termcols) && (rows == termrows)) return;

    // a line written now would be read by whatever is running rather than by the
    // shell, so wait for the sentinel that says it is done
    if (running || priming) {
        sizepending = true;
        return;
    }

    // record before the dialect check: cmd.exe has no command to take a size,
    // and without the bookkeeping every sentinel would retry this no-op
    termcols              = cols;
    termrows              = rows;
    sizepending           = false;
    const QString command = sizeCommand(shellprogram, cols, rows);
    if (command.isEmpty()) return;
    shell->write(qPrintable(command + "\n"));
}

void CommandWindow::changeDirectory(const QString &dir)
{
    if (dir.isEmpty() || !shell || shell->state() != QProcess::Running) return;
    // quoted the way the shell reads it: cmd.exe takes double quotes and needs
    // /d to follow the path onto another drive; everywhere else single quotes
    // are what keep a "$", a backtick, or a backslash in the path literal
    const QString command = (shellKind(shellprogram) == ShellKind::Cmd)
                                ? QStringLiteral("cd /d \"%1\"").arg(QDir::toNativeSeparators(dir))
                                : QStringLiteral("cd ") + singleQuoted(dir);
    // a line written now would be read by the running command, not the shell;
    // the queue is flushed when the sentinel says the shell is at a prompt
    if (running) {
        pendingsetup << command << sentinelCommand(shellprogram, sentinel);
        return;
    }
    shell->write(qPrintable(command + "\n"));
    shell->write(qPrintable(sentinelCommand(shellprogram, sentinel) + "\n"));
}

// The shell starts where the input file is and stays independent afterwards: a
// file opened later must not move it out from under whatever it is doing.  This
// is the explicit way back.  The GUI itself follows the input file (it makes
// the file's directory the process working directory), so that is where to ask.
void CommandWindow::changeToInputDirectory()
{
    changeDirectory(QDir::currentPath());
}

void CommandWindow::submit()
{
    // Return still arrives while the line is read-only, so the guard belongs
    // here and not only on the typing: sending anything now would queue it
    // behind the running command and leave the state waiting for a second
    // sentinel that answers nothing.
    if (running) return;

    const QString line = prompt->text();
    if (line.trimmed().isEmpty()) {
        prompt->clear();
        return;
    }
    if (!shell || shell->state() != QProcess::Running) {
        appendOutput("No shell is running. Use File > Restart Shell.\n");
        return;
    }

    // the transcript reads like a session: the prompt, then what came back
    appendOutput(QString("%1$ %2\n").arg(abbreviateHome(workingdir), line));
    prompt->clear();

    if (history.isEmpty() || history.last() != line) {
        history << line;
        // the line just typed is a completion for the next one
        refreshCompletions();
    }
    historypos = history.size();

    running = true;
    updatePrompt();
    shell->write(qPrintable(line + "\n"));
    shell->write(qPrintable(sentinelCommand(shellprogram, sentinel) + "\n"));
}

void CommandWindow::readOutput()
{
    consume(QString::fromLocal8Bit(shell->readAllStandardOutput()));
}

void CommandWindow::consume(const QString &chunk)
{
    pending += chunk;
    pending.replace("\r\n", "\n");

    // only whole lines can be examined for the sentinel
    int nl = pending.indexOf('\n');
    while (nl >= 0) {
        const QString line = pending.left(nl);
        pending.remove(0, nl + 1);

        // "open" reports one file per line; collect them and show them once the
        // command that produced them is done, so that a single "open *.png"
        // becomes one slide show rather than one per file
        const int want = line.indexOf(openmark);
        if (want >= 0) {
            pendingopen << line.mid(want + openmark.size());
            nl = pending.indexOf('\n');
            continue;
        }

        const int mark = line.indexOf(sentinel);
        if (mark >= 0) {
            // "<status>_<directory>"; the status has no underscore in it, so the
            // first one separates them and the rest is the path, spaces and all
            const QString tail  = line.mid(mark + sentinel.size());
            const int sep       = tail.indexOf('_');
            const bool wasabout = running;
            // clear the state before the prompt is redrawn from it
            running = false;
            priming = false;
            if (sep > 0) {
                const int status = tail.left(sep).toInt();
                workingdir       = tail.mid(sep + 1);
                if (wasabout && status != 0)
                    appendOutput(QString("[exit status %1]\n").arg(status));
            }
            updatePrompt();
            // out of the parsing loop first: a movie file among them opens a
            // dialog, which would run the event loop from inside this
            if (!pendingopen.isEmpty()) QTimer::singleShot(0, this, &CommandWindow::openReported);
            // the shell is at a prompt again, so anything that had to wait for
            // it -- a resize, an edited alias -- can be passed on now
            for (const auto &command : pendingsetup)
                shell->write(qPrintable(command + "\n"));
            pendingsetup.clear();
            if (sizepending) sendTerminalSize();
        } else if (!priming && !isShellJobControlNoise(line)) {
            appendOutput(line + "\n");
        }
        nl = pending.indexOf('\n');
    }
}

void CommandWindow::appendOutput(const QString &text)
{
    QString out = text;
    // a progress bar rewrites its line with a carriage return; keep only what it
    // ended up showing rather than every intermediate state
    if (out.contains('\r')) {
        QStringList lines = out.split('\n');
        for (auto &line : lines) {
            const int cr = line.lastIndexOf('\r');
            if (cr >= 0) line = line.mid(cr + 1);
        }
        out = lines.join('\n');
    }
    if (out.endsWith('\n')) out.chop(1);

    scrollback->appendPlainText(out);
    scrollback->moveCursor(QTextCursor::End);
}

void CommandWindow::openReported()
{
    const QStringList wanted = pendingopen;
    pendingopen.clear();
    if (wanted.isEmpty() || !lammpsgui) return;

    // images and movies go into one viewer, so that "open melt-*.png" is a slide
    // show of the sequence rather than a tab for every frame of it
    QStringList pictures;
    QStringList toedit;
    QStringList toplot;
    for (const auto &entry : wanted) {
        // "<what>_<file>", the same shape as the sentinel: no keyword has an
        // underscore in it, so the first one ends it and the rest is the file
        const int sep = entry.indexOf('_');
        if (sep < 1) continue;
        const QString what = entry.left(sep);
        const QString name = entry.mid(sep + 1);
        if (name.isEmpty()) continue; // the command was given nothing to show

        // the shell reported the name as it was written, which for a relative
        // one means relative to where the shell is -- not to where this
        // application was started
        const QFileInfo info(QDir(workingdir), name);
        if (!info.exists()) {
            appendOutput(QString("%1: no such file: %2\n").arg(what, name));
            continue;
        }
        const QString path = info.absoluteFilePath();
        if (what == QLatin1String("edit"))
            toedit << path;
        else if (what == QLatin1String("plot"))
            toplot << path;
        else if (isImageFile(path) || isMovieFile(path))
            pictures << path;
        else
            lammpsgui->viewFile(path);
    }

    if (!pictures.isEmpty()) lammpsgui->openImageFiles(pictures);

    // one editor, one file in it: naming several is a mistake worth saying so
    // about rather than opening whichever happened to be last
    if (!toedit.isEmpty()) {
        if (toedit.size() > 1)
            appendOutput(QString("edit: the editor holds one file at a time, opening \"%1\"\n")
                             .arg(QFileInfo(toedit.first()).fileName()));
        lammpsgui->openFile(toedit.first());
    }

    // each plot asks which columns to draw, so a canceled dialog means "stop",
    // not "ask me again for every remaining file"
    for (const auto &path : toplot)
        if (!lammpsgui->plotFile(path)) break;
}

void CommandWindow::updatePrompt()
{
    // While a command holds the shell there is nothing useful to do with a typed
    // line: it would go down the same pipe and be read by the running program,
    // if it reads at all, and by the shell only once that had finished.  Refuse
    // it instead, and say why.
    prompt->setReadOnly(running);
    killbutton->setEnabled(running);
    if (running) {
        cwdlabel->setText("running >");
        prompt->setPlaceholderText("command running -- append \"&\" to background the next one");
    } else {
        // a deep directory would otherwise take the whole row and leave the
        // input line a few pixels wide; the front of a path is the part that
        // can be spared, and the whole of it stays available as a tool tip
        const QFontMetrics metrics(cwdlabel->font());
        const int budget = qMax(Cfg::COMMAND_MIN_CWD_WIDTH, width() / 2);
        cwdlabel->setText(
            metrics.elidedText(abbreviateHome(workingdir) + "$", Qt::ElideLeft, budget));
        // the path in full, and unabbreviated, is what the elision leaves out
        cwdlabel->setToolTip(workingdir);
        prompt->setPlaceholderText("enter a command");
    }
}

void CommandWindow::shellFinished()
{
    running = false;
    updatePrompt();
    appendOutput("\n[the shell exited; use File > Restart Shell to start a new one]\n");
}

// The processes the shell started directly.  Without job control the shell has
// no job table to ask, so this goes to the operating system instead; a command
// that started children of its own leaves those behind, which is the price of
// not having a session to signal.
QList<qint64> CommandWindow::shellChildren() const
{
    QList<qint64> kids;
    if (!shell || shell->processId() <= 0) return kids;
    const qint64 pid = shell->processId();

#if defined(Q_OS_LINUX)
    QFile children(QString("/proc/%1/task/%1/children").arg(pid));
    if (children.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto parts = QString::fromLatin1(children.readAll()).split(' ', Qt::SkipEmptyParts);
        for (const auto &part : parts) {
            bool ok        = false;
            const qint64 c = part.trimmed().toLongLong(&ok);
            if (ok && (c > 0)) kids << c;
        }
    }
#elif !defined(Q_OS_WIN32)
    QProcess pgrep;
    pgrep.start("pgrep", {"-P", QString::number(pid)});
    if (pgrep.waitForFinished(Cfg::COMMAND_PGREP_TIMEOUT)) {
        const auto lines =
            QString::fromLatin1(pgrep.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            bool ok        = false;
            const qint64 c = line.trimmed().toLongLong(&ok);
            if (ok && (c > 0)) kids << c;
        }
    }
#endif
    return kids;
}

void CommandWindow::killCommand()
{
#if defined(Q_OS_WIN32)
    appendOutput("\n[killing is not supported on this platform;"
                 " use File > Restart Shell]\n");
#else
    if (!shell || (shell->state() != QProcess::Running) || !running) return;

    const auto kids = shellChildren();
    if (kids.isEmpty()) {
        // nothing identifiable to end; freeing the prompt is the next best thing
        appendOutput("\n[cannot identify the running command; restarting the shell]\n");
        restartShell();
        return;
    }

    // ask first, insist shortly afterwards
    appendOutput("\n[killing the running command]\n");
    for (const auto pid : kids)
        ::kill(static_cast<pid_t>(pid), SIGTERM);
    QTimer::singleShot(Cfg::COMMAND_KILL_GRACE, this, [kids]() {
        for (const auto pid : kids)
            if (::kill(static_cast<pid_t>(pid), 0) == 0) ::kill(static_cast<pid_t>(pid), SIGKILL);
    });
#endif
}

void CommandWindow::interrupt()
{
#if defined(Q_OS_WIN32)
    appendOutput("\n[interrupting is not supported on this platform;"
                 " use File > Restart Shell]\n");
#else
    if (!shell || shell->state() != QProcess::Running) return;

    // The shell was put in a session of its own so that its children share a
    // process group with it and nothing else does.  Ask for that group rather
    // than assuming the shell leads it, and refuse to signal our own group,
    // which would take this application down with the command.
    const pid_t group = ::getpgid(shell->processId());
    if ((group <= 0) || (group == ::getpgid(0))) {
        appendOutput("\n[cannot interrupt this command; use File > Restart Shell]\n");
        return;
    }
    // Best effort: bash has no job control here -- there is no terminal to hand
    // a foreground process group to -- and it starts children with SIGINT
    // ignored, so a program that does not install its own handler will sit
    // through this.  File > Restart Shell is the way out of those.
    ::kill(-group, SIGINT);
    appendOutput("\n[interrupt sent; use File > Restart Shell if it had no effect]\n");
    resynchronize();
#endif
}

// A shell left waiting for the rest of an unfinished construct -- "if true;
// then" and nothing more -- has read the sentinel that framed the line as part
// of that construct.  Nothing will ever report the command as done, so the
// prompt stays blocked, and because it is blocked the construct cannot be
// finished from here either.  The interrupt above puts such a shell back at a
// prompt; this is what tells the panel about it.  If a command really was
// running and survived the interrupt, its own sentinel is still queued behind
// it and arrives first, so this one costs a second, harmless report.
void CommandWindow::resynchronize()
{
    QTimer::singleShot(Cfg::COMMAND_RESYNC_DELAY, this, [this]() {
        if (shell && (shell->state() == QProcess::Running))
            shell->write(qPrintable(sentinelCommand(shellprogram, sentinel) + "\n"));
    });
}

void CommandWindow::restartShell()
{
    // Only the shell is ended.  Whatever it started keeps running, which is what
    // is wanted for a program launched with "&", and equally for the graphical
    // one that is holding the prompt: the point is to get a usable prompt back,
    // not to take the user's windows away.
    appendOutput("\n[restarting the shell]\n");
    running = false;
    updatePrompt();
    startShell();
}

void CommandWindow::clearScrollback()
{
    scrollback->clear();
}

void CommandWindow::quit()
{
    if (lammpsgui) lammpsgui->quit();
}

QStringList CommandWindow::pathCommands()
{
    static QStringList cached;
    if (!cached.isEmpty()) return cached;

    // the same path the shell is given, so completion offers what it can run
    const auto dirs = shellSearchPath().split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const auto &dir : dirs) {
        const QFileInfoList entries =
            QDir(dir).entryInfoList(QDir::Files | QDir::Executable | QDir::NoDotAndDotDot);
        for (const auto &entry : entries)
            cached << entry.fileName();
    }
    cached.removeDuplicates();
    cached.sort();
    return cached;
}

void CommandWindow::refreshCompletions()
{
    // Whole lines already typed come first, so that a few characters bring back
    // the command they began rather than the shortest program that starts the
    // same way.  Sorted and without repeats, because the history is kept in the
    // order it was typed and the same line is usually typed more than once --
    // that order is what the arrow keys are for, and it is the wrong one here.
    QStringList sorted = history;
    sorted.removeDuplicates();
    sorted.sort();

    // a name that is both a command and a line of its own is offered once; a
    // set of the history lines keeps this linear -- pathCommands() is a few
    // thousand names and this runs after every submitted command
    const QSet<QString> seen(sorted.cbegin(), sorted.cend());
    for (const auto &command : pathCommands())
        if (!seen.contains(command)) sorted << command;

    commands->setStringList(sorted);
}

// Only what is in the directory the shell is in, and only its plain names: no
// path traversal, no absolute paths, nothing to get right per platform.  That
// covers what the panel is for -- the files a run just wrote, sitting in the
// directory it wrote them to -- and the shell completes nothing at all here, so
// this is the only completion an argument gets.
void CommandWindow::refreshFileNames()
{
    QStringList names;
    const auto entries =
        QDir(workingdir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &entry : entries)
        names << (entry.isDir() ? entry.fileName() + QLatin1Char('/') : entry.fileName());

    filenames->setStringList(names);
    filedir = workingdir;
}

void CommandWindow::updateCompleter(const QString &text)
{
    // a command -- the first word, or the first after a separator -- is
    // completed from the history and the search path; an argument from the
    // directory the shell is in
    if (inCommandPosition(text)) {
        if (completer->model() != commands) completer->setModel(commands);
        if (commands->stringList().isEmpty()) refreshCompletions();
        return;
    }

    // rebuilt on the way in as well as after a cd, so a file a command just
    // wrote is offered without having to reopen the panel
    if ((completer->model() != filenames) || (filedir != workingdir)) {
        refreshFileNames();
        completer->setModel(filenames);
    }
}

bool CommandWindow::eventFilter(QObject *watched, QEvent *event)
{
    // the panel is as wide as the scrollback can show, in characters of the font
    // it shows them in, so either changing means the shell must be told again
    if (((watched == scrollback->viewport()) && (event->type() == QEvent::Resize)) ||
        ((watched == scrollback) && (event->type() == QEvent::FontChange))) {
        sendTerminalSize();
        // the directory in front of the prompt is elided to the width there is
        updatePrompt();
    }

    if ((watched == prompt) && (event->type() == QEvent::KeyPress)) {
        auto *key = static_cast<QKeyEvent *>(event);
        // walk the history; the completer popup uses the arrows itself, so this
        // only applies while it is not showing
        if (!completer->popup()->isVisible()) {
            if (key->key() == Qt::Key_Up) {
                if (historypos > 0) prompt->setText(history.value(--historypos));
                return true;
            }
            if (key->key() == Qt::Key_Down) {
                if (historypos < history.size()) ++historypos;
                prompt->setText(historypos < history.size() ? history.value(historypos)
                                                            : QString());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// Local Variables:
// c-basic-offset: 4
// End:
