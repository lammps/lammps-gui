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

#ifndef LAMMPSRUNNER_H
#define LAMMPSRUNNER_H

#include <QThread>
#include <string>

class LammpsWrapper;

/**
 * @brief Worker thread for executing LAMMPS simulations
 *
 * This class runs LAMMPS simulations in a background thread to maintain
 * UI responsiveness during long calculations. It executes either a
 * LAMMPS command string or a full input file and emits a signal upon
 * completion.
 *
 * Input is passed using std::string, ensuring clean ownership transfer.
 */
class LammpsRunner : public QThread {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit LammpsRunner(QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LammpsRunner() override = default;

    LammpsRunner(const LammpsRunner &)            = delete;
    LammpsRunner(LammpsRunner &&)                 = delete;
    LammpsRunner &operator=(const LammpsRunner &) = delete;
    LammpsRunner &operator=(LammpsRunner &&)      = delete;

    /**
     * @brief Prepare the runner thread with LAMMPS instance and commands
     * @param _lammps     Pointer to LammpsWrapper instance
     * @param _input      String of LAMMPS commands to execute (can be empty)
     * @param _file       Input file path to execute (can be empty)
     * @param _clearfirst If true, wipe the current LAMMPS system state first
     *
     * Sets up the runner with the LAMMPS instance and input. Clears any
     * previous LAMMPS state with the "clear" command, unless _clearfirst
     * is false, which continues from the current state (used to extend a
     * previous run). Either input or file should be provided, not both.
     */
    void setupRun(LammpsWrapper *_lammps, std::string _input, std::string _file = {},
                  bool _clearfirst = true);

signals:
    /**
     * @brief Signal emitted when LAMMPS execution completes
     */
    void resultReady();

protected:
    /**
     * @brief Thread execution function - runs LAMMPS commands or input file
     *
     * This function executes in the worker thread. It processes either
     * a string of LAMMPS commands or an input file, then signals completion.
     */
    void run() override;

private:
    LammpsWrapper *lammps; ///< Pointer to the LAMMPS wrapper instance (not owned)
    std::string input;     ///< String of LAMMPS commands to execute
    std::string file;      ///< Input file path to execute
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
