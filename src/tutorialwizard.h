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

#ifndef TUTORIALWIZARD_H
#define TUTORIALWIZARD_H

#include <QWizard>

class LammpsGui;

/**
 * @brief Wizard dialog for interactive LAMMPS tutorials
 *
 * TutorialWizard provides a step-by-step wizard interface for setting up
 * and running LAMMPS tutorials. It guides users through directory selection,
 * file preparation, and launching tutorial exercises.
 */
class TutorialWizard : public QWizard {
    Q_OBJECT

public:
    /**
     * @brief Construct a tutorial wizard
     * @param collection Tutorial collection index
     * @param ntutorial Tutorial number within the collection
     * @param lammpsgui Pointer to LammpsGui for sending signals
     * @param parent Parent widget
     */
    TutorialWizard(int collection, int ntutorial, LammpsGui *lammpsgui, QWidget *parent = nullptr);

    /**
     * @brief Accept the wizard and set up the tutorial
     *
     * Called when the user completes the wizard. Sets up tutorial files
     * and opens the tutorial in the main window.
     */
    void accept() override;

private:
    int collection;       ///< Tutorial collection index
    int ntutorial;        ///< Tutorial number identifier within the collection
    LammpsGui *lammpsgui; ///< Main widget pointer for receiving signals
};

#endif // TUTORIALWIZARD_H

// Local Variables:
// c-basic-offset: 4
// End:
