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

#include "lammpsrunner.h"
#include "lammpswrapper.h"

#include <utility>

LammpsRunner::LammpsRunner(QObject *parent) : QThread(parent), lammps(nullptr) {}

void LammpsRunner::setupRun(LammpsWrapper *_lammps, std::string _input, std::string _file,
                            bool _clearfirst)
{
    lammps = _lammps;
    input  = std::move(_input);
    file   = std::move(_file);
    if (_clearfirst) lammps->command("clear");
}

void LammpsRunner::run()
{
    if (!input.empty()) {
        lammps->commandsString(input.c_str());
    } else if (!file.empty()) {
        lammps->file(file.c_str());
    }
    emit resultReady();
}

// Local Variables:
// c-basic-offset: 4
// End:
