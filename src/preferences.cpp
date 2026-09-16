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

#include "preferences.h"

#include "codeeditor.h"
#include "commandwindow.h"
#include "constants.h"
#include "helpers.h"
#include "lammpsgui.h"
#include "lammpswrapper.h"
#include "logwindow.h"
#include "qaddon.h"
#include "tutorialwizard.h"
#include "urldownloader.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QFontInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QValidator>

Preferences::Preferences(LammpsWrapper *_lammps, LammpsGui *_lammpsgui, QWidget *parent) :
    QDialog(parent), tabWidget(new QTabWidget),
    buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel)),
    settings(new QSettings), lammps(_lammps), lammpsgui(_lammpsgui), needRelaunch(false)
{
    tabWidget->addTab(new GeneralTab(settings, lammps, lammpsgui), "&General Settings");
    tabWidget->addTab(new AcceleratorTab(settings, lammps), "&Accelerators");
    tabWidget->addTab(new SnapshotTab(settings), "&Snapshot Image");
    tabWidget->addTab(new EditorTab(settings), "&Editor Settings");
    tabWidget->addTab(new ChartsTab(settings), "Cha&rts Settings");

    styleDialogButtons(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &Preferences::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout;
    layout->addWidget(tabWidget);
    layout->addWidget(buttonBox);
    setLayout(layout);
    setWindowIcon(QIcon(Cfg::MAIN_ICON));
    setWindowTitle("LAMMPS-GUI - Preferences");
    resize(Cfg::PREFERENCES_WIDTH, Cfg::PREFERENCES_HEIGHT);
}

Preferences::~Preferences()
{
    delete buttonBox;
    delete tabWidget;
    delete settings;
}

namespace {
const QHash<QString, int> buttonToChoice = {
    {"none", AcceleratorTab::None},     {"opt", AcceleratorTab::Opt},
    {"openmp", AcceleratorTab::OpenMP}, {"intel", AcceleratorTab::Intel},
    {"kokkos", AcceleratorTab::Kokkos}, {"gpu", AcceleratorTab::Gpu}};

const QHash<QString, int> buttonToPrecision = {{"inteldouble", AcceleratorTab::Double},
                                               {"intelmixed", AcceleratorTab::Mixed},
                                               {"intelsingle", AcceleratorTab::Single}};
} // namespace

void Preferences::accept()
{
    // store all data in settings class
    // and then confirm accepting

    // store selected accelerator and precision settings from radiobuttons
    QList<QRadioButton *> allButtons = tabWidget->findChildren<QRadioButton *>();
    for (const auto &anyButton : allButtons) {
        if (anyButton->isChecked()) {
            const auto &button = anyButton->objectName();
            if (buttonToChoice.contains(button)) {
                settings->setValue(Keys::ACCELERATOR, buttonToChoice.value(button));
            } else if (buttonToPrecision.contains(button)) {
                settings->setValue(Keys::INTELPREC, buttonToPrecision.value(button));
            }
        }
    }

    QLineEdit *field;

    // store number of threads, reset to 1 for "None" and "Opt" settings
    field = tabWidget->findChild<QLineEdit *>("nthreads");
    if (field && lammpsgui) {
        int accel = settings->value(Keys::ACCELERATOR, AcceleratorTab::None).toInt();
        if ((accel == AcceleratorTab::None) || (accel == AcceleratorTab::Opt)) {
            lammpsgui->nthreads = 1;
        } else if (field->hasAcceptableInput()) {
            settings->setValue(Keys::NTHREADS, field->text());
            lammpsgui->nthreads = settings->value(Keys::NTHREADS, 1).toInt();
        }
    }

    // store setting for GPU package
    auto *box = tabWidget->findChild<QCheckBox *>("gpuneigh");
    if (box) settings->setValue(Keys::GPUNEIGH, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("gpupaironly");
    if (box) settings->setValue(Keys::GPUPAIRONLY, box->isChecked());

    // store image width, height, zoom, and rendering settings

    settings->beginGroup(Keys::GROUP_SNAPSHOT);
    field = tabWidget->findChild<QLineEdit *>("xsize");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::XSIZE, field->text());
    field = tabWidget->findChild<QLineEdit *>("ysize");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::YSIZE, field->text());
    field = tabWidget->findChild<QLineEdit *>("zoom");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::ZOOM, field->text());
    field = tabWidget->findChild<QLineEdit *>("hrot");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::HROT, field->text());
    field = tabWidget->findChild<QLineEdit *>("vrot");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::VROT, field->text());
    box = tabWidget->findChild<QCheckBox *>("anti");
    if (box) settings->setValue(Keys::ANTIALIAS, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("ssao");
    if (box) settings->setValue(Keys::SSAO, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("shiny");
    if (box) settings->setValue(Keys::SHINYSTYLE, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("box");
    if (box) settings->setValue(Keys::BOX, box->isChecked());
    field = tabWidget->findChild<QLineEdit *>("boxdiam");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::BOXDIAM, field->text());
    box = tabWidget->findChild<QCheckBox *>("axes");
    if (box) settings->setValue(Keys::AXES, box->isChecked());
    field = tabWidget->findChild<QLineEdit *>("axeslen");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::AXESLEN, field->text());
    field = tabWidget->findChild<QLineEdit *>("axesdiam");
    if (field)
        if (field->hasAcceptableInput()) settings->setValue(Keys::AXESDIAM, field->text());
    box = tabWidget->findChild<QCheckBox *>("vdwstyle");
    if (box) settings->setValue(Keys::VDWSTYLE, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("autobond");
    if (box) settings->setValue(Keys::AUTOBOND, box->isChecked());
    field = tabWidget->findChild<QLineEdit *>("bondcut");
    if (field) settings->setValue(Keys::BONDCUT, field->text());
    field = tabWidget->findChild<QLineEdit *>("backcolor");
    if (field && field->hasAcceptableInput()) settings->setValue(Keys::BACKCOLOR, field->text());
    field = tabWidget->findChild<QLineEdit *>("backcolor2");
    if (field && field->hasAcceptableInput()) settings->setValue(Keys::BACKCOLOR2, field->text());
    box = tabWidget->findChild<QCheckBox *>("usegradient");
    if (box) settings->setValue(Keys::USEGRADIENT, box->isChecked());
    field = tabWidget->findChild<QLineEdit *>("boxcolor");
    if (field && field->hasAcceptableInput()) settings->setValue(Keys::BOXCOLOR, field->text());
    settings->endGroup();

    // general settings
    box = tabWidget->findChild<QCheckBox *>("echo");
    if (box) settings->setValue(Keys::ECHO, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("cite");
    if (box) settings->setValue(Keys::CITE, box->isChecked());
    auto *radio = tabWidget->findChild<QRadioButton *>("layoutdocked");
    if (radio) {
        // the layout is applied when the main window is built, so a change
        // only takes effect in a fresh process
        if (radio->isChecked() != settings->value(Keys::DOCKED, false).toBool())
            setRelaunch(QString("The window layout was changed."));
        settings->setValue(Keys::DOCKED, radio->isChecked());
    }
    box = tabWidget->findChild<QCheckBox *>("maximized");
    if (box) settings->setValue(Keys::MAXIMIZED, box->isChecked());
    auto *shell = tabWidget->findChild<QComboBox *>("shell");
    if (shell) settings->setValue(Keys::SHELL, shell->currentText());
    box = tabWidget->findChild<QCheckBox *>("viewlog");
    if (box) settings->setValue(Keys::VIEWLOG, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("viewchart");
    if (box) settings->setValue(Keys::VIEWCHART, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("viewslide");
    if (box) settings->setValue(Keys::VIEWSLIDE, box->isChecked());

    settings->beginGroup(Keys::GROUP_TUTORIAL);
    box = tabWidget->findChild<QCheckBox *>("solution");
    if (box) settings->setValue(Keys::SOLUTION, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("webpage");
    if (box) settings->setValue(Keys::WEBPAGE, box->isChecked());
    settings->endGroup();

    auto *spin = tabWidget->findChild<QSpinBox *>("updfreq");
    if (spin) settings->setValue(Keys::UPDFREQ, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("updchart");
    if (spin) settings->setValue(Keys::UPDCHART, spin->value());

    field = tabWidget->findChild<QLineEdit *>("proxyval");
    if (field) settings->setValue(Keys::HTTPS_PROXY, field->text());
    spin = tabWidget->findChild<QSpinBox *>("downloadtimeout");
    if (spin) settings->setValue(Keys::DOWNLOAD_TIMEOUT, spin->value());

    // reformatting settings

    settings->beginGroup(Keys::GROUP_REFORMAT);
    spin = tabWidget->findChild<QSpinBox *>("cmdval");
    if (spin) settings->setValue(Keys::COMMAND, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("typeval");
    if (spin) settings->setValue(Keys::TYPE, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("idval");
    if (spin) settings->setValue(Keys::ID, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("nameval");
    if (spin) settings->setValue(Keys::NAME, spin->value());
    box = tabWidget->findChild<QCheckBox *>("retval");
    if (box) settings->setValue(Keys::RETURN, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("autoval");
    if (box) settings->setValue(Keys::AUTOMATIC, box->isChecked());
    box = tabWidget->findChild<QCheckBox *>("savval");
    if (box) settings->setValue(Keys::AUTOSAVE, box->isChecked());
    settings->endGroup();

    // stored outside the reformat group; also read by doRun()
    box = tabWidget->findChild<QCheckBox *>("lintcheck");
    if (box) settings->setValue(Keys::LINTCHECK, box->isChecked());

    // chart window settings

    settings->beginGroup(Keys::GROUP_CHARTS);
    field = tabWidget->findChild<QLineEdit *>("title");
    if (field) settings->setValue(Keys::TITLE, field->text());
    auto *combo = tabWidget->findChild<QComboBox *>("smoothchoice");
    if (combo) settings->setValue(Keys::SMOOTHCHOICE, combo->currentIndex());
    combo = tabWidget->findChild<QComboBox *>("rawbrush");
    if (combo) settings->setValue(Keys::RAWBRUSH, combo->currentIndex());
    combo = tabWidget->findChild<QComboBox *>("smoothbrush");
    if (combo) settings->setValue(Keys::SMOOTHBRUSH, combo->currentIndex());
    combo = tabWidget->findChild<QComboBox *>("errbrush");
    if (combo) settings->setValue(Keys::ERRBRUSH, combo->currentIndex());
    combo = tabWidget->findChild<QComboBox *>("rawmode");
    if (combo) settings->setValue(Keys::RAWMODE, combo->currentIndex());
    combo = tabWidget->findChild<QComboBox *>("smoothmode");
    if (combo) settings->setValue(Keys::SMOOTHMODE, combo->currentIndex());
    auto *dspin = tabWidget->findChild<QDoubleSpinBox *>("rawwidth");
    if (dspin) settings->setValue(Keys::RAWWIDTH, dspin->value());
    dspin = tabWidget->findChild<QDoubleSpinBox *>("smoothwidth");
    if (dspin) settings->setValue(Keys::SMOOTHWIDTH, dspin->value());
    dspin = tabWidget->findChild<QDoubleSpinBox *>("errwidth");
    if (dspin) settings->setValue(Keys::ERRWIDTH, dspin->value());
    dspin = tabWidget->findChild<QDoubleSpinBox *>("rawpointsize");
    if (dspin) settings->setValue(Keys::RAWPOINTSIZE, dspin->value());
    dspin = tabWidget->findChild<QDoubleSpinBox *>("smoothpointsize");
    if (dspin) settings->setValue(Keys::SMOOTHPOINTSIZE, dspin->value());
    spin = tabWidget->findChild<QSpinBox *>("smoothwindow");
    if (spin) settings->setValue(Keys::SMOOTHWINDOW, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("smoothorder");
    if (spin) settings->setValue(Keys::SMOOTHORDER, spin->value());
    auto *check = tabWidget->findChild<QCheckBox *>("grid");
    if (check) settings->setValue(Keys::GRID, check->isChecked());
    check = tabWidget->findChild<QCheckBox *>("minorgrid");
    if (check) settings->setValue(Keys::MINORGRID, check->isChecked());
    settings->endGroup();
    spin = tabWidget->findChild<QSpinBox *>("chartx");
    if (spin) settings->setValue(Keys::CHARTX, spin->value());
    spin = tabWidget->findChild<QSpinBox *>("charty");
    if (spin) settings->setValue(Keys::CHARTY, spin->value());

    // must come after all settings are stored: relaunchApplication() replaces
    // the process image, so nothing after it runs and pending changes must be
    // flushed to disk first
    if (needRelaunch) {
        if (relaunchReasons.isEmpty())
            relaunchReasons << QString("LAMMPS library plugin path was changed.");
        warning(this, "Relaunching LAMMPS-GUI", relaunchReasons.join(' '),
                "LAMMPS-GUI must be relaunched to activate it.");
        settings->sync();
        relaunchOrExit(this);
    }

    QDialog::accept();
}

GeneralTab::GeneralTab(QSettings *_settings, LammpsWrapper *_lammps, LammpsGui *_lammpsgui,
                       QWidget *parent) :
    QWidget(parent), settings(_settings), lammps(_lammps), lammpsgui(_lammpsgui)
{
    auto *layout = new QGridLayout;

    auto *echo = new QCheckBox("Echo input to output buffer");
    echo->setObjectName("echo");
    echo->setChecked(settings->value(Keys::ECHO, false).toBool());
    auto *cite = new QCheckBox("Include citation details");
    cite->setObjectName("cite");
    cite->setChecked(settings->value(Keys::CITE, false).toBool());
    auto *logv = new QCheckBox("Show Output window by default");
    logv->setObjectName("viewlog");
    logv->setChecked(settings->value(Keys::VIEWLOG, true).toBool());
    auto *pltv = new QCheckBox("Show Charts window by default");
    pltv->setObjectName("viewchart");
    pltv->setChecked(settings->value(Keys::VIEWCHART, true).toBool());
    auto *sldv = new QCheckBox("Show Slide Show window by default");
    sldv->setObjectName("viewslide");
    sldv->setChecked(settings->value(Keys::VIEWSLIDE, true).toBool());
    auto *maxi = new QCheckBox("Open main window maximized");
    maxi->setObjectName("maximized");
    maxi->setChecked(settings->value(Keys::MAXIMIZED, false).toBool());
    maxi->setToolTip("Start with the main window filling the screen instead of\n"
                     "restoring the size it had when it was last closed.\n"
                     "Only applies to the combined main window: with individual\n"
                     "windows the output windows would end up behind it.");
    // window layout: the first choice on the tab, on a line of its own
    const bool isdocked = settings->value(Keys::DOCKED, false).toBool();
    auto *winlayout     = new QRadioButton("Individual Windows");
    winlayout->setObjectName("layoutwindows");
    winlayout->setChecked(!isdocked);
    winlayout->setToolTip("Show the Output, Charts, Image, Slide Show and Variables views as\n"
                          "individual windows that are placed and stacked freely.");
    auto *docklayout = new QRadioButton("Combined Main Window");
    docklayout->setObjectName("layoutdocked");
    docklayout->setChecked(isdocked);
    docklayout->setToolTip("Show those views as panels docked around the editor inside the\n"
                           "main window: charts, image and slide show in a tabbed group on\n"
                           "the right, output and variables across the bottom.");
    auto *layoutgroup = new QButtonGroup(this);
    layoutgroup->addButton(winlayout);
    layoutgroup->addButton(docklayout);

    // opening maximized only makes sense for the combined window; with
    // individual windows a maximized main window covers the very views it is
    // supposed to sit beside, so the option is forced off there
    auto applyLayoutStyle = [maxi](bool docked) {
        maxi->setEnabled(docked);
        if (!docked) maxi->setChecked(false);
    };
    connect(docklayout, &QRadioButton::toggled, maxi, applyLayoutStyle);
    applyLayoutStyle(isdocked);
    auto *layoutrow = new QHBoxLayout;
    layoutrow->addWidget(new QLabel("Window Layout Style:"));
    layoutrow->addWidget(winlayout);
    layoutrow->addWidget(docklayout);

    settings->beginGroup(Keys::GROUP_TUTORIAL);
    auto *solution = new QCheckBox("Download tutorial solutions enabled");
    solution->setObjectName("solution");
    solution->setChecked(settings->value(Keys::SOLUTION, false).toBool());
    auto *webpage = new QCheckBox("Open tutorial webpage enabled");
    webpage->setObjectName("webpage");
    webpage->setChecked(settings->value(Keys::WEBPAGE, true).toBool());
    settings->endGroup();

    auto *getallfont =
        new QPushButton(QIcon(":/icons/preferences-desktop-font.svg"), "Select &Default Font...");
    auto *gettextfont =
        new QPushButton(QIcon(":/icons/preferences-desktop-font.svg"), "Select &Text Font...");
    connect(getallfont, &QPushButton::released, this, &GeneralTab::newAllFont);
    connect(gettextfont, &QPushButton::released, this, &GeneralTab::newTextFont);

    auto *freqlabel = new QLabel("Data update interval (ms):");
    auto *freqval   = new QSpinBox;
    freqval->setRange(Cfg::DATA_UPDATE_INTERVAL_MIN, Cfg::DATA_UPDATE_INTERVAL_MAX);
    freqval->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    freqval->setValue(settings->value(Keys::UPDFREQ, Cfg::DATA_UPDATE_INTERVAL_DEFAULT).toInt());
    freqval->setObjectName("updfreq");

    auto *chartlabel = new QLabel("Charts update interval (ms):");
    auto *chartval   = new QSpinBox;
    chartval->setRange(Cfg::CHART_UPDATE_INTERVAL_MIN, Cfg::CHART_UPDATE_INTERVAL_MAX);
    chartval->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    chartval->setValue(settings->value(Keys::UPDCHART, Cfg::CHART_UPDATE_INTERVAL_DEFAULT).toInt());
    chartval->setObjectName("updchart");

    int nrow = 0;
    layout->addLayout(layoutrow, nrow++, 0, 1, 2);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    layout->addWidget(echo, nrow, 0);
    layout->addWidget(cite, nrow++, 1);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    layout->addWidget(logv, nrow, 0);
    layout->addWidget(pltv, nrow++, 1);
    layout->addWidget(sldv, nrow, 0);
    layout->addWidget(maxi, nrow++, 1);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    layout->addWidget(solution, nrow, 0);
    layout->addWidget(webpage, nrow++, 1);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    layout->addWidget(getallfont, nrow, 0);
    layout->addWidget(gettextfont, nrow++, 1);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    layout->addWidget(freqlabel, nrow, 0);
    layout->addWidget(freqval, nrow++, 1);
    layout->addWidget(chartlabel, nrow, 0);
    layout->addWidget(chartval, nrow++, 1);
    layout->addWidget(new QHline, nrow++, 0, 1, 2);

    auto *proxylabel = new QLabel("HTTPS proxy setting (empty for no proxy):");
    layout->addWidget(proxylabel, nrow, 0);

    auto https_proxy = QString::fromLocal8Bit(qgetenv("https_proxy"));
    if (https_proxy.isEmpty()) {
        https_proxy     = settings->value(Keys::HTTPS_PROXY, "").toString();
        auto *proxyedit = new QLineEdit(https_proxy);
        proxyedit->setObjectName("proxyval");
        layout->addWidget(proxyedit, nrow++, 1);
    } else {
        layout->addWidget(new QLabel(https_proxy), nrow++, 1);
    }

    auto *timeoutlabel = new QLabel("Download timeout (seconds):");
    auto *timeoutval   = new QSpinBox;
    timeoutval->setRange(Cfg::DOWNLOAD_TIMEOUT_MIN, Cfg::DOWNLOAD_TIMEOUT_MAX);
    timeoutval->setValue(
        qBound(Cfg::DOWNLOAD_TIMEOUT_MIN,
               settings->value(Keys::DOWNLOAD_TIMEOUT, Cfg::DOWNLOAD_TIMEOUT_DEFAULT).toInt(),
               Cfg::DOWNLOAD_TIMEOUT_MAX));
    timeoutval->setObjectName("downloadtimeout");
    timeoutval->setToolTip(
        "Abort a download when no data has arrived for this many seconds.\n"
        "Increase this value on a slow internet connection, so that large\n"
        "downloads like the LAMMPS shared library are not canceled prematurely.");
    layout->addWidget(timeoutlabel, nrow, 0);
    layout->addWidget(timeoutval, nrow++, 1);

#if defined(LAMMPS_GUI_USE_PLUGIN)
    layout->addWidget(new QHline, nrow++, 0, 1, 2);
    auto *pluginlabel = new QLabel("Path to LAMMPS Shared Library File:");
    auto *pluginedit =
        new QLineEdit(settings->value(Keys::PLUGIN_PATH, "liblammpsplugin.so").toString());
    auto *plugindownload = new QPushButton("Download &LAMMPS shared library...");
    auto *pluginbrowse   = new QPushButton("&Browse...");
    plugindownload->setIcon(QIcon(":/icons/download-file.svg"));
    pluginbrowse->setIcon(QIcon(":/icons/document-open.svg"));

    // no compatible pre-compiled LAMMPS library exists for MSVC compiled executables
    if (getLammpsDownloadUrl().isEmpty()) {
        plugindownload->setEnabled(false);
        plugindownload->setToolTip("The pre-compiled LAMMPS shared libraries are not "
                                   "compatible with this LAMMPS-GUI executable");
    }

    auto *pluginlayout = new QHBoxLayout;
    pluginedit->setObjectName("pluginedit");
    pluginlayout->addWidget(pluginedit);
    pluginlayout->addWidget(pluginbrowse);

    connect(plugindownload, &QPushButton::released, this, &GeneralTab::downloadPlugin);
    connect(pluginbrowse, &QPushButton::released, this, &GeneralTab::pluginPath);

    layout->addWidget(pluginlabel, nrow, 0);
    layout->addWidget(plugindownload, nrow++, 1);
    layout->addLayout(pluginlayout, nrow++, 0, 1, 2);
#endif
    layout->addWidget(new QHline, nrow++, 0, 1, 2);

    // deliberately the last line of the tab: which command interpreter the
    // command window starts, as a choice of what is installed, not free text
    auto *shelllabel = new QLabel("Command window shell:");
    auto *shellcombo = new QComboBox;
    shellcombo->setObjectName("shell");
    shellcombo->addItems(CommandWindow::availableShells());
    const QString curshell = CommandWindow::preferredShell();
    if (shellcombo->findText(curshell) < 0) shellcombo->insertItem(0, curshell);
    shellcombo->setCurrentIndex(shellcombo->findText(curshell));
    shellcombo->setToolTip("Command interpreter started by the Command window.\n"
                           "Takes effect when the next shell starts: when the window\n"
                           "is first opened, or on File > Restart Shell.");
    layout->addWidget(shelllabel, nrow, 0);
    layout->addWidget(shellcombo, nrow++, 1);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), nrow, 0);
    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), nrow++,
                    1);
    setLayout(layout);
}

void GeneralTab::updateFonts(const QFont &all, const QFont &text)
{
    if (lammpsgui) {
        lammpsgui->setFont(all);
        lammpsgui->textEdit->document()->setDefaultFont(text);
        if (lammpsgui->wizard) lammpsgui->wizard->setFont(all);
        if (lammpsgui->logwindow) lammpsgui->logwindow->document()->setDefaultFont(text);
        if (lammpsgui->varwindow) lammpsgui->varwindow->setFont(text);
    }

    Preferences *prefs = nullptr;
    for (QWidget *widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "preferences") prefs = qobject_cast<Preferences *>(widget);
    if (prefs) prefs->setFont(all);
}

// build the two application fonts (general and fixed-width) from the settings
static void loadGuiFonts(QSettings *settings, QFont &all_font, QFont &mono_font)
{
    QFontInfo all_info(*GUI_ALLFONT);
    all_font.setFamily(settings->value(Keys::ALLFAMILY, all_info.family()).toString());
    all_font.setPointSize(settings->value(Keys::ALLSIZE, all_info.pointSize()).toInt());
    all_font.setStyleHint(GUI_ALLFONT->styleHint());

    mono_font = monoFontFromSettings();
}

void GeneralTab::newAllFont()
{
    QFont all_font, mono_font;
    loadGuiFonts(settings, all_font, mono_font);

    bool font_ok   = false;
    QFont new_font = QFontDialog::getFont(&font_ok, all_font, this, QString("Select Default Font"));
    if (!font_ok) return;

    updateFonts(new_font, mono_font);
    settings->setValue(Keys::ALLFAMILY, new_font.family());
    settings->setValue(Keys::ALLSIZE, new_font.pointSize());
    settings->sync();
}

void GeneralTab::newTextFont()
{
    QFont all_font, mono_font;
    loadGuiFonts(settings, all_font, mono_font);

    bool font_ok   = false;
    QFont new_font = QFontDialog::getFont(&font_ok, mono_font, this, QString("Select Text Font"));
    if (!font_ok) return;

    updateFonts(all_font, new_font);
    settings->setValue(Keys::MONOFAMILY, new_font.family());
    settings->setValue(Keys::MONOSIZE, new_font.pointSize());
    settings->sync();
}

void GeneralTab::downloadPlugin()
{
    // set platform-specific library file name and config directory
    const auto libName = getLammpsLibName();

    // store in the same config directory where QSettings stores preferences
    const auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty() || !QDir().mkpath(configDir)) {
        critical(this, "LAMMPS-GUI Error", "Cannot determine configuration directory.",
                 "Unable to create a writable directory in the user configuration "
                 "folder for storing the downloaded LAMMPS shared library.");
        return;
    }
    auto libPath = configDir + QDir::separator() + libName;
    auto dlUrl   = getLammpsDownloadUrl();

    URLDownloader downloader(this);
    if (downloader.download(dlUrl, libPath, true, true)) {
        auto canonical = QFileInfo(libPath).canonicalFilePath();
        settings->setValue(Keys::PLUGIN_PATH, canonical);
        auto *field = findChild<QLineEdit *>("pluginedit");
        if (field) field->setText(canonical);
        settings->sync();

        if (auto *prefs = qobject_cast<Preferences *>(window())) prefs->setRelaunch(true);
    }
}

void GeneralTab::pluginPath()
{
    auto *field = findChild<QLineEdit *>("pluginedit");
#if defined(Q_OS_MACOS)
    const QString pattern = "LAMMPS shared library (liblammps*.dylib)";
#elif defined(Q_OS_WIN32)
    const QString pattern = "LAMMPS shared library (liblammps*.dll)";
#else
    const QString pattern = "LAMMPS shared library (liblammps*.so*)";
#endif
    if (field) {
        auto libdir      = QFileInfo(".").absoluteDir();
        const auto &path = field->text();
        if (!path.isEmpty()) {
            libdir = QFileInfo(path).absoluteDir();
        }
        QString pluginfile = QFileDialog::getOpenFileName(
            this, "Select LAMMPS shared library", libdir.canonicalPath(), pattern, nullptr,
            QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly);

        if (!pluginfile.isEmpty() && pluginfile.contains("liblammps", Qt::CaseSensitive)) {
            auto canonical = QFileInfo(pluginfile).canonicalFilePath();
            settings->setValue(Keys::PLUGIN_PATH, canonical);
            field->setText(canonical);
            settings->sync();

            if (auto *prefs = qobject_cast<Preferences *>(window())) prefs->setRelaunch(true);
        }
    }
}

AcceleratorTab::AcceleratorTab(QSettings *_settings, LammpsWrapper *_lammps, QWidget *parent) :
    QWidget(parent), settings(_settings), lammps(_lammps)
{
    auto *mainLayout  = new QHBoxLayout;
    auto *accelerator = new QGroupBox("Choose Accelerator:");
    auto *none        = new QRadioButton("&None");
    auto *opt         = new QRadioButton("O&pt");
    auto *openmp      = new QRadioButton("&OpenMP");
    auto *intel       = new QRadioButton("&Intel");
    auto *kokkos      = new QRadioButton("&Kokkos");
    auto *gpu         = new QRadioButton("GP&U");

    auto *accelframe   = new QFrame;
    auto *accelLayout  = new QVBoxLayout;
    auto *buttonLayout = new QVBoxLayout;
    accelLayout->addWidget(accelerator);
    buttonLayout->addWidget(none);
    buttonLayout->addWidget(opt);
    buttonLayout->addWidget(openmp);
    buttonLayout->addWidget(intel);
    buttonLayout->addWidget(kokkos);
    buttonLayout->addWidget(gpu);
    buttonLayout->addStretch(1);
    accelerator->setLayout(buttonLayout);
    accelframe->setLayout(accelLayout);
    mainLayout->addWidget(accelframe);

    none->setEnabled(true);
    none->setObjectName("none");
    opt->setEnabled(lammps->configHasPackage("OPT"));
    opt->setObjectName("opt");
    openmp->setEnabled(lammps->configHasPackage("OPENMP"));
    openmp->setObjectName("openmp");
    intel->setEnabled(lammps->configHasPackage("INTEL"));
    intel->setObjectName("intel");
    // Kokkos support only works with Serial and OpenMP for now.
    kokkos->setEnabled(false);
    if (lammps->configHasPackage("KOKKOS")) {
        if ((lammps->configAccelerator("KOKKOS", "api", "openmp") ||
             lammps->configAccelerator("KOKKOS", "api", "serial")) &&
            !(lammps->configAccelerator("KOKKOS", "api", "cuda") ||
              lammps->configAccelerator("KOKKOS", "api", "hip") ||
              lammps->configAccelerator("KOKKOS", "api", "sycl")))
            kokkos->setEnabled(true);
    }
    kokkos->setObjectName("kokkos");
    gpu->setEnabled(lammps->configHasPackage("GPU") && lammps->hasGpuDevice());
    gpu->setObjectName("gpu");

    auto *choices       = new QFrame;
    auto *choiceLayout  = new QVBoxLayout;
    QLabel *ntlabel     = nullptr;
    QLineEdit *ntchoice = nullptr;
    if (lammps->configHasOmpSupport()) {
        // maximum number of threads is limited to half of available threads and no more than 16
        // unless OMP_NUM_THREADS is set to a larger value
        int maxthreads = std::min(QThread::idealThreadCount() / 2, 16);
        maxthreads     = std::max(maxthreads, 1);
        maxthreads     = std::max(maxthreads, qEnvironmentVariable("OMP_NUM_THREADS").toInt());

        ntlabel      = new QLabel(QString("Number of threads (max %1):").arg(maxthreads));
        ntchoice     = new QLineEdit(settings->value(Keys::NTHREADS, maxthreads).toString());
        auto *intval = new QIntValidator(1, maxthreads, this);
        ntchoice->setValidator(intval);
    } else {
        ntlabel  = new QLabel("Number of threads (OpenMP not available):");
        ntchoice = new QLineEdit("1");
        ntchoice->setEnabled(false);
    }
    ntchoice->setObjectName("nthreads");

    connect(none, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(opt, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(openmp, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(intel, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(kokkos, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(gpu, &QRadioButton::released, this, &AcceleratorTab::updateAccel);

    auto *intelLayout = new QHBoxLayout;
    auto *intelprec   = new QGroupBox("Intel Precision:");
    auto *inteldouble = new QRadioButton("&Double");
    auto *intelmixed  = new QRadioButton("&Mixed");
    auto *intelsingle = new QRadioButton("&Single");
    intelLayout->addWidget(inteldouble);
    inteldouble->setObjectName("inteldouble");
    intelLayout->addWidget(intelmixed);
    intelmixed->setObjectName("intelmixed");
    intelLayout->addWidget(intelsingle);
    intelsingle->setObjectName("intelsingle");
    intelprec->setLayout(intelLayout);
    intelprec->setObjectName("intelprec");
    intelprec->setEnabled(false);

    connect(inteldouble, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(intelmixed, &QRadioButton::released, this, &AcceleratorTab::updateAccel);
    connect(intelsingle, &QRadioButton::released, this, &AcceleratorTab::updateAccel);

    auto *gpuLayout   = new QHBoxLayout;
    auto *gpuchoice   = new QGroupBox("GPU Settings:");
    auto *gpuneigh    = new QCheckBox("Neighbor&list on GPU");
    auto *gpupaironly = new QCheckBox("Pair st&yles only");
    gpuLayout->addWidget(gpuneigh);
    gpuneigh->setObjectName("gpuneigh");
    gpuneigh->setChecked(settings->value(Keys::GPUNEIGH, true).toBool());
    gpuLayout->addWidget(gpupaironly);
    gpupaironly->setObjectName("gpupaironly");
    gpupaironly->setChecked(settings->value(Keys::GPUPAIRONLY, false).toBool());
    gpuchoice->setLayout(gpuLayout);
    gpuchoice->setObjectName("gpuchoice");
    gpuchoice->setEnabled(false);

    choiceLayout->addWidget(new QLabel("Settings for accelerator packages:\n"));
    choiceLayout->addWidget(ntlabel);
    choiceLayout->addWidget(ntchoice);
    choiceLayout->addWidget(intelprec);
    choiceLayout->addWidget(gpuchoice);
    choiceLayout->addStretch(1);
    choices->setLayout(choiceLayout);
    mainLayout->addWidget(choices);
    setLayout(mainLayout);

    // trigger update of nthreads line editor field depending on accelerator choice
    // fall back on None, if configured accelerator package is no longer available
    int choice = settings->value(Keys::ACCELERATOR, AcceleratorTab::None).toInt();
    int iprec  = settings->value(Keys::INTELPREC, AcceleratorTab::Mixed).toInt();
    if (iprec == AcceleratorTab::Double)
        inteldouble->setChecked(true);
    else if (iprec == AcceleratorTab::Mixed)
        intelmixed->setChecked(true);
    else if (iprec == AcceleratorTab::Single)
        intelsingle->setChecked(true);

    switch (choice) {
        case AcceleratorTab::Opt:
            if (opt->isEnabled())
                opt->click();
            else
                none->click();
            break;
        case AcceleratorTab::OpenMP:
            if (openmp->isEnabled())
                openmp->click();
            else
                none->click();
            break;
        case AcceleratorTab::Intel:
            if (intel->isEnabled()) {
                intel->click();
                intelprec->setEnabled(true);
            } else {
                none->click();
            }
            break;
        case AcceleratorTab::Kokkos:
            if (kokkos->isEnabled())
                kokkos->click();
            else
                none->click();
            break;
        case AcceleratorTab::Gpu:
            if (gpu->isEnabled()) {
                gpu->click();
                gpuchoice->setEnabled(true);
            } else
                none->click();
            break;
        case AcceleratorTab::None: // fallthrough
        default:
            none->click();
            break;
    }
}

void AcceleratorTab::updateAccel()
{
    // store selected accelerator
    int choice = AcceleratorTab::None;

    QList<QRadioButton *> allButtons = findChildren<QRadioButton *>();
    for (auto &anyButton : allButtons) {
        if (anyButton->isChecked()) {
            const auto &button = anyButton->objectName();
            if (buttonToChoice.contains(button)) {
                choice = buttonToChoice.value(button);
            }
        }
    }

    auto *group = findChild<QGroupBox *>("intelprec");
    group->setEnabled(choice == AcceleratorTab::Intel);

    group = findChild<QGroupBox *>("gpuchoice");
    group->setEnabled(choice == AcceleratorTab::Gpu);

    // The number of threads field is disabled and the value set to 1 for "None" and "Opt" choice
    auto *field = findChild<QLineEdit *>("nthreads");
    if (field) {
        if ((choice == AcceleratorTab::None) || (choice == AcceleratorTab::Opt) ||
            !lammps->configHasOmpSupport()) {
            field->setText("1");
            field->setEnabled(false);
        } else {
            field->setText(settings->value(Keys::NTHREADS, 1).toString());
            field->setEnabled(true);
        }
    }
}

SnapshotTab::SnapshotTab(QSettings *_settings, QWidget *parent) :
    QWidget(parent), settings(_settings)
{
    auto *grid = new QGridLayout;

    // left column labels
    auto *xsize = new QLabel("Image width:");
    auto *ysize = new QLabel("Image height:");
    auto *zoom  = new QLabel("Zoom factor:");
    auto *hrota = new QLabel("Horizontal View Angle:");
    auto *vrota = new QLabel("Vertical View Angle:");
    auto *anti  = new QLabel("Antialias:");
    auto *ssao  = new QLabel("HQ Image mode:");
    auto *shiny = new QLabel("Shiny Image mode:");
    auto *cback = new QLabel("Background Color:");
    auto *cgrad = new QLabel("Background Gradient:");
    auto *cbac2 = new QLabel("Background2 Color:");

    // right column labels
    auto *bbox   = new QLabel("Show Box:");
    auto *bxdiam = new QLabel("Box Diameter:");
    auto *cbox   = new QLabel("Box Color:");
    auto *axes   = new QLabel("Show Axes:");
    auto *axlen  = new QLabel("Axes Length:");
    auto *axdia  = new QLabel("Axes Diameter:");
    auto *vdw    = new QLabel("VDW Style:");
    auto *bond   = new QLabel("Dynamic Bonds:");
    auto *bclbl  = new QLabel("Bond Cutoff:");

    settings->beginGroup(Keys::GROUP_SNAPSHOT);

    auto *colorcompleter = new QColorCompleter();
    auto *colorvalidator = new QColorValidator();
    QFontMetrics metrics(fontMetrics());
    auto *intval = new QIntValidator(100, 100000, this);

    // factory helpers for the repetitive snapshot widget setup
    auto makeCheckBox = [&](const QString &key, const QString &name, bool def) {
        auto *box = new QCheckBox;
        box->setChecked(settings->value(key, def).toBool());
        box->setObjectName(name);
        return box;
    };
    auto makeNumEdit = [&](const QString &key, const QString &def, QValidator *validator) {
        auto *edit = new QLineEdit(settings->value(key, def).toString());
        edit->setValidator(validator);
        edit->setObjectName(key);
        return edit;
    };
    auto makeColorEdit = [&](const QString &key, const QString &def) {
        auto *edit = new QLineEdit(settings->value(key, def).toString());
        edit->setObjectName(key);
        edit->setCompleter(colorcompleter);
        edit->setValidator(colorvalidator);
        edit->setFixedSize(metrics.averageCharWidth() * 12, metrics.height() + 4);
        return edit;
    };

    // left column values
    auto *xval       = makeNumEdit(Keys::XSIZE, "600", intval);
    auto *yval       = makeNumEdit(Keys::YSIZE, "600", intval);
    auto *zval       = makeNumEdit(Keys::ZOOM, "1.0", new QDoubleValidator(0.01, 100.0, 100, this));
    auto *hrval      = makeNumEdit(Keys::HROT, "60", new QIntValidator(0, 360, this));
    auto *vrval      = makeNumEdit(Keys::VROT, "30", new QIntValidator(-180, 180, this));
    auto *aval       = makeCheckBox(Keys::ANTIALIAS, "anti", false);
    auto *sval       = makeCheckBox(Keys::SSAO, "ssao", false);
    auto *hval       = makeCheckBox(Keys::SHINYSTYLE, "shiny", true);
    auto *background = makeColorEdit(Keys::BACKCOLOR, "black");
    auto *background2 = makeColorEdit(Keys::BACKCOLOR2, "white");
    auto *gradient    = makeCheckBox(Keys::USEGRADIENT, "usegradient", true);

    // right column values
    auto *bval  = makeCheckBox(Keys::BOX, "box", true);
    auto *bdval = makeNumEdit(Keys::BOXDIAM, "0.025", new QDoubleValidator(0.001, 1.0, 100, this));
    auto *boxcolor = makeColorEdit(Keys::BOXCOLOR, "gold");
    auto *eval     = makeCheckBox(Keys::AXES, "axes", false);
    auto *alval    = makeNumEdit(Keys::AXESLEN, "0.5", new QDoubleValidator(0.01, 10.0, 100, this));
    auto *adval = makeNumEdit(Keys::AXESDIAM, "0.05", new QDoubleValidator(0.001, 1.0, 100, this));
    auto *vval  = makeCheckBox(Keys::VDWSTYLE, "vdwstyle", false);
    auto *uval  = makeCheckBox(Keys::AUTOBOND, "autobond", false);

    // bond cutoff has no input validator
    auto *bcut = new QLineEdit(settings->value(Keys::BONDCUT, "1.6").toString());
    bcut->setObjectName("bondcut");

    settings->endGroup();

    // vertical separator
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    // left column layout (columns 0-1)
    int i = 0;
    grid->addWidget(xsize, i, 0, Qt::AlignTop);
    grid->addWidget(xval, i++, 1, Qt::AlignTop);
    grid->addWidget(ysize, i, 0, Qt::AlignTop);
    grid->addWidget(yval, i++, 1, Qt::AlignTop);
    grid->addWidget(zoom, i, 0, Qt::AlignTop);
    grid->addWidget(zval, i++, 1, Qt::AlignTop);
    grid->addWidget(hrota, i, 0, Qt::AlignTop);
    grid->addWidget(hrval, i++, 1, Qt::AlignTop);
    grid->addWidget(vrota, i, 0, Qt::AlignTop);
    grid->addWidget(vrval, i++, 1, Qt::AlignTop);
    grid->addWidget(anti, i, 0, Qt::AlignTop);
    grid->addWidget(aval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(ssao, i, 0, Qt::AlignTop);
    grid->addWidget(sval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(shiny, i, 0, Qt::AlignTop);
    grid->addWidget(hval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(cback, i, 0, Qt::AlignTop);
    grid->addWidget(background, i++, 1, Qt::AlignVCenter);
    grid->addWidget(cgrad, i, 0, Qt::AlignTop);
    grid->addWidget(gradient, i++, 1, Qt::AlignVCenter);
    grid->addWidget(cbac2, i, 0, Qt::AlignTop);
    grid->addWidget(background2, i++, 1, Qt::AlignVCenter);
    int nrows = i;

    // right column layout (columns 3-4)
    int j = 0;
    grid->addWidget(bbox, j, 3, Qt::AlignTop);
    grid->addWidget(bval, j++, 4, Qt::AlignVCenter);
    grid->addWidget(bxdiam, j, 3, Qt::AlignTop);
    grid->addWidget(bdval, j++, 4, Qt::AlignTop);
    grid->addWidget(cbox, j, 3, Qt::AlignTop);
    grid->addWidget(boxcolor, j++, 4, Qt::AlignVCenter);
    grid->addWidget(axes, j, 3, Qt::AlignTop);
    grid->addWidget(eval, j++, 4, Qt::AlignVCenter);
    grid->addWidget(axlen, j, 3, Qt::AlignTop);
    grid->addWidget(alval, j++, 4, Qt::AlignTop);
    grid->addWidget(axdia, j, 3, Qt::AlignTop);
    grid->addWidget(adval, j++, 4, Qt::AlignTop);
    grid->addWidget(vdw, j, 3, Qt::AlignTop);
    grid->addWidget(vval, j++, 4, Qt::AlignVCenter);
    grid->addWidget(bond, j, 3, Qt::AlignTop);
    grid->addWidget(uval, j++, 4, Qt::AlignVCenter);
    grid->addWidget(bclbl, j, 3, Qt::AlignTop);
    grid->addWidget(bcut, j++, 4, Qt::AlignVCenter);

    // equal weight for left and right halves
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    grid->setColumnStretch(4, 1);

    // expanding spacer row so preferences stay at the top
    grid->setRowStretch(nrows, 1);

    // extend separator to span all rows including the spacer row
    grid->addWidget(separator, 0, 2, nrows + 1, 1);

    setLayout(grid);

    connect(vval, &QCheckBox::toggled, this, &SnapshotTab::chooseVdw);
    connect(uval, &QCheckBox::toggled, this, &SnapshotTab::chooseBond);

    // the second background color only applies when the gradient is enabled
    background2->setEnabled(gradient->isChecked());
    connect(gradient, &QCheckBox::toggled, background2, &QLineEdit::setEnabled);
}

void SnapshotTab::chooseVdw()
{
    auto *vdw = findChild<QCheckBox *>("vdwstyle");
    auto *bnd = findChild<QCheckBox *>("autobond");
    if (vdw && bnd) {
        if (vdw->isChecked()) bnd->setChecked(false);
    }
}

void SnapshotTab::chooseBond()
{
    auto *vdw = findChild<QCheckBox *>("vdwstyle");
    auto *bnd = findChild<QCheckBox *>("autobond");
    if (vdw && bnd) {
        if (bnd->isChecked()) vdw->setChecked(false);
    }
}

EditorTab::EditorTab(QSettings *_settings, QWidget *parent) : QWidget(parent), settings(_settings)
{
    settings->beginGroup(Keys::GROUP_REFORMAT);
    auto *grid     = new QGridLayout;
    auto *reformat = new QLabel("Tab Reformatting settings:");
    auto *cmdlbl   = new QLabel("Command width:");
    auto *typelbl  = new QLabel("Type width:");
    auto *idlbl    = new QLabel("ID width:");
    auto *namelbl  = new QLabel("Name width:");
    auto *retlbl   = new QLabel("Reformat with 'Enter':");
    auto *autolbl  = new QLabel("Automatic completion:");
    auto *savlbl   = new QLabel("Auto-save on 'Run' and 'Quit':");
    auto *lintlbl  = new QLabel("Check input before 'Run':");
    auto *cmdval   = new QSpinBox;
    auto *typeval  = new QSpinBox;
    auto *idval    = new QSpinBox;
    auto *nameval  = new QSpinBox;
    auto *retval   = new QCheckBox;
    auto *autoval  = new QCheckBox;
    auto *savval   = new QCheckBox;
    cmdval->setObjectName("cmdval");
    cmdval->setRange(Cfg::COMPLETION_CHARS_MIN, Cfg::COMPLETION_CHARS_MAX);
    cmdval->setValue(settings->value(Keys::COMMAND, "16").toInt());
    typeval->setObjectName("typeval");
    typeval->setRange(Cfg::COMPLETION_CHARS_MIN, Cfg::COMPLETION_CHARS_MAX);
    typeval->setValue(settings->value(Keys::TYPE, "4").toInt());
    idval->setObjectName("idval");
    idval->setRange(Cfg::COMPLETION_CHARS_MIN, Cfg::COMPLETION_CHARS_MAX);
    idval->setValue(settings->value(Keys::ID, "8").toInt());
    nameval->setObjectName("nameval");
    nameval->setRange(Cfg::COMPLETION_CHARS_MIN, Cfg::COMPLETION_CHARS_MAX);
    nameval->setValue(settings->value(Keys::NAME, "8").toInt());
    retval->setObjectName("retval");
    retval->setChecked(settings->value(Keys::RETURN, false).toBool());
    autoval->setObjectName("autoval");
    autoval->setChecked(settings->value(Keys::AUTOMATIC, true).toBool());
    savval->setObjectName("savval");
    savval->setChecked(settings->value(Keys::AUTOSAVE, false).toBool());
    settings->endGroup();

    // the pre-run check setting is stored outside the reformat group
    auto *lintval = new QCheckBox;
    lintval->setObjectName("lintcheck");
    lintval->setChecked(settings->value(Keys::LINTCHECK, true).toBool());

    int i = 0;
    grid->addWidget(reformat, i++, 0, 1, 2, Qt::AlignTop | Qt::AlignHCenter);
    grid->addWidget(cmdlbl, i, 0, Qt::AlignTop);
    grid->addWidget(cmdval, i++, 1, Qt::AlignTop);
    grid->addWidget(typelbl, i, 0, Qt::AlignTop);
    grid->addWidget(typeval, i++, 1, Qt::AlignTop);
    grid->addWidget(idlbl, i, 0, Qt::AlignTop);
    grid->addWidget(idval, i++, 1, Qt::AlignTop);
    grid->addWidget(namelbl, i, 0, Qt::AlignTop);
    grid->addWidget(nameval, i++, 1, Qt::AlignTop);
    grid->addWidget(retlbl, i, 0, Qt::AlignTop);
    grid->addWidget(retval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(autolbl, i, 0, Qt::AlignTop);
    grid->addWidget(autoval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(new QLabel(" "), i++, 0);
    grid->addWidget(savlbl, i, 0, Qt::AlignTop);
    grid->addWidget(savval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(lintlbl, i, 0, Qt::AlignTop);
    grid->addWidget(lintval, i++, 1, Qt::AlignVCenter);

    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Minimum, QSizePolicy::Expanding), i, 0);
    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Minimum, QSizePolicy::Expanding), i, 1);
    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Expanding, QSizePolicy::Expanding), i, 2);
    setLayout(grid);
}

ChartsTab::ChartsTab(QSettings *_settings, QWidget *parent) : QWidget(parent), settings(_settings)
{
    auto *grid     = new QGridLayout;
    auto *chartlbl = new QLabel("Charts default settings:");

    settings->beginGroup(Keys::GROUP_CHARTS);
    auto *titlelbl = new QLabel("Default chart title:");
    auto *titletxt =
        new QLineEdit(settings->value(Keys::TITLE, Cfg::CHART_TITLE_DEFAULT).toString());
    titletxt->setObjectName("title");
    auto *titlehlp = new QLabel("(use %f for current input file)");

    // list of choices must be kept in sync with list in chartviewer
    auto *smoothlbl = new QLabel("Default plot data choice:");
    auto *smoothval = new QComboBox;
    smoothval->addItem("Raw");
    smoothval->addItem("Smooth");
    smoothval->addItem("Both");
    smoothval->setObjectName("smoothchoice");
    smoothval->setCurrentIndex(settings->value(Keys::SMOOTHCHOICE, 0).toInt());

    // the series style defaults, laid out like the per-chart "Chart Style"
    // dialog they preset.  Color lists and display-mode lists must be kept in
    // sync with mybrushes and ChartDisplayMode in chartviewer.
    auto colorBox = [this](const QString &name, const QString &key, int fallback) {
        auto *combo = new QComboBox;
        combo->addItem("Black");
        combo->addItem("Blue");
        combo->addItem("Red");
        combo->addItem("Green");
        combo->addItem("Gray");
        combo->setObjectName(name);
        combo->setCurrentIndex(settings->value(key, fallback).toInt());
        return combo;
    };
    auto modeBox = [this](const QString &name, const QString &key) {
        auto *combo = new QComboBox;
        combo->addItem("Lines");
        combo->addItem("Points");
        combo->addItem("Lines + Points");
        combo->setObjectName(name);
        combo->setCurrentIndex(settings->value(key, 0).toInt());
        return combo;
    };
    auto widthBox = [this](const QString &name, const QString &key, double fallback) {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(Cfg::LINE_WIDTH_MIN, Cfg::LINE_WIDTH_MAX);
        spin->setSingleStep(0.5);
        spin->setObjectName(name);
        spin->setValue(settings->value(key, fallback).toDouble());
        return spin;
    };
    auto pointBox = [this](const QString &name, const QString &key) {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(Cfg::POINT_SIZE_MIN, Cfg::POINT_SIZE_MAX);
        spin->setSingleStep(1.0);
        spin->setObjectName(name);
        spin->setValue(settings->value(key, Cfg::POINT_SIZE_DEFAULT).toDouble());
        return spin;
    };

    auto *rawbox  = new QGroupBox("Raw data");
    auto *rawform = new QFormLayout(rawbox);
    rawform->addRow("Display:", modeBox("rawmode", Keys::RAWMODE));
    rawform->addRow("Color:", colorBox("rawbrush", Keys::RAWBRUSH, Cfg::RAWBRUSH_DEFAULT));
    rawform->addRow("Line width:", widthBox("rawwidth", Keys::RAWWIDTH, Cfg::LINE_WIDTH_DEFAULT));
    rawform->addRow("Point size:", pointBox("rawpointsize", Keys::RAWPOINTSIZE));

    auto *procbox  = new QGroupBox("Processed data");
    auto *procform = new QFormLayout(procbox);
    procform->addRow("Display:", modeBox("smoothmode", Keys::SMOOTHMODE));
    procform->addRow("Color:",
                     colorBox("smoothbrush", Keys::SMOOTHBRUSH, Cfg::SMOOTHBRUSH_DEFAULT));
    procform->addRow("Line width:",
                     widthBox("smoothwidth", Keys::SMOOTHWIDTH, Cfg::LINE_WIDTH_DEFAULT));
    procform->addRow("Point size:", pointBox("smoothpointsize", Keys::SMOOTHPOINTSIZE));

    auto *errbox = new QGroupBox("Error bars");
    errbox->setToolTip("Error bars are drawn for imported data that carries an\n"
                       "uncertainty, e.g. the average of a set of fix ave/* blocks.");
    auto *errform = new QFormLayout(errbox);
    errform->addRow("Color:", colorBox("errbrush", Keys::ERRBRUSH, Cfg::ERRBRUSH_DEFAULT));
    errform->addRow("Line width:", widthBox("errwidth", Keys::ERRWIDTH, Cfg::ERR_WIDTH_DEFAULT));

    auto *stylerow = new QHBoxLayout;
    stylerow->addWidget(rawbox);
    stylerow->addWidget(procbox);
    stylerow->addWidget(errbox);
    stylerow->addStretch(1);

    auto *smwindlbl = new QLabel("Default smoothing window:");
    auto *smwindval = new QSpinBox;
    smwindval->setRange(Cfg::SMOOTH_WINDOW_MIN, Cfg::SMOOTH_WINDOW_MAX);
    smwindval->setValue(settings->value(Keys::SMOOTHWINDOW, Cfg::SMOOTH_WINDOW_DEFAULT).toInt());
    smwindval->setObjectName("smoothwindow");

    auto *smordrlbl = new QLabel("Default smoothing order:");
    auto *smordrval = new QSpinBox;
    smordrval->setRange(Cfg::SMOOTH_ORDER_MIN, Cfg::SMOOTH_ORDER_MAX);
    smordrval->setValue(settings->value(Keys::SMOOTHORDER, Cfg::SMOOTH_ORDER_DEFAULT).toInt());
    smordrval->setObjectName("smoothorder");

    auto *gridlbl = new QLabel("Draw major grid:");
    auto *usegrid = new QCheckBox;
    usegrid->setChecked(settings->value(Keys::GRID, true).toBool());
    usegrid->setObjectName("grid");
    auto *minorlbl = new QLabel("Draw minor grid:");
    auto *useminor = new QCheckBox;
    useminor->setChecked(settings->value(Keys::MINORGRID, true).toBool());
    useminor->setObjectName("minorgrid");
    settings->endGroup();

    auto *chartxlbl = new QLabel("Chart default width:");
    auto *chartxval = new QSpinBox;
    chartxval->setRange(Cfg::CHART_WIDTH_MIN, Cfg::CHART_WIDTH_MAX);
    chartxval->setValue(settings->value(Keys::CHARTX, Cfg::CHART_DEFAULT_WIDTH).toInt());
    chartxval->setObjectName("chartx");
    auto *chartylbl = new QLabel("Chart default height:");
    auto *chartyval = new QSpinBox;
    chartyval->setRange(Cfg::CHART_HEIGHT_MIN, Cfg::CHART_HEIGHT_MAX);
    chartyval->setValue(settings->value(Keys::CHARTY, Cfg::CHART_DEFAULT_HEIGHT).toInt());
    chartyval->setObjectName("charty");

    int i = 0;
    grid->addWidget(chartlbl, i++, 0, 1, 2, Qt::AlignTop | Qt::AlignHCenter);
    grid->addWidget(titlelbl, i, 0, Qt::AlignTop);
    grid->addWidget(titletxt, i, 1, Qt::AlignTop);
    grid->addWidget(titlehlp, i++, 2, Qt::AlignTop);
    grid->addWidget(smoothlbl, i, 0, Qt::AlignTop);
    grid->addWidget(smoothval, i++, 1, Qt::AlignTop);
    grid->addLayout(stylerow, i++, 0, 1, 3);
    grid->addWidget(smwindlbl, i, 0, Qt::AlignTop);
    grid->addWidget(smwindval, i++, 1, Qt::AlignTop);
    grid->addWidget(smordrlbl, i, 0, Qt::AlignTop);
    grid->addWidget(smordrval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(chartxlbl, i, 0, Qt::AlignTop);
    grid->addWidget(chartxval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(chartylbl, i, 0, Qt::AlignTop);
    grid->addWidget(chartyval, i++, 1, Qt::AlignVCenter);
    grid->addWidget(gridlbl, i, 0, Qt::AlignTop);
    grid->addWidget(usegrid, i++, 1, Qt::AlignTop | Qt::AlignLeft);
    grid->addWidget(minorlbl, i, 0, Qt::AlignTop);
    grid->addWidget(useminor, i++, 1, Qt::AlignTop | Qt::AlignLeft);
    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Minimum, QSizePolicy::Expanding), i, 0);
    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Minimum, QSizePolicy::Expanding), i, 1);
    grid->addItem(new QSpacerItem(100, 100, QSizePolicy::Expanding, QSizePolicy::Expanding), i, 2);
    setLayout(grid);
}

// Local Variables:
// c-basic-offset: 4
// End:
