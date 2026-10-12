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

#include "helpers.h"
#include "constants.h"

#include <QAbstractButton>
#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QDataStream>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QStyle>
#include <QStyleHints>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QWidget>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

// define consistent function aliases to avoid complications from pre-processing
#if defined(Q_OS_WIN32)
#include <io.h>
#include <process.h>

const auto &mydup    = _dup;
const auto &mydup2   = _dup2;
const auto &myfileno = _fileno;
const auto &myclose  = _close;
const auto &myopen   = _open;
const auto &myexecl  = _execl;
#else
#include <unistd.h>
const auto &mydup    = dup;
const auto &mydup2   = dup2;
const auto &myfileno = fileno;
const auto &myclose  = close;
const auto &myopen   = open;
const auto &myexecl  = execl;
#endif

namespace {
const QStringList months({"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
                          "Nov", "Dec"});

#if defined(Q_OS_WIN32)
// "NUL" without a colon: the spelling verified to work with the MinGW runtime
constexpr char NULL_DEVICE[] = "NUL";
#else
constexpr char NULL_DEVICE[] = "/dev/null";
#endif

int saved_stdout_fd    = -1;
int silenced_counter   = 0;
bool stdout_silenced   = false;
bool capture_is_active = false;
} // namespace

// will be allocated and initialized in main() to avoid segfault on macOS
std::unique_ptr<QFont> GUI_MONOFONT;
std::unique_ptr<QFont> GUI_ALLFONT;

// build the configured fixed-width font from the settings (see helpers.h)
QFont monoFontFromSettings()
{
    QSettings settings;
    QFontInfo mono_info(*GUI_MONOFONT);
    QFont mono_font;
    mono_font.setFamily(settings.value(Keys::MONOFAMILY, mono_info.family()).toString());
    mono_font.setPointSize(settings.value(Keys::MONOSIZE, mono_info.pointSize()).toInt());
    mono_font.setStyleHint(GUI_MONOFONT->styleHint());
    mono_font.setFixedPitch(true);
    return mono_font;
}

// re-assert the configured fixed-width font on a text view's document (see helpers.h)
void reassertMonoFont(QTextDocument *document)
{
    if (!document) return;
    const QFont mono = monoFontFromSettings();
    if (document->defaultFont() != mono) document->setDefaultFont(mono);
}

// re-exec the current process in place; returns only if the re-exec failed
void relaunchApplication()
{
    const auto path = QCoreApplication::applicationFilePath().toStdString();
    const auto arg0 = QCoreApplication::arguments().at(0).toStdString();
    myexecl(path.c_str(), arg0.c_str(), static_cast<char *>(nullptr));
}

void relaunchOrExit(QWidget *parent)
{
    relaunchApplication();
    // only reached when the re-exec failed
    critical(parent, "LAMMPS-GUI Error", "Relaunching LAMMPS-GUI failed.",
             "The changed settings have been saved and take effect when LAMMPS-GUI is "
             "started again. Click on 'Close' to exit.");
    exit(1);
}

// compare two date strings return -1 if first is older than second, 0 if same, or 1
// otherwise

int dateCompare(const QString &one, const QString &two)
{
    if (one == two) return 0;

    // split string into words and check each of them
    auto onelist = one.split(" ", Qt::SkipEmptyParts);
    auto twolist = two.split(" ", Qt::SkipEmptyParts);
    if (onelist.size() != 3) return -1;
    if (twolist.size() != 3) return 1;

    if (onelist[2].toInt() < twolist[2].toInt()) {
        return -1;
    } else if (onelist[2].toInt() > twolist[2].toInt()) {
        return 1;
    }

    onelist[1].truncate(3);
    twolist[1].truncate(3);
    if (months.indexOf(onelist[1]) < months.indexOf(twolist[1])) {
        return -1;
    } else if (months.indexOf(onelist[1]) > months.indexOf(twolist[1])) {
        return 1;
    }

    if (onelist[0].toInt() < twolist[0].toInt()) {
        return -1;
    } else if (onelist[0].toInt() > twolist[0].toInt()) {
        return 1;
    }
    return 0;
}

// Convert string into words on whitespace while handling single and double
// quotes. Adapted from LAMMPS_NS::utils::split_words() to preserve quotes.
// Operates directly on the QString's UTF-16 data so callers never need a
// QString <-> std::string round trip.

QStringList splitLine(const QString &text)
{
    QStringList list;
    const ushort *buf = text.utf16();
    qsizetype beg     = 0;
    qsizetype len     = 0;
    qsizetype add     = 0;

    ushort c = *buf;
    while (c) { // leading whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') {
            c = *++buf;
            ++beg;
            continue;
        };
        len = 0;

    // handle escaped/quoted text.
    quoted:

        if (c == '\'') { // handle single quote
            add = 0;
            len = 1;
            c   = *++buf;
            while ((c != '\'') && (c != '\0')) {
                if ((c == '\\') && (buf[1] == '\'')) {
                    ++buf;
                    ++len;
                }
                c = *++buf;
                ++len;
            }
            ++len;
            c = *++buf;

            // handle triple double quotation marks
        } else if ((c == '"') && (buf[1] == '"') && (buf[2] == '"') && (buf[3] != '"')) {
            len = 3;
            add = 1;
            buf += 3;
            c = *buf;

        } else if (c == '"') { // handle double quote
            add = 0;
            len = 1;
            c   = *++buf;
            while ((c != '"') && (c != '\0')) {
                if ((c == '\\') && (buf[1] == '"')) {
                    ++buf;
                    ++len;
                }
                c = *++buf;
                ++len;
            }
            ++len;
            c = *++buf;
        }

        while (true) { // unquoted
            if ((c == '\'') || (c == '"')) goto quoted;
            // skip escaped quote
            if ((c == '\\') && ((buf[1] == '\'') || (buf[1] == '"'))) {
                ++buf;
                ++len;
                c = *++buf;
                ++len;
            }
            if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f') ||
                (c == '\0')) {
                if (beg < text.size()) list << text.mid(beg, len);
                beg += len + add;
                break;
            }
            c = *++buf;
            ++len;
        }
    }
    return list;
}

// Use one of our own SVG icons as the large QMessageBox icon instead
// of the standard icon. Also set button and window icon consistently.

void setDialogIcons(QMessageBox &mb, const QString &iconPath)
{
    const int extent = mb.style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, &mb);
    mb.setIconPixmap(QIcon(iconPath).pixmap(QSize(extent, extent), mb.devicePixelRatioF()));
    mb.setWindowIcon(QIcon(Cfg::MAIN_ICON));
    mb.setStandardButtons(QMessageBox::Ok);
    styleMessageBoxButtons(mb);
}

// customized information dialog

void information(QWidget *parent, const QString &title, const QString &text1, const QString &text2)
{
    QMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setText(text1);
    if (!text2.isEmpty()) mb.setInformativeText(QString("<p>%1</p>").arg(text2));
    setDialogIcons(mb, ":/icons/help-tutorial.svg");
    mb.exec();
}

// customized critical error dialog

void critical(QWidget *parent, const QString &title, const QString &text1, const QString &text2)
{
    QMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setText(text1);
    if (!text2.isEmpty()) mb.setInformativeText(QString("<p>%1</p>").arg(text2));
    setDialogIcons(mb, ":/icons/process-stop.svg");
    mb.exec();
}

// customized warning dialog

void warning(QWidget *parent, const QString &title, const QString &text1, const QString &text2)
{
    QMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setText(text1);
    if (!text2.isEmpty()) mb.setInformativeText(QString("<p>%1</p>").arg(text2));
    setDialogIcons(mb, ":/icons/warning.svg");
    mb.exec();
}

// platform specific shared library name

QString getLammpsLibName()
{
#if defined(LAMMPS_GUI_USE_PLUGIN)
#if defined(Q_OS_MACOS)
    return Cfg::LAMMPS_LIB_MACOS;
#elif defined(Q_OS_WIN32)
    return Cfg::LAMMPS_LIB_WINDOWS;
#else
    return Cfg::LAMMPS_LIB_LINUX;
#endif
#else
    return QStringLiteral("");
#endif
}

// platform specific shared library download URL

QString getLammpsDownloadUrl()
{
    const QString libName = getLammpsLibName();
    if (libName.isEmpty()) return libName;
#if defined(_MSC_VER)
    // the pre-compiled LAMMPS libraries on the webserver are built with MinGW, which
    // uses a different C runtime than MSVC: the library would load through the C API,
    // but the fd-level stdout capture silently breaks across the two runtimes
    return QStringLiteral("");
#else
    return QStringLiteral("https://download.lammps.org/lammps-gui/") + libName;
#endif
}

// save image directly and if that fails, save to PNG and convert with ImageMagick
void exportImage(QWidget *parent, QImage *image, const QString &title, const QString &defaultname)
{
    if (!image) return;
    QString fileName = QFileDialog::getSaveFileName(parent, "Export Current Image to Image File",
                                                    QDir::current().absoluteFilePath(defaultname),
                                                    Cfg::FILTER_IMAGE);
    if (fileName.isEmpty()) return;
    fileName = ensureFileSuffix(fileName, "png");

    // try direct save and if it fails write to PNG and then convert with ImageMagick if available
    if (!image->save(fileName)) {
        if (hasExe("magick") || hasExe("convert")) {
            QTemporaryFile tmpfile(QDir::tempPath() + "/LAMMPS_GUI.XXXXXX.png");
            // open and close to generate temporary file name
            (void)tmpfile.open();
            (void)tmpfile.close();
            if (!image->save(tmpfile.fileName())) {
                warning(parent, title + " Error", "Could not save image to file " + fileName);
                return;
            }

            QString cmd = findExe("magick");
            QStringList args{tmpfile.fileName(), fileName};
            if (cmd.isEmpty()) cmd = findExe("convert");
            QProcess convert;
            convert.start(cmd, args);
            if (!convert.waitForFinished(-1)) {
                QFile::remove(fileName);
                warning(parent, title + " Error",
                        "ImageMagick conversion failed while saving to file " + fileName + ":",
                        convert.errorString());
                return;
            }
            if (convert.exitStatus() != QProcess::NormalExit || convert.exitCode() != 0) {
                const QString stderrText = QString::fromLocal8Bit(convert.readAllStandardError());
                QFile::remove(fileName);
                warning(parent, title + " Error",
                        "ImageMagick conversion failed while saving to file " + fileName + ":",
                        stderrText.trimmed().isEmpty() ? "" : "Details:\n" + stderrText.trimmed());
                return;
            }
            if (!QFile::exists(fileName)) {
                warning(parent, title + " Error",
                        "ImageMagick reported success, but the output file " + fileName +
                            " was not created.");
                return;
            }
        } else {
            warning(parent, title + " Error", "Could not save image to file " + fileName);
        }
    }
}

// find if executable is in path

bool hasExe(const QString &exe)
{
    return !findExe(exe).isEmpty();
}

QString renameToBackup(const QString &file)
{
    const QString base = file + Cfg::BACKUP_SUFFIX;
    QString backup     = base;
    // a stale backup file may itself be locked and undeletable; then switch
    // to a numbered backup name rather than failing the rename below
    for (int i = 1; QFileInfo::exists(backup) && !QFile::remove(backup); ++i) {
        if (i > 99) return {};
        backup = base + QString::number(i);
    }
    if (QFile::rename(file, backup)) return backup;
    return {};
}

QString findExe(const QString &exe)
{
    QString path = QStandardPaths::findExecutable(exe);
#if defined(Q_OS_MACOS)
    // an app bundle launched from the Finder inherits a minimal PATH without
    // the package manager locations (Homebrew on Arm and Intel macs, MacPorts)
    if (path.isEmpty())
        path = QStandardPaths::findExecutable(
            exe, {"/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin"});
#endif
    return path;
}

bool looksLikeBinaryFile(const QString &filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray chunk = f.read(8192);
    return chunk.contains('\0');
}

// known image extensions, including formats Qt cannot read natively but
// that ImageMagick can convert for display (tga, eps, sgi, ...)
static const QStringList imageExtensions = {"png", "jpg", "jpeg", "bmp", "ppm", "pgm", "pbm",
                                            "gif", "tif", "tiff", "tga", "eps", "sgi", "webp",
                                            "xpm", "ico", "svg",  "jp2", "heic"};

// container formats that FFmpeg can decode into a sequence of images
static const QStringList movieExtensions = {"mp4", "m4v",  "mkv", "webm", "avi", "mov",
                                            "mpg", "mpeg", "ogv", "wmv",  "flv"};

bool isImageFile(const QString &filename)
{
    const QString suffix = QFileInfo(filename).suffix().toLower();
    if (imageExtensions.contains(suffix)) return true;

    // otherwise let Qt sniff the contents, but only for a file that exists
    if (!QFileInfo::exists(filename)) return false;
    return !QImageReader::imageFormat(filename).isEmpty();
}

bool isMovieFile(const QString &filename)
{
    const QString suffix = QFileInfo(filename).suffix().toLower();
    if (movieExtensions.contains(suffix)) return true;

    // a GIF is only a movie when it has more than a single frame
    if ((suffix == "gif") && QFileInfo::exists(filename))
        return QImageReader(filename).imageCount() > 1;
    return false;
}

QString defaultFileStem(const QString &filename)
{
    // non-image, non-movie extensions that are also stripped from the stem
    static const QStringList otherExtensions = {"lmp", "txt",  "csv", "dat",     "yaml",
                                                "yml", "json", "log", "restart", "rst"};

    QString stem = QFileInfo(filename).fileName();
    if (stem.startsWith("in.")) stem.remove(0, 3);

    // strip all trailing known extensions, so e.g. "melt.lmp.txt" becomes "melt"
    int dot = stem.lastIndexOf('.');
    while (dot > 0) {
        const QString ext = stem.mid(dot + 1).toLower();
        if (!otherExtensions.contains(ext) && !imageExtensions.contains(ext) &&
            !movieExtensions.contains(ext))
            break;
        stem.truncate(dot);
        dot = stem.lastIndexOf('.');
    }
    if (stem.isEmpty()) stem = QStringLiteral("lammps");
    return stem;
}

QString ensureFileSuffix(const QString &filename, const QString &suffix)
{
    if (filename.isEmpty() || !QFileInfo(filename).suffix().isEmpty()) return filename;
    return filename + '.' + suffix;
}

bool isRestartFile(const QString &filename)
{
    // LAMMPS binary restart files start with this magic string
    static const char magic[] = "LammpS RestartT";
    char buffer[16]           = "               ";
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream in(&file);
        in.readRawData(buffer, 16);
        file.close();
    }
    return strcmp(buffer, magic) == 0;
}

// recursively remove all contents from a directory

void purgeDirectory(const QString &dir)
{
    QDir directory(dir);
    for (const auto &entry : directory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        if (entry.isDir()) {
            QDir(entry.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(entry.absoluteFilePath());
        }
    }
}

// prefer the platform color scheme when Qt is recent enough to report it;
// otherwise (or when the platform does not know) compare the black level of
// the default palette's foreground and background colors
bool isLightTheme()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::instance()) {
        switch (QGuiApplication::styleHints()->colorScheme()) {
            case Qt::ColorScheme::Light:
                return true;
            case Qt::ColorScheme::Dark:
                return false;
            default: // Qt::ColorScheme::Unknown: fall through to the heuristic
                break;
        }
    }
#endif
    QPalette p;
    int fg = p.brush(QPalette::Active, QPalette::WindowText).color().black();
    int bg = p.brush(QPalette::Active, QPalette::Window).color().black();

    return (fg > bg);
}

// standardized "this is not what you asked for" confirmation dialog
bool confirmUnexpectedFile(QWidget *parent, const QString &filename, const QString &kind)
{
    QMessageBox mb(parent);
    mb.setWindowTitle("Unexpected File Type");
    mb.setWindowIcon(parent ? parent->windowIcon() : QIcon());
    mb.setText(
        QString("\"%1\" does not look like a %2 file.").arg(QFileInfo(filename).fileName(), kind));
    mb.setInformativeText("Do you want to open it anyway?");
    const int extent = mb.style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, &mb);
    mb.setIconPixmap(
        QIcon(":/icons/system-help.svg").pixmap(QSize(extent, extent), mb.devicePixelRatioF()));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    styleMessageBoxButtons(mb);

    // the usual reason to be asked this is a name that was mistyped or a file
    // that was mis-picked, so the safe answer is the one Return and Escape give
    mb.setDefaultButton(QMessageBox::No);
    mb.setEscapeButton(QMessageBox::No);

    if (parent) mb.setFont(parent->font());
    return mb.exec() == QMessageBox::Yes;
}

// standardized "Unsaved Changes" confirmation dialog
int showUnsavedChangesDialog(QWidget *parent, const QString &filename, const QString &question)
{
    QMessageBox mb(parent);
    mb.setWindowTitle("Unsaved Changes");
    mb.setWindowIcon(parent ? parent->windowIcon() : QIcon());
    mb.setText(QString("The buffer ") + filename + " has changes");
    mb.setInformativeText(question);
    const int extent = mb.style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, &mb);
    mb.setIconPixmap(
        QIcon(":/icons/system-help.svg").pixmap(QSize(extent, extent), mb.devicePixelRatioF()));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    styleMessageBoxButtons(mb);

    if (parent) mb.setFont(parent->font());
    return mb.exec();
}

// apply our bundled SVG icons to a dialog button box's standard buttons (see helpers.h)

// The icon for each standard button.  QMessageBox::StandardButton has the same
// values as QDialogButtonBox::StandardButton, so one table serves both.
static const struct {
    QDialogButtonBox::StandardButton id;
    const char *icon;
} BUTTON_ICONS[] = {
    {QDialogButtonBox::Ok, ":/icons/dialog-ok.svg"},
    {QDialogButtonBox::Yes, ":/icons/dialog-ok.svg"},
    {QDialogButtonBox::No, ":/icons/dialog-no.svg"},
    {QDialogButtonBox::Cancel, ":/icons/dialog-cancel.svg"},
    {QDialogButtonBox::Close, ":/icons/window-close.svg"},
};

void styleDialogButtons(QDialogButtonBox *box)
{
    if (!box) return;
    for (const auto &entry : BUTTON_ICONS) {
        if (auto *button = box->button(entry.id)) button->setIcon(QIcon(entry.icon));
    }
}

void styleMessageBoxButtons(QMessageBox &mb)
{
    for (const auto &entry : BUTTON_ICONS) {
        if (auto *button = mb.button(static_cast<QMessageBox::StandardButton>(entry.id)))
            button->setIcon(QIcon(entry.icon));
    }
}

// silence stdout by redirecting to the null device

void silenceStdout()
{
    if (capture_is_active) return;

    // count nested silence requests even when stdout is already redirected, so
    // restoreStdout() only restores when the outermost request is released
    ++silenced_counter;
    if (stdout_silenced) return;

    fflush(stdout);
    saved_stdout_fd = mydup(myfileno(stdout));
    if (saved_stdout_fd == -1) return;

    int devnull = myopen(NULL_DEVICE, O_WRONLY, 0);
    if (devnull == -1) {
        myclose(saved_stdout_fd);
        saved_stdout_fd = -1;
        return;
    }
    mydup2(devnull, myfileno(stdout));
    myclose(devnull);
    stdout_silenced = true;
}

// restore stdout after silencing

void restoreStdout()
{
    if (silenced_counter > 0) --silenced_counter;
    if (!stdout_silenced || (saved_stdout_fd == -1) || (silenced_counter > 0)) return;

    fflush(stdout);
    mydup2(saved_stdout_fd, myfileno(stdout));
    myclose(saved_stdout_fd);
    saved_stdout_fd = -1;
    stdout_silenced = false;
}

// check if stdout is currently silenced

bool isStdoutSilenced()
{
    return stdout_silenced;
}

// notify silence/restore system about StdCapture state changes

void notifyCaptureState(bool active)
{
    capture_is_active = active;
}

// RAII guard collecting Qt log messages instead of printing them (see helpers.h)

QtMessageSilencer *QtMessageSilencer::active = nullptr;

// Only the outermost guard swaps the message handler and thus remembers the
// real one. A nested guard that installed collect() again would make previous
// point at collect() itself, and forwarding to it would never terminate.
QtMessageSilencer::QtMessageSilencer() : outer(active), previous(nullptr)
{
    if (!active) previous = qInstallMessageHandler(collect);
    active = this;
}

QtMessageSilencer::~QtMessageSilencer()
{
    active = outer;
    if (!active) qInstallMessageHandler(previous);
}

QString QtMessageSilencer::messages() const
{
    return collected.join('\n');
}

void QtMessageSilencer::collect(QtMsgType type, const QMessageLogContext &context,
                                const QString &message)
{
    auto *guard = active;
    if (!guard) return;

    // an error that aborts the program, or one the application may act on, must
    // reach whoever was handling messages before the outermost guard took over
    if ((type == QtFatalMsg) || (type == QtCriticalMsg)) {
        const QtMessageSilencer *root = guard;
        while (root->outer)
            root = root->outer;
        if (root->previous)
            root->previous(type, context, message);
        else
            fprintf(stderr, "%s\n", qUtf8Printable(message));
        return;
    }
    guard->collected << message;
}

// desaturate and flatten an image, keeping its alpha channel (see helpers.h)

QImage grayscaleImage(const QImage &src)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            // Dropping the color alone leaves an icon that still has all of its
            // contrast and thus reads as active. Pull the gray levels towards a
            // common midpoint as well, so the result looks unmistakably faded.
            const double gray =
                Cfg::GRAYSCALE_MIDPOINT +
                (qGray(line[x]) - Cfg::GRAYSCALE_MIDPOINT) * Cfg::GRAYSCALE_CONTRAST;
            const int v = std::clamp(qRound(gray), 0, 255);
            line[x]     = qRgba(v, v, v, qAlpha(line[x]));
        }
    }
    return img;
}

QPixmap grayscalePixmap(const QPixmap &src)
{
    return QPixmap::fromImage(grayscaleImage(src.toImage()));
}

// shared tool-button sizing policy (see helpers.h)

QSize toolButtonSize(const QAbstractButton *sample)
{
    const int side = sample->minimumSizeHint().height() + Cfg::TOOLBAR_BUTTON_MARGIN;
    return {side, side};
}

void styleToolButtons(const QSize &size, std::initializer_list<QAbstractButton *> buttons)
{
    const QSize iconsize(Cfg::TOOLBAR_ICON_SIZE, Cfg::TOOLBAR_ICON_SIZE);
    for (auto *button : buttons) {
        button->setMinimumSize(size);
        button->setMaximumSize(size);
        button->setIconSize(iconsize);
    }
}

// shared viewer window auto-resize policy (see helpers.h)

QSize viewerFitSize(const QSize &content, const QSize &budget, int frame, int sbext)
{
    const int w = content.width() + frame;
    const int h = content.height() + frame;

    // an axis that overflows its budget is clamped and gets a scroll bar,
    // which consumes part of the viewport on the other axis
    return {std::min(w + ((h > budget.height()) ? sbext : 0), std::max(budget.width(), 0)),
            std::min(h + ((w > budget.width()) ? sbext : 0), std::max(budget.height(), 0))};
}

QSize fitViewerWindow(QWidget *window, QScrollArea *area, const QSize &content, const QSize &budget,
                      const QSize &lastFit)
{
    if (content.isEmpty()) return lastFit;

    const int frame     = 2 * area->frameWidth();
    const int sbext     = area->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, area);
    const QSize desired = viewerFitSize(content, budget, frame, sbext);
    if (desired == lastFit) return lastFit;

    // pin the scroll area only while the window is resized around it; a
    // permanent minimum would override the user's own resizing afterwards
    area->setMinimumSize(desired);
    window->adjustSize();
    area->setMinimumSize(QSize(0, 0));

    // a hidden window is laid out with unpolished style metrics, so the
    // applied size is only approximate; report no memoized size so the first
    // call on the shown window (see the showEvent() overrides) fits again
    return window->isVisible() ? desired : QSize();
}

// shared window-manager hint policy for output windows (see helpers.h)

namespace {
/// What the command line asked for, if it asked for anything: -1 leaves the
/// choice to the preferences, which is the case in all but a forced session.
int forcedlayout = -1;
} // namespace

void forceLayout(bool docked)
{
    forcedlayout = docked ? 1 : 0;
}

bool dockedLayout()
{
    if (forcedlayout >= 0) return forcedlayout > 0;
    return QSettings().value(Keys::DOCKED, false).toBool();
}

namespace {
QList<QKeySequence> mainwindow_shortcuts;
} // namespace

void setMainWindowShortcuts(const QList<QKeySequence> &keys)
{
    mainwindow_shortcuts = keys;
}

bool isMainWindowShortcut(const QKeySequence &keys)
{
    return !keys.isEmpty() && mainwindow_shortcuts.contains(keys);
}

void scopeShortcut(QWidget *widget, QAction *action, const QKeySequence &keys)
{
    if (!widget || !action) return;
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    widget->addAction(action);
}

void applyWindowFlags(QWidget *window)
{
    if (!window) return;
    auto flags = window->windowFlags();
    flags &= ~Qt::Dialog;
    flags |= Qt::CustomizeWindowHint;
    flags &= ~Qt::WindowMinimizeButtonHint;
#if defined(Q_OS_MACOS)
    // keep the maximize button on macOS: removing it disables window resizing
    flags |= Qt::WindowMaximizeButtonHint;
#else
    flags &= ~Qt::WindowMaximizeButtonHint;
#endif
    window->setWindowFlags(flags);
}

void retireViewMenuBar(QMenuBar *menubar)
{
    if (!menubar) return;
    // on macOS this hands the system-wide menu bar back to the main window's;
    // everywhere else a QMenuBar is not native and this is already false
    menubar->setNativeMenuBar(false);
    menubar->hide();
}

bool installViewMenuBar(QMenuBar *menubar, QMenu *file, const QList<QMenu *> &shared)
{
    if (!menubar) return false;
    if (dockedLayout()) {
        retireViewMenuBar(menubar);
        return false;
    }
    if (file) menubar->addMenu(file);
    for (auto *menu : shared)
        menubar->addMenu(menu);
    return true;
}

void layoutViewMenuBar(QWidget *view, QMenuBar *menubar)
{
    if (!view || !menubar || menubar->isHidden()) return;
    const QRect cr = view->contentsRect();
    menubar->setGeometry(cr.left(), cr.top(), cr.width(), menubar->sizeHint().height());
}

// Local Variables:
// c-basic-offset: 4
// End:
