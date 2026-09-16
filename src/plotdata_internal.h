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

#ifndef PLOTDATA_INTERNAL_H
#define PLOTDATA_INTERNAL_H

// Implementation-detail helpers shared between the flat-file parsers in
// plotdata.cpp and the block-file parser in plotblockdata.cpp.  Not part of
// any public API.  Defined in plotdata.cpp.

#include <QLatin1String>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <vector>

/**
 * @brief Whitespace-separated numbers on a line
 *
 * Reads the fields without a regular expression and without a string per
 * field.  Returns false at the first field that is not a number, which is how
 * comment text and stray log output are told apart from data, and false for a
 * line without fields.
 *
 * @param line  The line, leading and trailing whitespace allowed
 * @param row   Receives the numbers (cleared first)
 * @return true if the line consists of numbers only
 */
bool numericFields(QStringView line, std::vector<double> &row);

/**
 * @brief Whether some line of the text, less leading whitespace, starts with the prefix
 *
 * Scans without splitting the text into a list of lines first.
 */
bool anyLineStartsWith(const QString &text, QLatin1String prefix);

/** @brief Placeholder column names "column1", "column2", ... for @p ncol columns */
QStringList genericColumnNames(int ncol);

/** @brief Strip a single layer of matching single or double quotes from a token */
QString unquote(QString t);

/** @brief The text between the first '[' and the last ']' (empty if not found) */
QString bracketContents(const QString &s);

/** @brief The "\\s+" splitter shared by the line scanners */
const QRegularExpression &whitespaceRe();

#endif
// Local Variables:
// c-basic-offset: 4
// End:
