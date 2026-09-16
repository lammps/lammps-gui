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

#include "imageviewer.h"

#include "dumpimage.h"
#include "imageviewer_internal.h"

#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "lammpswrapper.h"
#include "qaddon.h"
#include "stdcapture.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleValidator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QRect>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

// clang-format off
/* periodic table of elements for translation of ordinal to atom type */
namespace {
    const char * const pte_label[] = {
    "X",  "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P" , "S",  "Cl", "Ar", "K",  "Ca", "Sc",
    "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge",
    "As", "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr", "Nb", "Mo", "Tc",
    "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb", "Te", "I",  "Xe",
    "Cs", "Ba", "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd", "Tb",
    "Dy", "Ho", "Er", "Tm", "Yb", "Lu", "Hf", "Ta", "W",  "Re", "Os",
    "Ir", "Pt", "Au", "Hg", "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr",
    "Ra", "Ac", "Th", "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf",
    "Es", "Fm", "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt",
    "Ds", "Rg"
};
constexpr int nr_pte_entries = sizeof(pte_label) / sizeof(char *);

/* corresponding table of masses. */
constexpr double pte_mass[] = {
    /* X  */ 0.00000, 1.00794, 4.00260, 6.941, 9.012182, 10.811,
    /* C  */ 12.0107, 14.0067, 15.9994, 18.9984032, 20.1797,
    /* Na */ 22.989770, 24.3050, 26.981538, 28.0855, 30.973761,
    /* S  */ 32.065, 35.453, 39.948, 39.0983, 40.078, 44.955910,
    /* Ti */ 47.867, 50.9415, 51.9961, 54.938049, 55.845, 58.9332,
    /* Ni */ 58.6934, 63.546, 65.409, 69.723, 72.64, 74.92160,
    /* Se */ 78.96, 79.904, 83.798, 85.4678, 87.62, 88.90585,
    /* Zr */ 91.224, 92.90638, 95.94, 98.0, 101.07, 102.90550,
    /* Pd */ 106.42, 107.8682, 112.411, 114.818, 118.710, 121.760,
    /* Te */ 127.60, 126.90447, 131.293, 132.90545, 137.327,
    /* La */ 138.9055, 140.116, 140.90765, 144.24, 145.0, 150.36,
    /* Eu */ 151.964, 157.25, 158.92534, 162.500, 164.93032,
    /* Er */ 167.259, 168.93421, 173.04, 174.967, 178.49, 180.9479,
    /* W  */ 183.84, 186.207, 190.23, 192.217, 195.078, 196.96655,
    /* Hg */ 200.59, 204.3833, 207.2, 208.98038, 209.0, 210.0, 222.0,
    /* Fr */ 223.0, 226.0, 227.0, 232.0381, 231.03588, 238.02891,
    /* Np */ 237.0, 244.0, 243.0, 247.0, 247.0, 251.0, 252.0, 257.0,
    /* Md */ 258.0, 259.0, 262.0, 261.0, 262.0, 266.0, 264.0, 269.0,
    /* Mt */ 268.0, 271.0, 272.0
};

/*
 * corresponding table of VDW radii.
 * van der Waals radii are taken from A. Bondi,
 * J. Phys. Chem., 68, 441 - 452, 1964,
 * except the value for H, which is taken from R.S. Rowland & R. Taylor,
 * J.Phys.Chem., 100, 7384 - 7391, 1996. Radii that are not available in
 * either of these publications have RvdW = 2.00 \AA
 * The radii for ions (Na, K, Cl, Ca, Mg, and Cs) are based on the CHARMM27
 * Rmin/2 parameters for (SOD, POT, CLA, CAL, MG, CES) by default.
 */
constexpr double pte_vdw_radius[] = {
    /* X  */ 1.5, 1.2, 1.4, 1.82, 2.0, 2.0,
    /* C  */ 1.7, 1.55, 1.52, 1.47, 1.54,
    /* Na */ 1.36, 1.18, 2.0, 2.1, 1.8,
    /* S  */ 1.8, 2.27, 1.88, 1.76, 1.37, 2.0,
    /* Ti */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Ni */ 1.63, 1.4, 1.39, 1.07, 2.0, 1.85,
    /* Se */ 1.9, 1.85, 2.02, 2.0, 2.0, 2.0,
    /* Zr */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Pd */ 1.63, 1.72, 1.58, 1.93, 2.17, 2.0,
    /* Te */ 2.06, 1.98, 2.16, 2.1, 2.0,
    /* La */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Eu */ 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Er */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* W  */ 2.0, 2.0, 2.0, 2.0, 1.72, 1.66,
    /* Hg */ 1.55, 1.96, 2.02, 2.0, 2.0, 2.0, 2.0,
    /* Fr */ 2.0, 2.0, 2.0, 2.0, 2.0, 1.86,
    /* Np */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Md */ 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
    /* Mt */ 2.0, 2.0, 2.0
};

// clang-format on

// 1) find element in periodic table from their mass
int get_pte_from_mass(double mass)
{
    if (mass <= 0.0) return 0;
    int idx = 0;
    for (int i = 0; i < nr_pte_entries; ++i)
        if (fabs(mass - pte_mass[i]) < 0.65) idx = i;
    if ((mass > 0.0) && (mass < 2.2)) idx = 1;
    // discriminate between Cobalt and Nickel. The loop will detect Nickel
    if ((mass < 61.24) && (mass > 58.8133)) idx = 27;
    return idx;
}

QStringList defaultcolors = {"red",       "green",    "blue",       "yellow",   "cyan",
                             "magenta",   "orange",   "chartreuse", "brown",    "darkred",
                             "darkgreen", "darkblue", "olive",      "darkcyan", "darkmagenta",
                             "silver",    "gray"};

} // namespace

// 2) create a color gradient icon
QIcon gradient_icon(const QList<QPair<double, QColor>> &stops)
{
    if (stops.isEmpty()) return QIcon();

    // define pixmap and horizontal gradient
    QPixmap pixmap(ICON_SIZE, ICON_SIZE);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QLinearGradient gradient(0, 0, ICON_SIZE, 0);

    // place each color at its stop position
    for (const auto &s : stops)
        gradient.setColorAt(std::clamp(s.first, 0.0, 1.0), s.second);

    painter.fillRect(pixmap.rect(), gradient);
    painter.end();

    return QIcon(pixmap);
}

// 3) create a color sequence icon
QIcon sequence_icon(const QList<QColor> &colors)
{
    // if no colors or too many colors return empty icon
    if (colors.isEmpty() || (colors.size() * 2 > ICON_SIZE)) return QIcon();

    // define pixmap
    QPixmap pixmap(ICON_SIZE, ICON_SIZE);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    // distribute colors across icon in evenly sized chunks
    const int chunk = ICON_SIZE / colors.size();
    for (int i = 0; i < colors.size(); ++i)
        painter.fillRect(QRect(i * chunk, 0, chunk, ICON_SIZE), colors[i]);

    painter.end();

    return QIcon(pixmap);
}

// 4) create a single color icon
QPixmap color_icon(const QColor &color)
{
    // define pixmap and fill with color
    QPixmap pixmap(ICON_SIZE / 2, ICON_SIZE / 2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return pixmap;
}

// read JSON color and light data from file
QJsonObject loadJsonColors(QWidget *parent)
{
    QJsonObject obj;
    QString fileName = QFileDialog::getOpenFileName(parent, "Load Colors from JSON",
                                                    QDir::currentPath(), Cfg::FILTER_JSON);
    if (fileName.isEmpty()) return obj;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        warning(parent, "Load Colors", "Could not open file '" + fileName + "' for reading.");
        return obj;
    }

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (doc.isNull() || !doc.isObject()) {
        warning(parent, "Load Colors",
                "Invalid JSON colors file '" + fileName + "': " + err.errorString());
        return obj;
    }
    obj      = doc.object();
    auto app = obj.value("application").toString();
    auto key = obj.value("format").toString();
    auto rev = obj.value("revision").toInt();
    if ((app != "LAMMPS") || (key != "colors")) {
        warning(parent, "Load Colors",
                "JSON colors file '" + fileName + "' is not a LAMMPS colors file.");
        return {};
    }
    if (rev != 1) {
        warning(parent, "Load Colors",
                QString("JSON colors file '%1' has incompatible revision %2 instead of 1")
                    .arg(fileName)
                    .arg(rev));
        return {};
    }

    auto arr = obj.value("colors").toArray();
    if (arr.isEmpty()) {
        warning(parent, "Load Colors",
                "JSON colors file '" + fileName + "' contains no colors entry");
        return obj;
    }

    if (obj.value("lights").toObject().isEmpty()) {
        warning(parent, "Load Colors",
                "JSON colors file '" + fileName + "' contains no lights entry");
        return obj;
    }
    return obj;
}

// save JSON color and light data to file
void saveJsonColors(QWidget *parent, const QJsonArray &colors, const QJsonObject &lights)
{
    QJsonObject root;
    root["application"] = QStringLiteral("LAMMPS");
    root["format"]      = QStringLiteral("colors");
    root["revision"]    = 1;
    root["schema"]      = QStringLiteral("https://download.lammps.org/json/color-schema.json");
    root["colors"]      = colors;
    root["lights"]      = lights;

    QString fileName = QFileDialog::getSaveFileName(parent, "Save Colors to JSON",
                                                    QDir::current().absoluteFilePath("colors.json"),
                                                    Cfg::FILTER_JSON);
    if (fileName.isEmpty()) return;
    fileName = ensureFileSuffix(fileName, "json");

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        warning(parent, "Save Colors", "Could not open file '" + fileName + "' for writing.");
        return;
    }
    file.write(QJsonDocument(root).toJson());
}

// select the combo box entry matching the given text, if present (leave unchanged otherwise)
void selectComboItem(QComboBox *box, const QString &text)
{
    const int idx = box->findText(text);
    if (idx >= 0) box->setCurrentIndex(idx);
}

ImageViewer::ImageViewer(const QString &fileName, LammpsWrapper *_lammps, LammpsGui *_lammpsgui,
                         QWidget *parent) :
    QDialog(parent), menuBar(new QMenuBar), imageLabel(new QLabel), scrollArea(new QScrollArea),
    atomSize(1.0), bondSize(0.4), saveAsAct(nullptr), copyAct(nullptr), cmdAct(nullptr),
    lammps(_lammps), lammpsgui(_lammpsgui), group("all"), molecule("none"), filename(fileName),
    useelements(false), usediameter(false), usesigma(false), shutdown(false)
{
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    imageLabel->setScaledContents(false);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    scrollArea->setVisible(false);

    auto *imageLayout    = new QHBoxLayout;
    auto *settingsLayout = new QVBoxLayout;
    auto *mainLayout     = new QVBoxLayout;

    QFile image_styles(":/image_style.table");
    if (image_styles.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!image_styles.atEnd()) {
            auto line  = QString(image_styles.readLine());
            auto words = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (words.size() == 2) {
                if (words.at(0) == "compute") {
                    image_computes << words.at(1);
                } else if (words.at(0) == "fix") {
                    image_fixes << words.at(1);
                } else {
                    fprintf(stderr, "unhandled image style: %s\n", qPrintable(line));
                }
            } else {
                fprintf(stderr, "unhandled image style: %s\n", qPrintable(line));
            }
        }
        image_styles.close();
    }

    // store help URL info for computes and fixes with dump image support
    QFile help_index(":/help_index.table");
    if (help_index.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!help_index.atEnd()) {
            auto line  = QString(help_index.readLine());
            auto words = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (words.size() == 3) {
                if (words.at(1) == "fix") {
                    if (image_fixes.contains(words.at(2))) fix_map[words.at(2)] = words.at(0);
                } else if (words.at(1) == "compute") {
                    if (image_computes.contains(words.at(2)))
                        compute_map[words.at(2)] = words.at(0);
                }
            }
        }
        help_index.close();
    }

    readImageSettings();
    // initialize atomSize with lattice spacing
    const auto *xlattice = static_cast<const double *>(lammps->extractGlobal("xlattice"));
    if (xlattice) atomSize = *xlattice;

    auto pix   = QPixmap(":/icons/emblem-photos.png");
    auto fsize = QFontMetrics(QApplication::font()).size(Qt::TextSingleLine, "Height: 200");
#if defined(Q_OS_WIN32)
    fsize = fsize * 3 / 2;
#endif

    auto *renderstatus = new QLabel(QString());
    // The render-status icon shows a full-color "active" pixmap while an image is
    // rendering and a faded "idle" pixmap otherwise. Swap the two pixmaps
    // explicitly (stored as properties) rather than relying on the disabled-widget
    // visual, which does not refresh reliably on all platforms (e.g. macOS 12).
    const QPixmap activePix = pix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPixmap idlePix   = grayscalePixmap(activePix);
    renderstatus->setProperty("activePix", activePix);
    renderstatus->setProperty("idlePix", idlePix);
    renderstatus->setPixmap(idlePix);
    renderstatus->setToolTip("Render status");
    renderstatus->setObjectName("renderstatus");
    auto *asize = new QLineEdit(QString::number(2.0 * atomSize, 'f', 2));
    auto *valid = new QDoubleValidator(1.0e-10, 1.0e10, 3, this);
    asize->setValidator(valid);
    asize->setObjectName("atomSize");
    asize->setToolTip("Set Atom size");
    asize->setMinimumWidth(fsize.width() / 4);
    asize->setMaximumWidth(fsize.width() / 2);
    asize->setEnabled(false);
    asize->hide();
    auto *bsize = new QLineEdit(QString::number(bondSize, 'f', 2));
    bsize->setValidator(valid);
    bsize->setObjectName("bondSize");
    bsize->setToolTip("Set Bond size");
    bsize->setMinimumWidth(fsize.width() / 4);
    bsize->setMaximumWidth(fsize.width() / 2);
    bsize->setEnabled(false);
    bsize->hide();

    auto *xval = new QSpinBox;
    xval->setRange(100, 10000);
    xval->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    xval->setValue(xsize);
    xval->setObjectName("xsize");
    xval->setToolTip("Set rendered image width");
    xval->setMinimumSize(fsize);
    auto *yval = new QSpinBox;
    yval->setRange(100, 10000);
    yval->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    yval->setValue(ysize);
    yval->setObjectName("ysize");
    yval->setToolTip("Set rendered image height");
    yval->setMinimumSize(fsize);

    connect(asize, &QLineEdit::editingFinished, this, &ImageViewer::setAtomSize);
    connect(bsize, &QLineEdit::editingFinished, this, &ImageViewer::setBondSize);
    connect(xval, &QAbstractSpinBox::editingFinished, this, &ImageViewer::editSize);
    connect(yval, &QAbstractSpinBox::editingFinished, this, &ImageViewer::editSize);

    // workaround for incorrect highlight bug on macOS
    auto *dummy1 = new QPushButton(QIcon(), "");
    dummy1->hide();
    auto *dummy2 = new QPushButton(QIcon(), "");
    dummy2->hide();

    auto *dossao = new QPushButton(QIcon(":/icons/hd-img.svg"), "");
    dossao->setCheckable(true);
    dossao->setToolTip("Toggle SSAO rendering");
    dossao->setObjectName("ssao");
    const QSize buttonhint = toolButtonSize(dossao);
    auto *doanti           = new QPushButton(QIcon(":/icons/antialias.svg"), "");
    doanti->setCheckable(true);
    doanti->setToolTip("Toggle anti-aliasing");
    doanti->setObjectName("antialias");
    auto *dodepth = new QPushButton(QIcon(":/icons/depth-cue.svg"), "");
    dodepth->setCheckable(true);
    dodepth->setToolTip("Toggle depth cueing");
    dodepth->setObjectName("depthcue");
    auto *dodefocus = new QPushButton(QIcon(":/icons/defocus.svg"), "");
    dodefocus->setCheckable(true);
    dodefocus->setToolTip("Toggle defocusing of distant objects");
    dodefocus->setObjectName("defocus");
    auto *doshiny = new QPushButton(QIcon(":/icons/image-shiny.svg"), "");
    doshiny->setCheckable(true);
    doshiny->setToolTip("Toggle shininess");
    doshiny->setObjectName("shiny");
    auto *dovdw = new QPushButton(QIcon(":/icons/vdw-style.svg"), "");
    dovdw->setCheckable(true);
    dovdw->setToolTip("Toggle VDW style representation");
    dovdw->setObjectName("vdw");
    auto *dobond = new QPushButton(QIcon(":/icons/autobonds.svg"), "");
    dobond->setCheckable(true);
    dobond->setToolTip("Toggle dynamic bond representation");
    dobond->setObjectName("autobond");
    dobond->setEnabled(false);
    auto *bondcut = new QLineEdit(QString::number(bondcutoff));
    bondcut->setMaxLength(5);
    bondcut->setObjectName("bondcut");
    bondcut->setToolTip("Set dynamic bond cutoff");
    QFontMetrics metrics(bondcut->fontMetrics());
    // keep the cutoff field height in sync with the toolbar buttons
    bondcut->setFixedSize(metrics.averageCharWidth() * 6, buttonhint.height());
    bondcut->setEnabled(false);
    auto *dobox = new QPushButton(QIcon(":/icons/show-box.png"), "");
    dobox->setCheckable(true);
    dobox->setToolTip("Toggle displaying box");
    dobox->setObjectName("box");
    auto *doaxes = new QPushButton(QIcon(":/icons/show-axes.png"), "");
    doaxes->setCheckable(true);
    doaxes->setToolTip("Toggle displaying axes");
    doaxes->setObjectName("axes");
    auto *zoomin = new QPushButton(QIcon(":/icons/gtk-zoom-in.svg"), "");
    zoomin->setToolTip("Zoom in by 10 percent");
    auto *zoomout = new QPushButton(QIcon(":/icons/gtk-zoom-out.svg"), "");
    zoomout->setToolTip("Zoom out by 10 percent");
// the SVG versions do not render correctly with Qt before 6.7
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    auto *rotleft  = new QPushButton(QIcon(":/icons/rotate-left.svg"), "");
    auto *rotright = new QPushButton(QIcon(":/icons/rotate-right.svg"), "");
    auto *rotup    = new QPushButton(QIcon(":/icons/rotate-up.svg"), "");
    auto *rotdown  = new QPushButton(QIcon(":/icons/rotate-down.svg"), "");
#else
    auto *rotleft  = new QPushButton(QIcon(":/icons/rotate-left.png"), "");
    auto *rotright = new QPushButton(QIcon(":/icons/rotate-right.png"), "");
    auto *rotup    = new QPushButton(QIcon(":/icons/rotate-up.png"), "");
    auto *rotdown  = new QPushButton(QIcon(":/icons/rotate-down.png"), "");
#endif
    rotleft->setToolTip("Rotate left by 10 degrees");
    rotright->setToolTip("Rotate right by 10 degrees");
    rotup->setToolTip("Rotate up by 10 degrees");
    rotdown->setToolTip("Rotate down by 10 degrees");
    auto *recenter = new QPushButton(QIcon(":/icons/move-recenter.svg"), "");
    recenter->setToolTip("Recenter on group");
    auto *reset = new QPushButton(QIcon(":/icons/gtk-zoom-fit.svg"), "");
    reset->setToolTip("Reset view to defaults");
    auto *fitwin = new QPushButton(QIcon(":/icons/fit-window.svg"), "");
    fitwin->setToolTip("Resize window to fit the image size");

    // square toolbar buttons with a snug, uniform icon (shared policy)
    styleToolButtons(buttonhint,
                     {dossao, doanti, dodepth, dodefocus, doshiny, dovdw, dobond, dobox, doaxes,
                      zoomin, zoomout, rotleft, rotright, rotup, rotdown, recenter, reset, fitwin});

    // match the first-row controls (menu bar and size fields) to the toolbar
    // button height so both rows line up and the layout looks balanced
    menuBar->setFixedHeight(buttonhint.height());
    xval->setFixedHeight(buttonhint.height());
    yval->setFixedHeight(buttonhint.height());
    asize->setFixedHeight(buttonhint.height());
    bsize->setFixedHeight(buttonhint.height());

    auto *setviz = new QPushButton("G&lobal");
    setviz->setToolTip("Open dialog for global graphics settings");
    auto *atomviz = new QPushButton("&Atoms/Bonds");
    atomviz->setToolTip("Open dialog for atom and bond settings");
    auto *fixviz = new QPushButton("&Compute/Fix");
    fixviz->setToolTip("Open dialog for visualizing extra graphics from computes and fixes");
    fixviz->setObjectName("image_styles");
    fixviz->setEnabled(false);
    auto *regviz = new QPushButton("&Regions");
    regviz->setToolTip("Open dialog for visualizing regions");
    regviz->setObjectName("regions");
    regviz->setEnabled(false);
    auto *colviz = new QPushButton("C&olors");
    colviz->setToolTip("Open dialog for customizing colors");
    auto *help = new QPushButton("Help");
    help->setToolTip("Open online help");
    help->setObjectName("visualization.html");

    auto *combo = new QComboBox;
    combo->setToolTip("Select group to display");
    combo->setObjectName("group");
    int ngroup = lammps->idCount("group");
    for (int i = 0; i < ngroup; ++i)
        combo->addItem(lammps->idName("group", i));

    auto *molbox = new QComboBox;
    molbox->setToolTip("Select molecule to display");
    molbox->setObjectName("molecule");
    molbox->addItem("none");
    int nmols = lammps->idCount("molecule");
    for (int i = 0; i < nmols; ++i)
        molbox->addItem(lammps->idName("molecule", i));

    auto *menuLayout   = new QHBoxLayout;
    auto *buttonLayout = new QHBoxLayout;
    auto *topLayout    = new QVBoxLayout;
    topLayout->addLayout(menuLayout);
    topLayout->addLayout(buttonLayout);
    topLayout->setSpacing(LAYOUT_SPACING);

    // a hidden dummy button as the first item works around a macOS bug where the
    // first widget in a toolbar row misbehaves (here: renderstatus not refreshing)
    menuLayout->addWidget(dummy1);
    menuLayout->addWidget(menuBar);
    menuLayout->insertStretch(2, 10);
    menuLayout->addWidget(renderstatus);
    menuLayout->addWidget(new QLabel(" Atom Size: "));
    // hide item initially
    menuLayout->itemAt(4)->widget()->setObjectName("AtomLabel");
    menuLayout->itemAt(4)->widget()->hide();
    menuLayout->addWidget(asize);
    menuLayout->addWidget(new QLabel(" Bond Size: "));
    // hide item initially
    menuLayout->itemAt(6)->widget()->setObjectName("BondLabel");
    menuLayout->itemAt(6)->widget()->hide();
    menuLayout->addWidget(bsize);
    menuLayout->addWidget(new QLabel(" <u>W</u>idth: "));
    menuLayout->addWidget(xval);
    menuLayout->addWidget(new QLabel(" <u>H</u>eight: "));
    menuLayout->addWidget(yval);
    menuLayout->insertStretch(-1, 50);
    menuLayout->setSpacing(LAYOUT_SPACING);

    buttonLayout->addWidget(dummy2);
    buttonLayout->addWidget(dossao);
    buttonLayout->addWidget(doanti);
    buttonLayout->addWidget(dodepth);
    buttonLayout->addWidget(dodefocus);
    buttonLayout->addWidget(doshiny);
    buttonLayout->addWidget(dovdw);
    buttonLayout->addWidget(dobond);
    buttonLayout->addWidget(bondcut);
    buttonLayout->addWidget(dobox);
    buttonLayout->addWidget(doaxes);
    buttonLayout->addWidget(zoomin);
    buttonLayout->addWidget(zoomout);
    buttonLayout->addWidget(rotleft);
    buttonLayout->addWidget(rotright);
    buttonLayout->addWidget(rotup);
    buttonLayout->addWidget(rotdown);
    buttonLayout->addWidget(recenter);
    buttonLayout->addWidget(reset);
    buttonLayout->addWidget(fitwin);
    buttonLayout->insertStretch(-1, 1);
    buttonLayout->setSizeConstraint(QLayout::SetMinimumSize);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(LAYOUT_SPACING);
    settingsLayout->addWidget(new QHline);
    settingsLayout->addWidget(new QLabel("<u>G</u>roup:"));
    settingsLayout->addWidget(combo);
    settingsLayout->addWidget(new QHline);
    settingsLayout->addWidget(new QLabel("<u>M</u>olecule:"));
    settingsLayout->addWidget(molbox);
    settingsLayout->addWidget(new QHline);
    settingsLayout->addWidget(new QLabel("Settings:"));
    settingsLayout->addWidget(setviz);
    settingsLayout->addWidget(atomviz);
    settingsLayout->addWidget(regviz);
    settingsLayout->addWidget(fixviz);
    settingsLayout->addWidget(colviz);
    settingsLayout->addWidget(new QHline);
    settingsLayout->addWidget(help);
    settingsLayout->insertStretch(-1, 10);
    settingsLayout->setSizeConstraint(QLayout::SetMinimumSize);
    settingsLayout->setSpacing(LAYOUT_SPACING);

    connect(dossao, &QPushButton::released, this, &ImageViewer::toggleSsao);
    connect(doanti, &QPushButton::released, this, &ImageViewer::toggleAnti);
    connect(dodepth, &QPushButton::released, this, &ImageViewer::toggleDepthcue);
    connect(dodefocus, &QPushButton::released, this, &ImageViewer::toggleDefocus);
    connect(doshiny, &QPushButton::released, this, &ImageViewer::toggleShiny);
    connect(dovdw, &QPushButton::released, this, &ImageViewer::toggleVdw);
    connect(dobond, &QPushButton::released, this, &ImageViewer::toggleBond);
    connect(bondcut, &QLineEdit::editingFinished, this, &ImageViewer::setBondcut);
    connect(dobox, &QPushButton::released, this, &ImageViewer::toggleBox);
    connect(doaxes, &QPushButton::released, this, &ImageViewer::toggleAxes);
    connect(zoomin, &QPushButton::released, this, &ImageViewer::doZoomIn);
    connect(zoomout, &QPushButton::released, this, &ImageViewer::doZoomOut);
    connect(rotleft, &QPushButton::released, this, &ImageViewer::doRotLeft);
    connect(rotright, &QPushButton::released, this, &ImageViewer::doRotRight);
    connect(rotup, &QPushButton::released, this, &ImageViewer::doRotUp);
    connect(rotdown, &QPushButton::released, this, &ImageViewer::doRotDown);
    connect(recenter, &QPushButton::released, this, &ImageViewer::doRecenter);
    connect(reset, &QPushButton::released, this, &ImageViewer::resetView);
    connect(fitwin, &QPushButton::released, this, &ImageViewer::resetWindowSize);
    // a docked panel is sized by its dock area, not by the image
    if (dockedLayout()) fitwin->hide();
    connect(setviz, &QPushButton::released, this, &ImageViewer::globalSettings);
    connect(atomviz, &QPushButton::released, this, &ImageViewer::atomSettings);
    connect(fixviz, &QPushButton::released, this, &ImageViewer::fixSettings);
    connect(regviz, &QPushButton::released, this, &ImageViewer::regionSettings);
    connect(colviz, &QPushButton::released, this, &ImageViewer::colorSettings);
    connect(help, &QPushButton::released, this, &ImageViewer::getHelp);
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ImageViewer::changeGroup);
    connect(molbox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ImageViewer::changeMolecule);

    mainLayout->addLayout(topLayout);
    imageLayout->addWidget(scrollArea, 10);
    imageLayout->addLayout(settingsLayout, 0);
    imageLayout->setSpacing(LAYOUT_SPACING);
    mainLayout->addLayout(imageLayout);
    mainLayout->setSpacing(LAYOUT_SPACING);
    setWindowIcon(QIcon(Cfg::MAIN_ICON));
    setWindowTitle(QString("LAMMPS-GUI - Image Viewer - ") + QFileInfo(fileName).fileName());
    createActions();

    resetView();
    // layout has not yet been established, so we need to fix up some pushbutton
    // properties directly since lookup in resetView() will have failed
    dobox->setChecked(showbox);
    doshiny->setChecked(shinyfactor > SHINY_CUT);
    dovdw->setChecked(vdwfactor > VDW_CUT);
    dovdw->setEnabled(showatoms && (useelements || usediameter || usesigma));
    dobond->setChecked(autobond);
    dobond->setEnabled(hasAutobonds());
    doaxes->setChecked(showaxes);
    dossao->setChecked(usessao);
    doanti->setChecked(antialias);
    dodepth->setChecked(usedepthcue);
    dodefocus->setChecked(usedefocus);

    scrollArea->setVisible(true);
    updateActions();
    setLayout(mainLayout);
    mainLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    adjustWindowSize();
    updateFixes();
    updateRegions();
    menuBar->setFocus();

    // make Alt-G, Alt-H, Alt-M, and Alt-W hotkeys work for comboboxes and spinboxes
    xval->installEventFilter(this);
    yval->installEventFilter(this);
    combo->installEventFilter(this);
    for (auto &obj : combo->children())
        obj->installEventFilter(this);
    molbox->installEventFilter(this);
    for (auto &obj : molbox->children())
        obj->installEventFilter(this);
    installEventFilter(this);

    applyWindowFlags(this);
}

ImageViewer::~ImageViewer()
{
    shutdown = true;

    // clear dynamically allocated storage

    for (auto &comp : computes)
        delete comp.second;
    for (auto &ifix : fixes)
        delete ifix.second;
    for (auto &ireg : regions)
        delete ireg.second;
}

void ImageViewer::readImageSettings()
{
    QSettings settings;
    settings.beginGroup(Keys::GROUP_SNAPSHOT);
    xsize          = settings.value(Keys::XSIZE, "600").toInt();
    ysize          = settings.value(Keys::YSIZE, "600").toInt();
    zoom           = settings.value(Keys::ZOOM, 1.0).toDouble();
    hrot           = settings.value(Keys::HROT, 60).toInt();
    vrot           = settings.value(Keys::VROT, 30).toInt();
    shinyfactor    = settings.value(Keys::SHINYSTYLE, true).toBool() ? SHINY_ON : SHINY_OFF;
    vdwfactor      = settings.value(Keys::VDWSTYLE, false).toBool() ? VDW_ON : VDW_OFF;
    autobond       = settings.value(Keys::AUTOBOND, false).toBool();
    bondcutoff     = settings.value(Keys::BONDCUT, 1.6).toDouble();
    showbox        = settings.value(Keys::BOX, true).toBool();
    showsubbox     = false;
    boxdiam        = settings.value(Keys::BOXDIAM, 0.025).toDouble();
    subboxdiam     = boxdiam;
    boxcolor       = settings.value(Keys::BOXCOLOR, "gold").toString();
    showaxes       = settings.value(Keys::AXES, false).toBool();
    usessao        = settings.value(Keys::SSAO, false).toBool();
    antialias      = settings.value(Keys::ANTIALIAS, false).toBool();
    axeslen        = settings.value(Keys::AXESLEN, 0.5).toDouble();
    axesdiam       = settings.value(Keys::AXESDIAM, 0.05).toDouble();
    axestrans      = 1.0;
    axesloc        = "yes"; // = "lowerleft"
    boxtrans       = 1.0;
    backcolor      = settings.value(Keys::BACKCOLOR, "black").toString();
    backcolor2     = settings.value(Keys::BACKCOLOR2, "white").toString();
    usegradient    = settings.value(Keys::USEGRADIENT, true).toBool();
    ssaoval        = 0.6;
    ssaosamples    = 0;
    usedepthcue    = false;
    depthcuefactor = 0.5;
    depthcuecolor  = "auto";
    depthcuestart  = "auto";
    usedefocus     = false;
    defocusfactor  = 0.5;
    defocusstart   = "auto";
    useoutline     = false;
    outlinewidth   = 2;
    outlinecolor   = "black";
    specular       = "auto";
    usemetal       = false;
    metalfactor    = 0.5;
    metalfinish    = "satin";
    gammaval       = 1.0;
    atomcustom     = false;
    atomtrans      = 1.0;
    bondtrans      = 1.0;
    atomcolor      = settings.value(Keys::COLOR, "type").toString();
    atomdiam       = settings.value(Keys::DIAMETER, "type").toString();
    bondcolor      = settings.value(Keys::BONDCOLOR, "atom").toString();
    bonddiam       = settings.value(Keys::BONDDIAM, "type").toString();
    bodycolor      = "atom";
    ellipsoidcolor = "atom";
    linecolor      = "atom";
    tricolor       = "atom";
    // the historical blue-low/red-high default map ("RWB" is its unlisted
    // reversed alias, kept so stale selections still render)
    colormap        = settings.value(Keys::COLORMAP, "BWR").toString();
    mapmin          = "auto";
    mapmax          = "auto";
    revcolormap     = false;
    bondcolormap    = settings.value(Keys::BONDCOLORMAP, "BWR").toString();
    bondmapmin      = "auto";
    bondmapmax      = "auto";
    revbondcolormap = false;

    showatoms      = true;
    showbonds      = lammps->extractSetting("molecule_flag") == 1;
    showbodies     = true;
    bodydiam       = 0.2;
    bodyflag       = TRIANGLES;
    showellipsoids = true;
    ellipsoidflag  = TRIANGLES;
    ellipsoidlevel = 3;
    ellipsoiddiam  = 0.2;
    showlines      = true;
    linediam       = 0.2;
    showtris       = true;
    tridiam        = 0.2;
    triflag        = CYLINDERS;
    xcenter = ycenter = zcenter = 0.5;
    dynamiccenter               = false;
    // the camera up direction defaults to the z-axis in 3d and the y-axis in 2d
    xup = yup = zup = 0.0;
    if (lammps->extractSetting("dimension") == 2) {
        zcenter = 0.0;
        yup     = 1.0;
    } else {
        zup = 1.0;
    }
    settings.endGroup();

    if (color_list.isEmpty()) resetColors(); // create list of default colors
}

void ImageViewer::resetView()
{
    readImageSettings();

    // reset state of checkable push buttons and combo box (if accessible)
    // also flag that atom/bond customizations should be ignored
    atomcustom = false;

    auto *field = findChild<QSpinBox *>("xsize");
    if (field) field->setValue(xsize);
    field = findChild<QSpinBox *>("ysize");
    if (field) field->setValue(ysize);

    auto *button = findChild<QPushButton *>("ssao");
    if (button) button->setChecked(usessao);
    button = findChild<QPushButton *>("antialias");
    if (button) button->setChecked(antialias);
    button = findChild<QPushButton *>("depthcue");
    if (button) button->setChecked(usedepthcue);
    button = findChild<QPushButton *>("defocus");
    if (button) button->setChecked(usedefocus);
    button = findChild<QPushButton *>("shiny");
    if (button) button->setChecked(shinyfactor > SHINY_CUT);
    button = findChild<QPushButton *>("vdw");
    if (button) button->setChecked(vdwfactor > VDW_CUT);
    button = findChild<QPushButton *>("autobond");
    if (button) {
        button->setEnabled(hasAutobonds());
        button->setChecked(autobond && hasAutobonds());
    }
    auto *cutoff = findChild<QLineEdit *>("bondcut");
    if (cutoff) {
        cutoff->setEnabled(autobond && hasAutobonds());
        cutoff->setText(QString::number(bondcutoff));
    }
    button = findChild<QPushButton *>("box");
    if (button) button->setChecked(showbox);
    button = findChild<QPushButton *>("axes");
    if (button) button->setChecked(showaxes);
    auto *cb = findChild<QComboBox *>("group");
    if (cb) cb->setCurrentText("all");
    createImage();
}

void ImageViewer::setAtomSize()
{
    auto *field = qobject_cast<QLineEdit *>(sender());
    if (!field) return;
    atomSize = 0.5 * field->text().toDouble();
    atomdiam = field->text();
    createImage();
}

void ImageViewer::setBondSize()
{
    auto *field = qobject_cast<QLineEdit *>(sender());
    if (!field) return;
    bondSize = field->text().toDouble();
    bonddiam = field->text();
    createImage();
}

void ImageViewer::editSize()
{
    auto *field = qobject_cast<QSpinBox *>(sender());
    if (!field) return;
    if (field->objectName() == "xsize") {
        xsize = field->value();
    } else if (field->objectName() == "ysize") {
        ysize = field->value();
    }
    createImage();
}

void ImageViewer::toggleSsao()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    usessao = !usessao;
    button->setChecked(usessao);
    createImage();
}

void ImageViewer::toggleAnti()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    antialias = !antialias;
    button->setChecked(antialias);
    createImage();
}

void ImageViewer::toggleDepthcue()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    usedepthcue = !usedepthcue;
    button->setChecked(usedepthcue);
    createImage();
}

void ImageViewer::toggleDefocus()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    usedefocus = !usedefocus;
    button->setChecked(usedefocus);
    createImage();
}

void ImageViewer::toggleShiny()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    if (shinyfactor > SHINY_CUT)
        shinyfactor = SHINY_OFF;
    else
        shinyfactor = SHINY_ON;
    button->setChecked(shinyfactor > SHINY_CUT);
    createImage();
}

void ImageViewer::toggleVdw()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    if (button->isChecked())
        vdwfactor = VDW_ON;
    else
        vdwfactor = VDW_OFF;

    // when enabling VDW rendering, we must turn off autobond
    bool do_vdw = vdwfactor > VDW_CUT;
    if (do_vdw) {
        autobond   = false;
        auto *bond = findChild<QPushButton *>("autobond");
        if (bond) bond->setChecked(false);
        auto *cutoff = findChild<QLineEdit *>("bondcut");
        if (cutoff) cutoff->setEnabled(false);
    }

    button->setChecked(do_vdw);
    createImage();
}

void ImageViewer::toggleBond()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (button) autobond = button->isChecked();
    auto *cutoff = findChild<QLineEdit *>("bondcut");
    if (cutoff) cutoff->setEnabled(autobond);
    setBondcut();

    // when enabling autobond, we must turn off VDW
    if (autobond) {
        vdwfactor = VDW_OFF;
        auto *vdw = findChild<QPushButton *>("vdw");
        if (vdw) vdw->setChecked(false);
    }

    auto *edit  = findChild<QLineEdit *>("bondSize");
    auto *label = findChild<QLabel *>("BondLabel");
    if ((showbonds || autobond) && (bonddiam != "type") && (bonddiam != "atom") &&
        (bonddiam != "none")) {
        if (edit) {
            edit->setEnabled(true);
            edit->show();
            bondSize = bonddiam.toDouble();
            edit->setText(bonddiam);
        }
        if (label) {
            label->setEnabled(true);
            label->show();
        }
    } else {
        if (edit) {
            edit->setEnabled(false);
            edit->hide();
        }
        if (label) {
            label->setEnabled(false);
            label->hide();
        }
    }

    if (button) button->setChecked(autobond);
    createImage();
}

void ImageViewer::vdwbondSync()
{
    auto *src    = qobject_cast<QCheckBox *>(sender());
    auto *dialog = src->parent();
    auto *vdw    = dialog->findChild<QCheckBox *>("vdwbutton");
    auto *ab     = dialog->findChild<QCheckBox *>("autobutton");

    if (src == vdw) {
        if (vdw->isChecked() && ab->isChecked()) ab->setChecked(false);
    } else {
        if (vdw->isChecked() && ab->isChecked()) vdw->setChecked(false);
    }

    // compute bond/local coloring needs real bonds, so it is unavailable while
    // AutoBonds is on; refresh the bond Color choices to match the live state.
    // Without real bonds, AutoBonds is the only source of bonds and those are
    // always colored by the atoms forming the bond, so force the bond Color
    // selection to "atom" and only leave the diameter selectable
    auto *bncolor = dialog->findChild<QComboBox *>("bncolor");
    if (bncolor) {
        const bool hasRealBonds = (lammps->extractSetting("molecule_flag") == 1);
        rebuildBondColorChoices(bncolor, hasRealBonds && !ab->isChecked());
        const bool atomsonly = !hasRealBonds && ab->isChecked();
        if (atomsonly) selectComboItem(bncolor, "atom");
        bncolor->setEnabled(!atomsonly);
    }
}

void ImageViewer::acolorSync()
{
    auto *src = qobject_cast<QComboBox *>(sender());
    if (!src) return;
    auto *dialog = qobject_cast<QWidget *>(src->parent());

    // enable/disable colormap selector (and its Reverse toggle) depending on the
    // atom coloring selection
    auto *amap         = dialog->findChild<QComboBox *>("amap");
    auto *arevbutton   = dialog->findChild<QCheckBox *>("arevbutton");
    const bool byvalue = (src->currentText() != "type") && (src->currentText() != "element");
    if (amap) amap->setEnabled(byvalue);
    if (arevbutton) arevbutton->setEnabled(byvalue);
    auto *acolor = dialog->findChild<QComboBox *>("acolor");
    auto *bcolor = dialog->findChild<QComboBox *>("bcolor");
    auto *ecolor = dialog->findChild<QComboBox *>("ecolor");
    auto *lcolor = dialog->findChild<QComboBox *>("lcolor");
    auto *tcolor = dialog->findChild<QComboBox *>("tcolor");

    if (src && acolor && bcolor && ecolor && lcolor && tcolor) {
        if (src == acolor) {
            if (src->currentText() != "type") {
                for (auto *box : {bcolor, ecolor, lcolor, tcolor})
                    selectComboItem(box, "atom");
            }
        } else {
            if (src->currentText() != "atom") {
                selectComboItem(acolor, "type");
            }
        }
    }
}

void ImageViewer::setBondcut()
{
    auto *cutoff = findChild<QLineEdit *>("bondcut");
    if (cutoff) {
        auto *dptr            = static_cast<double *>(lammps->extractGlobal("neigh_cutmax"));
        double max_bondcutoff = (dptr) ? *dptr : 0.0;
        double new_bondcutoff = cutoff->text().toDouble();

        if ((max_bondcutoff > 0.1) && (new_bondcutoff > max_bondcutoff))
            new_bondcutoff = max_bondcutoff;
        if (new_bondcutoff > 0.1) bondcutoff = new_bondcutoff;

        cutoff->setText(QString::number(bondcutoff));
    }
    createImage();
}

void ImageViewer::toggleBox()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    showbox = !showbox;
    button->setChecked(showbox);
    createImage();
}

void ImageViewer::toggleAxes()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) return;
    showaxes = !showaxes;
    button->setChecked(showaxes);
    createImage();
}

void ImageViewer::doZoomIn()
{
    zoom = zoom * 1.1;
    zoom = std::min(zoom, ZOOM_MAX);
    createImage();
}

void ImageViewer::doZoomOut()
{
    zoom = zoom / 1.1;
    zoom = std::max(zoom, ZOOM_MIN);
    createImage();
}

void ImageViewer::doRotLeft()
{
    vrot += 10;
    if (vrot < -180) vrot += 360;
    createImage();
}

void ImageViewer::doRotRight()
{
    vrot -= 10;
    if (vrot > 180) vrot -= 360;
    createImage();
}

void ImageViewer::doRotDown()
{
    hrot -= 10;
    if (hrot < 0) hrot += 360;
    createImage();
}

void ImageViewer::doRotUp()
{
    hrot += 10;
    if (hrot > 360) hrot -= 360;
    createImage();
}

void ImageViewer::doRecenter()
{
    QString commands = QString("variable LAMMPSGUI_CX delete\n"
                               "variable LAMMPSGUI_CY delete\n"
                               "variable LAMMPSGUI_CZ delete\n"
                               "variable LAMMPSGUI_CX equal (xcm(%1,x)-xlo)/lx\n"
                               "variable LAMMPSGUI_CY equal (xcm(%1,y)-ylo)/ly\n"
                               "variable LAMMPSGUI_CZ equal (xcm(%1,z)-zlo)/lz\n")
                           .arg(group);
    lammps->commandsString(commands);
    xcenter = lammps->extractVariable("LAMMPSGUI_CX");
    ycenter = lammps->extractVariable("LAMMPSGUI_CY");
    zcenter = lammps->extractVariable("LAMMPSGUI_CZ");
    if (lammps->extractSetting("dimension") == 2) zcenter = 0.0;
    lammps->commandsString("variable LAMMPSGUI_CX delete\n"
                           "variable LAMMPSGUI_CY delete\n"
                           "variable LAMMPSGUI_CZ delete\n");
    createImage();
}

void ImageViewer::cmdToClipboard()
{
    // Compose a reusable `dump`/`dump_modify` pair from the two argument strings
    // assembled for the last render, so no re-parsing of a joined command string
    // is needed. The render uses a one-shot WRITE_DUMP; here we emit a persistent
    // "viz" dump every 100 steps with zero-padded frame numbers instead.
    QString dumpargs = last_dumpargs;

    QString out;
    // When coloring bonds by a per-bond value, the render args reference a
    // `compute bond/local`; emit it first so the copied snippet is self-contained.
    // The id is lowercase (we don't want to model uppercase ids for users) and
    // long and tied to the dump id ("viz") to make a clash with an existing
    // compute unlikely.
    if (bondByValueActive()) {
        out += "compute bond_dump_viz " + group + " bond/local " + bondcolor + '\n';
        dumpargs.replace("c_" + bondComputeId, "c_bond_dump_viz");
    }

    QString imagefile = "myimage-*.ppm";
    if (lammps->configHasPngSupport())
        imagefile = "myimage-*.png";
    else if (lammps->configHasJpegSupport())
        imagefile = "myimage-*.jpg";

    out += "dump viz " + group + " image 100 " + imagefile + dumpargs + '\n';
    out += "dump_modify viz pad 9" + last_modifyargs + '\n';

#if QT_CONFIG(clipboard)
    auto *clip = QGuiApplication::clipboard();
    if (clip) {
        clip->setText(out, QClipboard::Clipboard);
        if (clip->supportsSelection()) clip->setText(out, QClipboard::Selection);
    } else
        fprintf(stderr, "# customized dump image command:\n%s", qPrintable(out));
#else
    fprintf(stderr, "# customized dump image command:\n%s", qPrintable(out));
#endif
}

void ImageViewer::resetColors()
{
    color_list.clear();
    color_list   = deftypecolors;
    ambientlight = 0.0;
    keylight     = 0.9;
    filllight    = 0.45;
    backlight    = 0.9;
}

void ImageViewer::loadColors()
{
    auto root = loadJsonColors(this);
    if (root.isEmpty()) return;

    auto arr = root["colors"].toArray();
    if (arr.isEmpty()) return;

    color_list.clear();
    for (const auto &item : arr) {
        auto obj  = item.toObject();
        QString n = obj.value("name").toString();
        double r  = std::clamp(obj.value("red").toDouble(1.0), 0.0, 1.0);
        double g  = std::clamp(obj.value("green").toDouble(1.0), 0.0, 1.0);
        double b  = std::clamp(obj.value("blue").toDouble(1.0), 0.0, 1.0);
        color_list.append({n, QColor::fromRgbF(r, g, b)});
    }
    if (color_list.isEmpty()) resetColors();

    auto lights = root.value("lights").toObject();
    if (!lights.isEmpty()) {
        ambientlight = lights.value("ambient").toDouble();
        keylight     = lights.value("key").toDouble();
        filllight    = lights.value("fill").toDouble();
        backlight    = lights.value("back").toDouble();
    }
    createImage();
}

void ImageViewer::saveColors()
{
    QJsonArray colors;
    for (const auto &c : color_list) {
        QJsonObject obj;
        obj["name"]  = c.first;
        obj["red"]   = c.second.redF();
        obj["green"] = c.second.greenF();
        obj["blue"]  = c.second.blueF();
        colors.append(obj);
    }
    QJsonObject lights;
    lights["ambient"] = ambientlight;
    lights["key"]     = keylight;
    lights["fill"]    = filllight;
    lights["back"]    = backlight;

    saveJsonColors(this, colors, lights);
}

void ImageViewer::changeGroup(int)
{
    auto *box = findChild<QComboBox *>("group");
    group     = box ? box->currentText() : "all";

    // reset molecule to "none" when changing group
    box = findChild<QComboBox *>("molecule");
    if (box && (box->currentIndex() > 0)) {
        box->setCurrentIndex(0); // triggers call to createImage()
    } else {
        createImage();
    }
}

void ImageViewer::changeMolecule(int)
{
    auto *box = findChild<QComboBox *>("molecule");
    molecule  = box ? box->currentText() : "none";

    box = findChild<QComboBox *>("group");
    if (molecule == "none") {
        box->setEnabled(true);
    } else {
        box->setEnabled(false);
    }

    createImage();
}

// intercept events
bool ImageViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        // don't handle any more key press events after entering destructor
        if (shutdown) return false;
        QKeyEvent *kev = static_cast<QKeyEvent *>(event);
        if ((kev->key() == Qt::Key_G) && (kev->modifiers() == Qt::AltModifier)) {
            auto *box = findChild<QComboBox *>("molecule");
            if (box) box->hidePopup();

            box = findChild<QComboBox *>("group");
            if (box) {
                box->setFocus();
                box->showPopup();
                return true;
            } else
                return false;
        } else if ((kev->key() == Qt::Key_M) && (kev->modifiers() == Qt::AltModifier)) {
            auto *box = findChild<QComboBox *>("group");
            if (box) box->hidePopup();

            box = findChild<QComboBox *>("molecule");
            if (box) {
                box->setFocus();
                box->showPopup();
                return true;
            } else
                return false;
        } else if ((kev->key() == Qt::Key_W) && (kev->modifiers() == Qt::AltModifier)) {
            auto *combo = findChild<QComboBox *>("molecule");
            if (combo) combo->hidePopup();
            combo = findChild<QComboBox *>("group");
            if (combo) combo->hidePopup();

            auto *box = findChild<QSpinBox *>("xsize");
            if (box) {
                box->setFocus();
                box->selectAll();
                return true;
            } else
                return false;
        } else if ((kev->key() == Qt::Key_H) && (kev->modifiers() == Qt::AltModifier)) {
            auto *combo = findChild<QComboBox *>("molecule");
            if (combo) combo->hidePopup();
            combo = findChild<QComboBox *>("group");
            if (combo) combo->hidePopup();

            auto *box = findChild<QSpinBox *>("ysize");
            if (box) {
                box->setFocus();
                box->selectAll();
                return true;
            } else
                return false;
        } else if (kev->modifiers() == Qt::AltModifier) {
            auto *combo = findChild<QComboBox *>("molecule");
            if (combo) combo->hidePopup();
            combo = findChild<QComboBox *>("group");
            if (combo) combo->hidePopup();
            setFocus();
        }
    }
    return QDialog::eventFilter(watched, event);
}

// Collect all widget state and LAMMPS-derived data required to assemble the
// dump-image command into a plain struct, so the command itself is built by
// the pure (GUI-free, testable) buildDumpImageCommand().  As a side effect
// this updates the useelements/usediameter/usesigma/atomcolor members that the
// settings dialogs and syncAtomSizeWidgets() rely on.
DumpImageParams ImageViewer::gatherDumpImageParams(const QString &dumpfilename)
{
    DumpImageParams p;

    p.group    = group;
    p.dumpfile = dumpfilename;

    // determine elements from masses and set their covalent radii
    const int ntypes       = lammps->extractSetting("ntypes");
    const int nbondtypes   = lammps->extractSetting("nbondtypes");
    auto *masses           = static_cast<double *>(lammps->extractAtom("mass"));
    const char *pair_style = static_cast<const char *>(lammps->extractGlobal("pair_style"));
    QString units          = static_cast<const char *>(lammps->extractGlobal("units"));
    QString elements{"element "};
    QString adiams;

    // detect if we can use element information, otherwise set atom color to "type" by default
    useelements = false;
    if (masses && ((units == "real") || (units == "metal"))) {
        useelements = true;
        if (!atomcustom) atomcolor = "element";
        for (int i = 1; i <= ntypes; ++i) {
            int idx = get_pte_from_mass(masses[i]);
            if (idx == 0) useelements = false;
            elements += QString(pte_label[idx]) + blank;
            adiams += QString("adiam %1 %2 ").arg(i).arg(vdwfactor * pte_vdw_radius[idx]);
        }
    } else {
        if (!atomcustom) atomcolor = "type";
    }

    usediameter = lammps->extractSetting("radius_flag") != 0;
    usesigma    = false;
    // if we cannot use element info or diameter data,
    // try to extract a number from the pair style, e.g. the Lennard-Jones sigma for radius
    if (!useelements && !usediameter && pair_style) {
        if ((strncmp(pair_style, "lj/", 3) == 0) || (strncmp(pair_style, "born/", 5) == 0) ||
            (strncmp(pair_style, "mie/", 4) == 0)) {
            auto **sigma = static_cast<double **>(lammps->extractPair("sigma"));
            if (sigma) {
                usesigma = true;
                for (int i = 1; i <= ntypes; ++i) {
                    if (sigma[i][i] > 0.0)
                        adiams += QString("adiam %1 %2 ").arg(i).arg(vdwfactor * sigma[i][i]);
                }
            }
        } else if ((strncmp(pair_style, "morse", 5) == 0) || (strncmp(pair_style, "nm/", 3) == 0)) {
            auto **r0 = static_cast<double **>(lammps->extractPair("r0"));
            if (r0) {
                usesigma = true;
                for (int i = 1; i <= ntypes; ++i) {
                    if (r0[i][i] > 0.0)
                        adiams += QString("adiam %1 %2 ").arg(i).arg(vdwfactor * r0[i][i]);
                }
            }
        } else if (strncmp(pair_style, "colloid", 7) == 0) {
            auto **diameter = static_cast<double **>(lammps->extractPair("d1"));
            auto **sigma    = static_cast<double **>(lammps->extractPair("sigma"));
            if (diameter && sigma) {
                usesigma = true;
                for (int i = 1; i <= ntypes; ++i) {
                    double diamval = sigma[i][i] + 0.5 * diameter[i][i];
                    if (diamval > 0.0)
                        adiams += QString("adiam %1 %2 ").arg(i).arg(vdwfactor * diamval);
                }
            }
        }
    }

    // resolve the final adiams string depending on the atom-size handling; this
    // mirrors the show/hide decisions made in syncAtomSizeWidgets()
    if (showatoms) {
        if (atomcustom && atomdiam.startsWith("v_")) {
            // an atom-style variable provides the per-atom diameters directly,
            // so the per-type dump_modify adiam settings are not used
            adiams.clear();
        } else if (atomcustom && (atomdiam != "element") && (atomdiam != "diameter") &&
                   (atomdiam != "sigma")) {
            adiams.clear();
            for (int i = 1; i <= ntypes; ++i)
                adiams += QString("adiam %1 %2 ").arg(i).arg(vdwfactor * atomSize);
        }
    } else {
        adiams.clear();
    }

    // LAMMPS-derived state
    p.ntypes       = ntypes;
    p.nbondtypes   = nbondtypes;
    p.elements     = elements;
    p.adiams       = adiams;
    p.useelements  = useelements;
    p.usediameter  = usediameter;
    p.usesigma     = usesigma;
    p.haspairstyle = pair_style && (strcmp(pair_style, "none") != 0);

    p.body_flag      = lammps->extractSetting("body_flag");
    p.line_flag      = lammps->extractSetting("line_flag");
    p.tri_flag       = lammps->extractSetting("tri_flag");
    p.ellipsoid_flag = lammps->extractSetting("ellipsoid_flag");
    p.bond_flag      = lammps->extractSetting("bond_flag");
    p.dimension      = lammps->extractSetting("dimension");
    p.version        = lammps->version();

    // atom appearance
    p.atomcustom = atomcustom;
    p.showatoms  = showatoms;
    p.atomcolor  = atomcolor;
    p.atomdiam   = atomdiam;
    p.vdwfactor  = vdwfactor;
    p.atomSize   = atomSize;

    // shaped particles
    p.showbodies     = showbodies;
    p.bodycolor      = bodycolor;
    p.bodydiam       = bodydiam;
    p.bodyflag       = bodyflag;
    p.showlines      = showlines;
    p.linecolor      = linecolor;
    p.linediam       = linediam;
    p.showtris       = showtris;
    p.tricolor       = tricolor;
    p.triflag        = triflag;
    p.tridiam        = tridiam;
    p.showellipsoids = showellipsoids;
    p.ellipsoidcolor = ellipsoidcolor;
    p.ellipsoidflag  = ellipsoidflag;
    p.ellipsoidlevel = ellipsoidlevel;
    p.ellipsoiddiam  = ellipsoiddiam;

    // bonds: a "compute bond/local" attribute name selects bond color-by-value,
    // in which case the emitted bond color is a reference to the managed compute.
    // A stale or saved bond/local selection that no longer applies (no real bonds
    // or AutoBonds active) falls back to coloring bonds by atom.
    const bool bondvalue = bondByValueActive();
    p.showbonds          = showbonds;
    p.bondbyvalue        = bondvalue;
    p.bondcolor          = bondvalue                            ? ("c_" + bondComputeId)
                           : bondLocalAttrs.contains(bondcolor) ? QStringLiteral("atom")
                                                                : bondcolor;
    p.bonddiam           = bonddiam;
    p.autobond           = autobond;
    p.bondcutoff         = bondcutoff;

    // view / image
    p.xsize       = xsize;
    p.ysize       = ysize;
    p.zoom        = zoom;
    p.shinyfactor = shinyfactor;
    p.antialias   = antialias;
    p.hrot        = hrot;
    p.vrot        = vrot;
    p.usessao     = usessao;
    p.ssaoval     = ssaoval;
    p.ssaosamples = ssaosamples;

    // depth cueing / outlines / specular preset
    p.usedepthcue    = usedepthcue;
    p.depthcuefactor = depthcuefactor;
    p.depthcuecolor  = depthcuecolor;
    p.depthcuestart  = depthcuestart;
    p.usedefocus     = usedefocus;
    p.defocusfactor  = defocusfactor;
    p.defocusstart   = defocusstart;
    p.useoutline     = useoutline;
    p.outlinewidth   = outlinewidth;
    p.outlinecolor   = outlinecolor;
    p.specular       = specular;
    p.usemetal       = usemetal;
    p.metalfactor    = metalfactor;
    p.metalfinish    = metalfinish;

    // box / axes
    p.showbox    = showbox;
    p.boxdiam    = boxdiam;
    p.showsubbox = showsubbox;
    p.subboxdiam = subboxdiam;
    p.showaxes   = showaxes;
    p.axesloc    = axesloc;
    p.axeslen    = axeslen;
    p.axesdiam   = axesdiam;

    // view center
    p.dynamiccenter = dynamiccenter;
    p.xcenter       = xcenter;
    p.ycenter       = ycenter;
    p.zcenter       = zcenter;

    // camera up direction
    p.xup = xup;
    p.yup = yup;
    p.zup = zup;

    // colors / lighting
    p.color_list   = color_list;
    p.boxcolor     = boxcolor;
    p.backcolor    = backcolor;
    p.backcolor2   = backcolor2;
    p.usegradient  = usegradient;
    p.axestrans    = axestrans;
    p.boxtrans     = boxtrans;
    p.atomtrans    = atomtrans;
    p.bondtrans    = bondtrans;
    p.ambientlight = ambientlight;
    p.keylight     = keylight;
    p.filllight    = filllight;
    p.backlight    = backlight;
    p.gammaval     = gammaval;

    // colormap
    p.colormap        = colormap;
    p.mapmin          = mapmin;
    p.mapmax          = mapmax;
    p.revcolormap     = revcolormap;
    p.bondcolormap    = bondcolormap;
    p.bondmapmin      = bondmapmin;
    p.bondmapmax      = bondmapmax;
    p.revbondcolormap = revbondcolormap;

    // regions / fixes / computes (non-owning pointer copies)
    p.computes = computes;
    p.fixes    = fixes;
    p.regions  = regions;

    return p;
}

// Show or hide the atom-size line edit and label and enable the VDW button to
// match the resolved element/diameter/sigma state.  This is the GUI-only
// counterpart to the adiams handling in gatherDumpImageParams() and must be
// called after it so the use* members are up to date.
void ImageViewer::syncAtomSizeWidgets()
{
    auto *edit   = findChild<QLineEdit *>("atomSize");
    auto *label  = findChild<QLabel *>("AtomLabel");
    auto *button = findChild<QPushButton *>("vdw");
    if (!(edit && label && button)) return;

    button->setEnabled(true);

    bool showsize;
    if (!showatoms) {
        showsize = false;
    } else if (atomcustom) {
        showsize = (atomdiam != "element") && (atomdiam != "diameter") && (atomdiam != "sigma") &&
                   !atomdiam.startsWith("v_");
    } else {
        showsize = !(useelements || usediameter || usesigma);
    }

    edit->setEnabled(showsize);
    label->setEnabled(showsize);
    if (showsize) {
        edit->show();
        label->show();
    } else {
        edit->hide();
        label->hide();
    }
}

// This function creates a visualization of the current system using the
// "dump image" command and reads and displays the rendered image.
// To visualize molecules we create new atoms with create_atoms and
// put them into a new, temporary group and then visualize that group.
// After rendering the image, the atoms and group are deleted.
// To update bond data, we also need to issue a "run 0" command.
void ImageViewer::createImage()
{
    // no point in trying to update the image when triggered after the destructor started
    if (shutdown) return;

    auto *renderstatus = findChild<QLabel *>("renderstatus");
    if (renderstatus) renderstatus->setPixmap(renderstatus->property("activePix").value<QPixmap>());
    repaint();
#if defined(Q_OS_MACOS)
    // Workaround for macOS: while the main thread is busy with the synchronous
    // render below, the backing store is not flushed to the screen, so repaint()
    // alone leaves the render-status icon stuck on its idle (gray) pixmap. Pump
    // the event loop once -- excluding user input so this slot is not re-entered
    // -- to push the active (colored) pixmap to the display.
    if (renderstatus) renderstatus->repaint();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#endif

    // The stop button halts a run via a walltime timeout whose state persists and
    // makes any later "run" exit immediately (run.cpp: if (timer->is_timeout())
    // return), so our render "run 0" would silently produce nothing. Reset it on
    // every render (cheap); this also covers a run started and stopped while the
    // viewer stays open.
    {
        StdoutSilencer guard;
        lammps->command("timer timeout off");
    }

    QString oldgroup = group;
    if (molecule != "none") {
        // get center of box
        double *boxlo, *boxhi, xmid, ymid, zmid;
        boxlo = static_cast<double *>(lammps->extractGlobal("boxlo"));
        boxhi = static_cast<double *>(lammps->extractGlobal("boxhi"));
        if (boxlo && boxhi) {
            xmid = 0.5 * (boxhi[0] + boxlo[0]);
            ymid = 0.5 * (boxhi[1] + boxlo[1]);
            zmid = 0.5 * (boxhi[2] + boxlo[2]);
        } else {
            xmid = ymid = zmid = 0.0;
        }

        {
            StdoutSilencer guard;
            QString molcreate = QString("create_atoms 0 single %1 %2 %3 mol %4 ") +
                                QString::number(Cfg::CREATE_ATOMS_SEED) + " group %5 units box";
            group = "imgviewer_tmp_mol";
            lammps->command(molcreate.arg(xmid).arg(ymid).arg(zmid).arg(molecule).arg(group));
            lammps->command(QString("neigh_modify exclude group all %1").arg(group));
            lammps->command("run 0 post no");
        }
        if (lammps->hasError()) (void)lammps->lastErrorMessage(); // clear pending error
    }

    // attempt to clean up if a previous render left our dump defined
    lammps->command("if $(is_defined(dump," + renderdumpid + ")) then 'undump " + renderdumpid +
                    "'");

    // (re)create the per-bond coloring compute when bond color-by-value applies
    // (a bond/local attribute, real bonds, AutoBonds off); the explicit dump +
    // run 0 below initializes it (write_dump's dump->init()+write() never runs
    // modify->init()). clear any leftover first.
    const bool bondvalue = bondByValueActive();
    lammps->command(QString("if $(is_defined(compute,%1)) then 'uncompute %1'").arg(bondComputeId));
    if (bondvalue)
        lammps->command(
            QString("compute %1 %2 bond/local %3").arg(bondComputeId, group, bondcolor));

    QDir dumpdir(QDir::tempPath());
    QFile dumpfile(dumpdir.absoluteFilePath(filename + ".ppm"));

    // gather parameters (also refreshes use* members), sync the atom-size widgets,
    // and assemble the dump and dump_modify argument strings
    DumpImageParams params = gatherDumpImageParams(dumpfile.fileName());
    syncAtomSizeWidgets();
    const DumpImageCommand exportcmds = buildDumpImageCommand(params);
    last_dumpargs                     = exportcmds.dumpargs;
    last_modifyargs                   = exportcmds.modifyargs;

    // with the SSAO Samples setting on "auto" interactive renders use a fixed,
    // low sample count so the viewer stays responsive, while the exported
    // command (cmdToClipboard) keeps deriving the count from the SSAO strength;
    // an explicitly requested count is honored also here, so high-quality
    // images can be saved straight from the viewer
    if (params.ssaosamples == 0) params.ssaosamples = Cfg::SSAO_VIEW_SAMPLES;
    const DumpImageCommand cmds = buildDumpImageCommand(params);

    // Render with an explicit dump + run 0 rather than write_dump: the run does a
    // real modify->init(), which initializes any compute the image references
    // (e.g. the per-bond compute). dump image needs a '*' in the file name (it is
    // replaced by the timestep) and "first yes" forces the single frame.
    const QString starfile = dumpdir.absoluteFilePath(filename + ".*.ppm");

    // A surviving "fix graphics/labels ... colorscale <id>" requires a dump image
    // named <id> to still exist, but LammpsGui::renderImage purges the deck's dumps
    // before opening the viewer, so the run 0 below would abort in the fix's init().
    // The fix only *displays* that dump's color map (it does not define one), so we
    // satisfy the dependency by naming our own render dump after the missing id: the
    // fix then finds a valid "dump image" and the render proceeds. The id is read
    // from the specific error and cached in renderdumpid, so the one-shot retry only
    // happens on the first affected render.
    static const QRegularExpression colorscaleErr(
        QStringLiteral(R"(Dump ID (\S+) for colorscale not found)"));
    static const QRegularExpression neighMultiErr(
        QStringLiteral("Cannot use comm mode multi without multi-style neighbor lists"));
    QString dumpid = renderdumpid;
    QString errmsg;
    for (int attempt = 0; attempt < 2; ++attempt) {
        {
            StdoutSilencer guard;
            lammps->command("dump " + dumpid + " " + group + " image 1 '" + starfile + "'" +
                            cmds.dumpargs);
            lammps->command("dump_modify " + dumpid + " first yes pad 0" + cmds.modifyargs);
            lammps->command("run 0 post no");
            lammps->command("undump " + dumpid);
        }
        errmsg              = lammps->lastErrorMessage();
        const auto colmatch = colorscaleErr.match(errmsg);
        // retry once under the missing colorscale dump id (unless we already use it)
        if (colmatch.hasMatch() && (colmatch.captured(1) != dumpid)) {
            dumpid = colmatch.captured(1);
            StdoutSilencer guard;
            lammps->command("if $(is_defined(dump," + dumpid + ")) then 'undump " + dumpid + "'");
            continue;
        }
        const auto neighmatch = neighMultiErr.match(errmsg);
        // retry once more after turning off comm_modify multi
        if (neighmatch.hasMatch()) {
            StdoutSilencer guard;
            lammps->command("comm_modify mode single");
            continue;
        }
        break;
    }
    // cache the working dump id only on success, so a deck with several distinct
    // colorscale dumps (which a single render dump cannot satisfy) does not oscillate
    if (errmsg.isEmpty()) renderdumpid = dumpid;
    const auto step = static_cast<long long>(lammps->getThermo("step"));
    const QString imagepath =
        dumpdir.absoluteFilePath(QString("%1.%2.ppm").arg(filename).arg(step));

    // the per-bond compute has done its job for this frame; remove it again
    if (bondvalue) {
        StdoutSilencer guard;
        lammps->command("uncompute " + bondComputeId);
    }

    // restore the pre-render state on every exit path: remove the per-step
    // frame file(s) this render produced (also on the error paths, so frames
    // written before a failure do not accumulate in the temporary directory),
    // remove the temporary molecule atoms/group created above, and reset the
    // render-status icon, otherwise a failed render leaves the icon stuck on
    // "active" and the leftover atoms corrupt every subsequent render
    auto restoreRenderState = [&]() {
        for (const auto &f : dumpdir.entryList({filename + ".*.ppm"}, QDir::Files))
            QFile::remove(dumpdir.absoluteFilePath(f));
        if (molecule != "none") {
            lammps->command("neigh_modify exclude none");
            lammps->command(QString("delete_atoms group %1 compress no").arg(group));
            lammps->command(QString("group %1 delete").arg(group));
            group = oldgroup;
        }
        if (renderstatus)
            renderstatus->setPixmap(renderstatus->property("idlePix").value<QPixmap>());
    };

    // display error message
    if (!errmsg.isEmpty()) {
        restoreRenderState();
        // ignore "Invalid LAMMPS handle", but report other errors
        if (!errmsg.contains("Invalid LAMMPS handle"))
            warning(this, "Image Viewer File Creation Error",
                    "LAMMPS failed to create the image:", QString("<code>%1</code>").arg(errmsg));
        return;
    }

    QImageReader reader(imagepath);
    reader.setAutoTransform(true);
    const QImage newImage = reader.read();

    // read of new image failed. nothing left to do.
    if (newImage.isNull()) {
        restoreRenderState();
        return;
    }

    // show image
    image = newImage;
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->setMinimumSize(image.width(), image.height());
    imageLabel->resize(image.width(), image.height());
    adjustWindowSize();
    restoreRenderState();
    repaint();
    updateActions();
}

void ImageViewer::saveAs()
{
    exportImage(this, &image, "ImageViewer", defaultFileStem(filename) + ".png");
}

void ImageViewer::copy()
{
#if QT_CONFIG(clipboard)
    auto *clip = QGuiApplication::clipboard();
    if (clip && !image.isNull()) {
        clip->setImage(image, QClipboard::Clipboard);
        if (clip->supportsSelection()) clip->setImage(image, QClipboard::Selection);
    } else
        fprintf(stderr, "Copy image to clipboard currently not available\n");
#else
    fprintf(stderr, "Copy image to clipboard not supported on this platform\n");
#endif
}

void ImageViewer::quit()
{
    if (lammpsgui) lammpsgui->quit();
}

void ImageViewer::getHelp()
{
    auto *src = sender();
    if (src) {
        QString page   = src->objectName();
        QString docver = "/";
        if (lammps) {
            QString git_branch = static_cast<const char *>(lammps->extractGlobal("git_branch"));
            if ((git_branch == "stable") || (git_branch == "maintenance")) {
                docver = "/stable/";
            } else if (git_branch == "release") {
                docver = "/";
            } else {
                docver = "/latest/";
            }
        }

        if (page == "visualization.html") {
            // lammps-gui docs
            QDesktopServices::openUrl(QUrl(QString("https://lammps-gui.lammps.org/%1").arg(page)));
        } else if (page.startsWith("compute_") || page.startsWith("fix_")) {
            // jump to the "Dump image info" section for computes and fixes
            QDesktopServices::openUrl(
                QUrl(QString("%1%2%3#dump-image-info").arg(Cfg::DOCS_URL, docver, page)));
        } else {
            // general LAMMPS doc page
            QDesktopServices::openUrl(QUrl(QString("%1%2%3").arg(Cfg::DOCS_URL, docver, page)));
        }
    }
}

void ImageViewer::createActions()
{
    QMenu *fileMenu = new QMenu("&File", this);
    fileMenu->setObjectName(Cfg::VIEW_FILE_MENU);
    installViewMenuBar(menuBar, fileMenu, lammpsgui ? lammpsgui->sharedMenus() : QList<QMenu *>());

    saveAsAct = addMenuAction(fileMenu, "&Save As...", ":/icons/document-save-as.svg", this,
                              &ImageViewer::saveAs);
    saveAsAct->setEnabled(false);
    scopeShortcut(this, saveAsAct, QKeySequence(Qt::CTRL | Qt::Key_S));
    fileMenu->addSeparator();
    copyAct =
        addMenuAction(fileMenu, "Copy &Image", ":/icons/edit-copy.svg", this, &ImageViewer::copy);
    scopeShortcut(this, copyAct, QKeySequence(QKeySequence::Copy));
    copyAct->setEnabled(false);
    cmdAct = addMenuAction(fileMenu, "Copy &dump image command", ":/icons/file-clipboard.svg", this,
                           &ImageViewer::cmdToClipboard);
    scopeShortcut(this, cmdAct, QKeySequence(Qt::CTRL | Qt::Key_D));
    fileMenu->addSeparator();
    addMenuAction(fileMenu, "&Load Colors from JSON...", ":/icons/document-open.svg", this,
                  &ImageViewer::loadColors);
    addMenuAction(fileMenu, "S&ave Colors to JSON...", ":/icons/document-save.svg", this,
                  &ImageViewer::saveColors);
    addMenuAction(fileMenu, "&Reset Colors", ":/icons/system-restart.svg", this, [this]() {
        resetColors();
        createImage();
    });
    fileMenu->addSeparator();
    scopeShortcut(
        this, addMenuAction(fileMenu, "&Close", ":/icons/window-close.svg", this, &QWidget::close),
        QKeySequence(Qt::CTRL | Qt::Key_W));
    scopeShortcut(
        this,
        addMenuAction(fileMenu, "&Quit", ":/icons/application-exit.svg", this, &ImageViewer::quit),
        QKeySequence(Qt::CTRL | Qt::Key_Q));
}

void ImageViewer::updateActions()
{
    saveAsAct->setEnabled(!image.isNull());
    copyAct->setEnabled(!image.isNull());
}

void ImageViewer::adjustWindowSize()
{
    // A docked panel is sized by its dock area.  Fitting the window around the
    // image resizes this widget, and that request travels up through the dock to
    // the main window -- which then jumps about as images are loaded.
    if (dockedLayout()) return;

    // the render size is set in the settings panel, so the size to fit is
    // known even before the first image has been rendered
    if ((xsize < 1) || (ysize < 1)) return;

    // make sure the scroll area is not resized beyond a certain fraction of the screen
    const QSize avail = screen()->availableSize();
    const QSize budget((avail.width() * 4 / 5) - EXTRA_WIDTH,
                       (avail.height() * 9 / 10) - EXTRA_HEIGHT);
    lastFitSize = fitViewerWindow(this, scrollArea, QSize(xsize, ysize), budget, lastFitSize);
}

void ImageViewer::resetWindowSize()
{
    // discard both a manual window resize and the memoized fit
    lastFitSize = QSize();
    adjustWindowSize();
}

void ImageViewer::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // any fit computed while the window was hidden used unpolished style
    // metrics and was not memoized (see fitViewerWindow()); apply the fit
    // again as soon as the shown window has settled
    if (!lastFitSize.isValid()) QTimer::singleShot(0, this, &ImageViewer::adjustWindowSize);
}

void ImageViewer::updatePeratom()
{
    atom_properties.clear();
    if (useelements) atom_properties << "element";
    atom_properties << "type";

    if (lammps) {
        StdoutSilencer guard;

        if (lammps->extractSetting("molecule_flag")) atom_properties << "mol";
        if (lammps->extractSetting("q_flag")) atom_properties << "q";
        if (lammps->extractSetting("mu_flag")) atom_properties << "mu";
        if (lammps->extractSetting("radius_flag")) atom_properties << "diameter";

        void *ptr = nullptr;
        int type  = 0;

        // add atom style variables to the list
        int num = lammps->idCount("variable");
        for (int idx = 0; idx < num; ++idx) {
            const QString name = lammps->idName("variable", idx);
            type               = lammps->extractVariableDatatype(name);
            if (type == LammpsWrapper::ATOM_STYLE) atom_properties << QString("v_%1").arg(name);
        }

        // add compatible computes to the list
        num = lammps->idCount("compute");
        for (int idx = 0; idx < num; ++idx) {
            const QString name = lammps->idName("compute", idx);
            ptr =
                lammps->extractCompute(name, LammpsWrapper::ATOM_STYLE, LammpsWrapper::VECTOR_TYPE);
            if (ptr) {
                atom_properties << QString("c_%1").arg(name);
                continue;
            }
            ptr =
                lammps->extractCompute(name, LammpsWrapper::ATOM_STYLE, LammpsWrapper::ARRAY_TYPE);
            if (ptr) {
                ptr  = lammps->extractCompute(name, LammpsWrapper::ATOM_STYLE,
                                              LammpsWrapper::NUM_COLS);
                type = *static_cast<int *>(ptr);
                for (int col = 1; col <= type; ++col) {
                    atom_properties << QString("c_%1[%2]").arg(name).arg(col);
                }
                continue;
            }

            // clear error status, if needed:
            (void)lammps->lastErrorMessage(); // read-and-clear any pending error
        }

        // add compatible fixes to the list
        num = lammps->idCount("fix");
        for (int idx = 0; idx < num; ++idx) {
            const QString name = lammps->idName("fix", idx);
            ptr = lammps->extractFix(name, LammpsWrapper::ATOM_STYLE, LammpsWrapper::ARRAY_TYPE, -1,
                                     -1);
            if (ptr) {
                ptr  = lammps->extractFix(name, LammpsWrapper::ATOM_STYLE, LammpsWrapper::NUM_COLS,
                                          -1, -1);
                type = *static_cast<int *>(ptr);
                if (type == 0) {
                    atom_properties << QString("f_%1").arg(name);
                } else {
                    for (int col = 1; col <= type; ++col) {
                        atom_properties << QString("f_%1[%2]").arg(name).arg(col);
                    }
                }
                continue;
            }

            // clear error status, if needed:
            (void)lammps->lastErrorMessage(); // read-and-clear any pending error
        }
    }
    // some more general dump custom properties
    atom_properties << "id" << "mass" << "x" << "y" << "z" << "vx" << "vy" << "vz" << "fx"
                    << "fy"
                    << "fz";
}

void ImageViewer::updateFixes()
{
    if (!lammps) return;

    // remove any fixes that no longer exist. to avoid inconsistencies while looping
    // over the fixes, we first collect the list of missing ids and then apply it.
    std::unordered_set<std::string> oldkeys;
    for (const auto &istyle : computes) {
        if (!lammps->hasId("compute", istyle.first.c_str())) oldkeys.insert(istyle.first);
    }
    for (const auto &id : oldkeys) {
        delete computes[id];
        computes.erase(id);
    }
    oldkeys.clear();
    for (const auto &istyle : fixes) {
        if (!lammps->hasId("fix", istyle.first.c_str())) oldkeys.insert(istyle.first);
    }
    for (const auto &id : oldkeys) {
        delete fixes[id];
        fixes.erase(id);
    }

    // map compute and fix styles to their ids by parsing info command output.
    StdCapture capturer;
    capturer.beginCapture();
    lammps->command("info computes fixes");
    capturer.endCapture();
    QString styleinfo(capturer.getCapture().c_str());
    QRegularExpression infoline(
        QStringLiteral(R"(^(Compute|Fix)\[.*\]: *([^,]+), *style = ([^,]+).*)"));
    QRegularExpression newline(QStringLiteral("[\r\n]+"));
    int i = 0;
    for (const auto &line : styleinfo.split(newline, Qt::SkipEmptyParts)) {
        auto match = infoline.match(line);
        if (match.hasMatch()) {
            auto id    = match.captured(2).toStdString();
            auto style = match.captured(3);
            if (match.captured(1) == "Compute") {
                if (image_computes.contains(style)) {
                    if (computes.count(id) == 0) {
                        const QString &color = defaultcolors[i % defaultcolors.size()];
                        computes[id] = new ImageInfo(false, style, TYPE, color, 1.0, 0.0, 0.0);
                        ++i;
                    } else {
                        computes[id]->style = style;
                    }
                }
            } else if (match.captured(1) == "Fix") {
                if (image_fixes.contains(style)) {
                    if (fixes.count(id) == 0) {
                        const QString &color = defaultcolors[i % defaultcolors.size()];
                        fixes[id] = new ImageInfo(false, style, TYPE, color, 1.0, 0.0, 0.0);
                        ++i;
                    } else {
                        fixes[id]->style = style;
                    }
                }
            }
        }
    }

    auto *button = findChild<QPushButton *>("image_styles");
    if (button) button->setEnabled((computes.size() + fixes.size()) > 0);
}

void ImageViewer::updateRegions()
{
    if (!lammps) return;

    // remove any regions that no longer exist. to avoid inconsistencies while looping
    // over the regions, we first collect the list of missing ids and then apply it.
    std::unordered_set<std::string> oldkeys;
    for (const auto &reg : regions) {
        if (!lammps->hasId("region", reg.first.c_str())) oldkeys.insert(reg.first);
    }
    for (const auto &id : oldkeys) {
        delete regions[id];
        regions.erase(id);
    }

    // add any new regions
    int nregions = lammps->idCount("region");
    for (int i = 0; i < nregions; ++i) {
        const QString name = lammps->idName("region", i);
        if (!name.isEmpty()) {
            std::string id = name.toStdString();
            if (regions.count(id) == 0) {
                const QString &color = defaultcolors[i % defaultcolors.size()];
                auto *reginfo        = new RegionInfo(false, FRAME, color, DEFAULT_DIAMETER,
                                                      DEFAULT_OPACITY, DEFAULT_NPOINTS);
                regions[id]          = reginfo;
            }
        }
    }

    auto *button = findChild<QPushButton *>("regions");
    if (button) button->setEnabled(regions.size() > 0);
}

bool ImageViewer::hasAutobonds()
{
    if (!lammps) return false;
    const auto *pair_style = static_cast<const char *>(lammps->extractGlobal("pair_style"));
    if (!pair_style) return false;
    return strcmp(pair_style, "none") != 0;
}

bool ImageViewer::bondByValueActive()
{
    // compute bond/local only works for real bonds: the atom style must support
    // bonds and AutoBonds (a distance search with no bond identities) must be off
    return bondLocalAttrs.contains(bondcolor) && !autobond &&
           (lammps->extractSetting("molecule_flag") == 1);
}

void ImageViewer::rebuildBondColorChoices(QComboBox *bncolor, bool allowByValue)
{
    if (!bncolor) return;
    const QString current = bncolor->currentText();
    bncolor->clear();
    bncolor->addItems({"atom", "type"});
    if (allowByValue) {
        bncolor->insertSeparator(bncolor->count());
        bncolor->addItems(bondLocalAttrs); // per-bond compute attributes -> color by value
    }
    // restore the previous selection, or fall back to "atom" if it is no longer
    // offered. Guard against an empty "current" (a freshly created, still-empty
    // combo): findText("") would match the inserted separator -- whose text is
    // empty -- and leave the box showing a blank selection instead of "atom".
    if (!current.isEmpty() && bncolor->findText(current) >= 0)
        selectComboItem(bncolor, current);
    else
        selectComboItem(bncolor, "atom");
}

// Local Variables:
// c-basic-offset: 4
// End:
