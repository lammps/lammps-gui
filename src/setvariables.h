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

#ifndef SET_VARIABLES_H
#define SET_VARIABLES_H

#include <QDialog>
#include <QList>

#include "inputvariables.h"

/**
 * @brief Dialog for editing LAMMPS index-style variable definitions
 *
 * SetVariables provides a dialog for managing name-value pairs that
 * will be used as index-style variables in LAMMPS input scripts.
 * Users can add, delete, and edit variable definitions. The dialog
 * shows the variables that the input script defines or uses; values
 * that override a differing definition in the input script are shown
 * in bold with a tooltip listing the script value.
 */
class SetVariables : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param vars Reference to list of variable entries (modified in place)
     * @param parent Parent widget
     */
    explicit SetVariables(QList<VariableEntry> &vars, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~SetVariables() override = default;

    SetVariables()                                = delete;
    SetVariables(const SetVariables &)            = delete;
    SetVariables(SetVariables &&)                 = delete;
    SetVariables &operator=(const SetVariables &) = delete;
    SetVariables &operator=(SetVariables &&)      = delete;

private slots:
    /**
     * @brief Accept dialog and update variable list
     */
    void accept() override;

    /**
     * @brief Add a new empty variable row
     */
    void addRow();

    /**
     * @brief Delete the currently selected variable row
     */
    void delRow();

private:
    QList<VariableEntry> &vars; ///< Reference to variable list
    class QVBoxLayout *layout;  ///< Dialog layout
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
