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

#ifndef INPUT_VARIABLES_H
#define INPUT_VARIABLES_H

#include <QList>
#include <QString>

/**
 * @brief One index-style variable managed via the Set Variables dialog
 *
 * The effective value is what LAMMPS-GUI defines before a run (like the
 * -var command line flag would); the script value records what the input
 * script currently assigns so that edits to the input can be told apart
 * from overrides entered in the Set Variables dialog.
 */
struct VariableEntry {
    QString name;        ///< variable name
    QString value;       ///< effective value defined before a run; empty if unset
    QString scriptValue; ///< value assigned in the input script; empty if not defined there
};

/**
 * @brief Result of matching a line against an index variable definition
 *
 * The value offsets refer to the unmodified line text so they can be used
 * to locate the value in a text editor for visual markup.
 */
struct IndexVariableMatch {
    bool valid = false;  ///< true if the line defines an index style variable
    QString name;        ///< variable name
    QString value;       ///< assigned value with surrounding whitespace removed
    int valueStart  = 0; ///< offset of the value in the line
    int valueLength = 0; ///< length of the (trimmed) value in the line
};

/**
 * @brief Match a single input script line against an index variable definition
 * @param line One line of input script text
 * @return Match result with the name, value, and value position on success
 */
IndexVariableMatch matchIndexVariable(const QString &line);

/**
 * @brief Collect index-style variables from an input script
 *
 * Records every index variable definition (first definition wins, as in
 * LAMMPS) with its assigned value and every use of an otherwise undefined
 * variable with an empty value.
 *
 * @param text Complete input script text
 * @return List of variable entries in order of appearance
 */
QList<VariableEntry> parseInputVariables(const QString &text);

/**
 * @brief Fold a fresh parse of the input script into an existing variable list
 *
 * A changed, non-empty script value is the most recent user edit and
 * replaces the current value, while an unchanged one preserves the value
 * (which may be an override from the Set Variables dialog).  Entries no
 * longer present in the script are kept as long as they have a value
 * (they may apply to included files) and dropped otherwise.
 *
 * @param parsed   Entries from parsing the current input script
 * @param previous Current variable list (parse results plus dialog edits)
 * @return Merged list: script order first, then retained leftover entries
 */
QList<VariableEntry> mergeInputVariables(const QList<VariableEntry> &parsed,
                                         const QList<VariableEntry> &previous);

/**
 * @brief Check whether an entry overrides the definition in the input script
 * @param entry Variable entry to check
 * @return true if the entry's value replaces a different script definition
 */
bool isOverridden(const VariableEntry &entry);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
