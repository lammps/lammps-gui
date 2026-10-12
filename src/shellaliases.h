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

#ifndef SHELLALIASES_H
#define SHELLALIASES_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QTableWidget;

/// One alias: the word typed, and what the shell should read instead.
using ShellAlias = QPair<QString, QString>;

/**
 * @brief Editor for the aliases the command window defines in its shell
 *
 * The shell the command window starts reads the user's start-up file, but a
 * section of that file guarded by a test for a terminal stops before it runs,
 * because the shell is reading a pipe.  On several distributions that is where
 * @c ls, @c ll and @c l. are defined, so those go missing while every other
 * alias is present.  Programs behave differently for the same reason: @c ls
 * lists one entry per line rather than in columns unless it is told otherwise.
 *
 * Rather than give the shell a terminal, this dialog lets those few aliases be
 * stated once and defines them in every shell the command window starts.  The
 * defaults cover the common case -- @c ls and @c ll with the arguments that
 * restore the output a terminal would have produced.
 *
 * Only what is in the table is sent, and only when a shell starts or the table
 * is accepted; nothing else ever reaches the shell without being typed.
 *
 * @see CommandWindow
 */
class ShellAliases : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit ShellAliases(QWidget *parent = nullptr);

    ~ShellAliases() override                      = default;
    ShellAliases()                                = delete;
    ShellAliases(const ShellAliases &)            = delete;
    ShellAliases(ShellAliases &&)                 = delete;
    ShellAliases &operator=(const ShellAliases &) = delete;
    ShellAliases &operator=(ShellAliases &&)      = delete;

    /**
     * @brief The aliases to define, in the order they are listed
     * @return Name and definition of each alias
     *
     * The defaults are returned until the dialog has been accepted once, so a
     * new installation starts with @c ls and @c ll already useful.
     */
    static QList<ShellAlias> aliases();

    /**
     * @brief The aliases a new installation starts with
     * @return Name and definition of each alias
     */
    static QList<ShellAlias> defaults();

public slots:
    /// Store the table and close.
    void accept() override;

private slots:
    void addRow();        ///< Append an empty row and start editing it
    void removeRow();     ///< Drop the selected rows
    void resetDefaults(); ///< Put the table back to @ref defaults()

private:
    /// Fill the table from a list, replacing what is in it.
    void setRows(const QList<ShellAlias> &list);

    /// Read the table back, dropping rows with no name.
    QList<ShellAlias> rows() const;

    QTableWidget *table; ///< Name and definition, one alias per row
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
