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

#ifndef FIND_AND_REPLACE_H
#define FIND_AND_REPLACE_H

#include <QDialog>

class CodeEditor;
class QLineEdit;
class QCheckBox;

/**
 * @brief Find and Replace dialog for the code editor
 *
 * FindAndReplace provides a dialog for searching and replacing text
 * in the CodeEditor. It supports case-sensitive/insensitive search,
 * whole word matching, text wrapping, and batch replace operations.
 * The dialog is non-modal so users can continue editing while searching.
 */
class FindAndReplace : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param _editor Pointer to the CodeEditor to search in
     * @param parent Parent widget
     */
    explicit FindAndReplace(CodeEditor *_editor, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~FindAndReplace() override = default;

    FindAndReplace()                                  = delete;
    FindAndReplace(const FindAndReplace &)            = delete;
    FindAndReplace(FindAndReplace &&)                 = delete;
    FindAndReplace &operator=(const FindAndReplace &) = delete;
    FindAndReplace &operator=(FindAndReplace &&)      = delete;

private slots:
    void findNext();    ///< Find next occurrence of search text
    void replaceNext(); ///< Replace current match and find next
    void replaceAll();  ///< Replace all occurrences in document
    void quit();        ///< Quit the entire application (via LammpsGui::quit)

private:
    CodeEditor *editor;                 ///< Editor to search in
    QLineEdit *search, *replace;        ///< Search and replacement text fields
    QCheckBox *withcase, *wrap, *whole; ///< Options: case-sensitive, wrap around, whole words
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
