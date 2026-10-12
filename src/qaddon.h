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

#ifndef QADDON_H
#define QADDON_H

#include <QCompleter>
#include <QFrame>
#include <QStringList>
#include <QValidator>
#include <QWidget>

class QPaintEvent;

/**
 * @brief List of color names accepted by the LAMMPS dump image command
 *
 * Shared between the color completers/validators of the Image Viewer
 * dialogs and the editor's syntax registry.
 *
 * @return sorted list of the valid color names
 */
const QStringList &lammpsImageColors();

/**
 * @brief Horizontal line widget for visual separation
 *
 * Provides a simple horizontal line widget for grouping UI elements
 * in forms or dialogs.
 */
class QHline : public QFrame {

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    QHline(QWidget *parent = nullptr);
};

/**
 * @brief Auto-completer for color name inputs
 *
 * Provides auto-completion for color names in text fields, suggesting
 * valid color names for use with the LAMMPS dump image command.
 */
class QColorCompleter : public QCompleter {
public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    QColorCompleter(QWidget *parent = nullptr);
};

/**
 * @brief Validator for color name inputs
 *
 * Ensures color inputs are names from the LAMMPS color list.
 * Fix-up trims whitespace and lowercases the input.
 */
class QColorValidator : public QValidator {
public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    QColorValidator(QWidget *parent = nullptr);

    /**
     * @brief Attempt to fix invalid color input
     * @param input String to fix (modified in place)
     */
    void fixup(QString &input) const override;

    /**
     * @brief Validate color input string
     * @param input String to validate
     * @param pos Cursor position (unused)
     * @return Validation state (Invalid, Intermediate, Acceptable)
     */
    QValidator::State validate(QString &input, int &pos) const override;
};

/**
 * @brief Widget displaying text rotated 90 degrees counter-clockwise
 *
 * Renders text vertically, optimized for y-axis titles in charts
 * where horizontal space is limited.
 */
class VerticalLabel : public QWidget {
public:
    /**
     * @brief Constructor
     * @param text Text to display
     * @param parent Parent widget
     */
    explicit VerticalLabel(const QString &text, QWidget *parent = nullptr);

    /**
     * @brief Update the displayed text
     * @param text New text to display
     */
    void setText(const QString &text);

    /**
     * @brief Get the displayed text
     * @return Current text
     */
    QString text() const;

protected:
    /** event handler for requests to draw all or parts of the custom widget */
    void paintEvent(QPaintEvent *event) override;
    /** return the recommended size for the widget */
    QSize sizeHint() const override;
    /** return the recommended minimum size for the widget */
    QSize minimumSizeHint() const override;

private:
    QString m_text; ///< Text to display
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
