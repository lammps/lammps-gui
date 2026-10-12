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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QFont;
class QSettings;
class QTabWidget;
class LammpsWrapper;
class LammpsGui;

/**
 * @brief Preferences/Settings dialog for LAMMPS-GUI
 *
 * This dialog provides a tabbed interface for configuring various aspects
 * of LAMMPS-GUI including:
 * - General settings (LAMMPS library path, plugins, etc.)
 * - Accelerator package settings
 * - Image viewer defaults
 * - Editor appearance and behavior
 * - Chart viewer settings
 *
 * Settings are persisted using QSettings and loaded on startup.
 */
class Preferences : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param lammps Pointer to LammpsWrapper for querying LAMMPS configuration
     * @param lammpsgui Pointer to LammpsGui for sending signals
     * @param parent Parent widget
     */
    explicit Preferences(LammpsWrapper *lammps, LammpsGui *lammpsgui, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~Preferences() override;

    Preferences()                               = delete;
    Preferences(const Preferences &)            = delete;
    Preferences(Preferences &&)                 = delete;
    Preferences &operator=(const Preferences &) = delete;
    Preferences &operator=(Preferences &&)      = delete;

private slots:
    /**
     * @brief Handle dialog acceptance - saves all settings
     */
    void accept() override;

public:
    /**
     * @brief Set flag indicating application needs restart
     * @param val true if restart needed, false otherwise
     *
     * Some settings require restarting the application to take effect.
     */
    void setRelaunch(bool val) { needRelaunch = val; }

    /**
     * @brief Request a restart and record why
     * @param reason One sentence naming the setting that changed
     *
     * The reasons are collected and shown together in the dialog that
     * announces the relaunch, so a single visit to the preferences that
     * changes several restart-only settings explains all of them.
     */
    void requestRelaunch(const QString &reason)
    {
        needRelaunch = true;
        if (!relaunchReasons.contains(reason)) relaunchReasons << reason;
    }

private:
    QTabWidget *tabWidget;       ///< Tab widget for preference categories
    QDialogButtonBox *buttonBox; ///< Dialog buttons (OK, Cancel)
    QSettings *settings;         ///< Qt settings storage
    LammpsWrapper *lammps;       ///< LAMMPS interface for configuration queries
    LammpsGui *lammpsgui;        ///< Main widget pointer for receiving signals
    bool needRelaunch;           ///< Flag indicating restart is needed
    QStringList relaunchReasons; ///< Settings that asked for the restart
};

// individual tabs

/**
 * @brief Preferences Tab for General LAMMPS-GUI Settings
 */
class GeneralTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to QSettings for storing preferences
     * @param lammps Pointer to LammpsWrapper for querying LAMMPS configuration
     * @param lammpsgui Pointer to LammpsGui for sending signals
     * @param parent Parent widget
     */
    explicit GeneralTab(QSettings *settings, LammpsWrapper *lammps, LammpsGui *lammpsgui,
                        QWidget *parent = nullptr);

private slots:
    void downloadPlugin();
    void pluginPath();
    void newAllFont();
    void newTextFont();

private:
    void updateFonts(const QFont &all, const QFont &text);
    QSettings *settings;
    LammpsWrapper *lammps;
    LammpsGui *lammpsgui;
};

/**
 * @brief Preferences Tab for LAMMPS Accelerator settings
 */
class AcceleratorTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to QSettings for storing preferences
     * @param lammps Pointer to LammpsWrapper for querying available accelerator packages
     * @param parent Parent widget
     */
    explicit AcceleratorTab(QSettings *settings, LammpsWrapper *lammps, QWidget *parent = nullptr);
    /** Constants for selecting LAMMPS accelerator package */
    enum AccelType {
        None,   ///< no accelerator
        Opt,    ///< OPT package
        OpenMP, ///< OPENMP package
        Intel,  ///< INTEL package
        Kokkos, ///< KOKKOS package
        Gpu     ///< GPU package
    };
    /** Constants for selecting LAMMPS accelerator precision */
    enum AccelPrec {
        Double, ///< full double precision
        Mixed,  ///< only accumulators in double precision, rest in single precision
        Single  ///< full single precision
    };

private slots:
    void updateAccel();

private:
    QSettings *settings;
    LammpsWrapper *lammps;
};

/**
 * @brief Preferences Tab for Snapshot Viewer Settings
 */
class SnapshotTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to QSettings for storing preferences
     * @param parent Parent widget
     */
    explicit SnapshotTab(QSettings *settings, QWidget *parent = nullptr);

private slots:
    void chooseVdw();
    void chooseBond();

private:
    QSettings *settings;
};

/**
 * @brief Preferences Tab for LAMMPS-GUI Editor Settings
 */
class EditorTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to QSettings for storing preferences
     * @param parent Parent widget
     */
    explicit EditorTab(QSettings *settings, QWidget *parent = nullptr);

private:
    QSettings *settings;
};

/**
 * @brief Preferences Tab for LAMMPS-GUI Charts Viewer Settings
 */
class ChartsTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param settings Pointer to QSettings for storing preferences
     * @param parent Parent widget
     */
    explicit ChartsTab(QSettings *settings, QWidget *parent = nullptr);

private:
    QSettings *settings;
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
