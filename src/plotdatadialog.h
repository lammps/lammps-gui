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

#ifndef PLOTDATADIALOG_H
#define PLOTDATADIALOG_H

#include "plotblockdata.h"
#include "plotdata.h"

#include <QDialog>
#include <QList>
#include <QPair>
#include <QStringList>
#include <memory>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTableWidget;

/**
 * @brief Dialog to choose which columns of a PlotData to plot
 *
 * Presents the parsed columns of an external data file and lets the user
 * assign a role to each column: exactly one column is the shared x-axis
 * (exclusive radio buttons), any number of columns are plotted on the
 * y-axis (checkboxes), and columns with neither selected are ignored.
 * When the x-axis selection moves to a different column, the previous
 * x-axis column becomes a y-axis column.
 * A small preview of the first rows is shown to help identify column content.
 *
 * A "Compute derived column" section at the bottom lets the user add derived columns
 * from expressions that reference the existing columns by name in braces, like
 * Python format strings (e.g. @c {nfcc}/{ntot} or @c {load}*1.602176634).
 * @c {name:first}, @c {name:last}, @c {name:min}, @c {name:max}, and
 * @c {name:mean} are per-column constants, and @c row is the 0-based row
 * index.  Each column has exactly one live name: renaming it takes effect
 * immediately, updates the preview, and rewrites the references in already
 * added derived columns.  See substituteColumnRefs() in customfunc.h.
 *
 * For a block-structured `fix ave/\*` file (see plotblockdata.h) the dialog
 * grows a "Data blocks" group above the column grid, which reduces the blocks
 * to the single flat table the columns below then refer to: either one chosen
 * block, or the average of a range of them with error bars.  Changing the
 * reduction rebuilds the column grid, keeping the roles and names the user has
 * already assigned and re-evaluating any derived columns against the new table.
 */
class PlotDataDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor for a flat data table
     * @param data   Parsed column data to choose from (stored as a working copy)
     * @param parent Parent widget
     */
    explicit PlotDataDialog(const PlotData &data, QWidget *parent = nullptr);

    /**
     * @brief Constructor for a block-structured fix ave/\* file
     * @param blocks Parsed blocks (stored as a working copy)
     * @param parent Parent widget
     *
     * Starts out on the reduction that suits the detected format, which the
     * "Data blocks" group then lets the user change.
     */
    explicit PlotDataDialog(const PlotBlockData &blocks, QWidget *parent = nullptr);

    /**
     * @brief Read a data file and build the column dialog for it
     *
     * The block-structured output of the fix ave/* styles is tried first,
     * since it is not a flat table and gets the dialog that can reduce it to
     * one; anything else goes through the flat-file parsers.
     *
     * @param fileName  File to read
     * @param parent    Parent widget of the dialog
     * @param error     Receives what the parsers reported, or the file name
     *                  when they reported nothing; untouched on success
     * @return The dialog, or nullptr when the file could not be read
     */
    static std::unique_ptr<PlotDataDialog> fromFile(const QString &fileName, QWidget *parent,
                                                    QString *error);

    ~PlotDataDialog() override = default;

    PlotDataDialog()                                  = delete;
    PlotDataDialog(const PlotDataDialog &)            = delete;
    PlotDataDialog(PlotDataDialog &&)                 = delete;
    PlotDataDialog &operator=(const PlotDataDialog &) = delete;
    PlotDataDialog &operator=(PlotDataDialog &&)      = delete;

    /**
     * @brief Index of the column used as the x-axis
     *
     * Returns the index of the column whose x-axis radio button is selected.
     * Falls back to 0 if no column is selected as the x-axis.
     * Indices refer to @ref buildData() columns.
     * @return Column index
     */
    int xColumn() const;

    /**
     * @brief Indices of the columns selected to plot on the y-axis
     *
     * Indices refer to @ref buildData() columns.
     * @return List of column indices (all columns with a checked y checkbox)
     */
    QList<int> yColumns() const;

    /**
     * @brief Return the working data with renames and derived columns applied
     *
     * Includes any columns added via the "Compute derived column" section;
     * renames are already committed when they are made, so this is the
     * working copy as it stands.
     * @return Updated PlotData ready for plotting
     */
    PlotData buildData() const;

    /**
     * @brief Error bars belonging to the columns of @ref buildData()
     *
     * Only a block average produces them; everything else returns entries that
     * are all empty.  Indexed like the columns of @ref buildData().
     * @return Per-column error bars
     */
    PlotErrors buildErrors() const;

private slots:
    /** @brief Evaluate the derived-column expression and append the new column */
    void computeColumn();
    /** @brief Re-reduce the blocks after a change in the "Data blocks" group */
    void applyReduction();
    /**
     * @brief Commit an edited column name (connected to the name editors)
     *
     * Renames the column in the working data, updates the preview header, and
     * rewrites the braced references in stored derived-column expressions so
     * they keep meaning the same column.  An empty, duplicate, or syntactically
     * unusable name (braces or colon) is refused and the editor reverted.
     */
    void commitRename();

private:
    /** @brief Assemble the dialog, with the block group first if there is one */
    void buildUi();
    /** @brief Build the "Data blocks" reduction controls */
    QGroupBox *buildBlockGroup();
    /** @brief Append one row (x radio button + y checkbox with @p checked state + name editor) */
    void appendColumnRow(const QString &name, bool checked);
    /** @brief Replace the per-column rows, preserving the roles and names in place */
    void rebuildColumnRows();
    /** @brief Refill the preview table from the current working data */
    void refreshPreview();
    /** @brief Update the step labels and the enabled state in the block group */
    void updateBlockLabels();
    /** @brief Error type currently selected in the block group */
    BlockErrorType errorType() const;
    /**
     * @brief Evaluate a derived-column expression over the working data
     * @param expr   Expression referencing columns as @c {name} (see customfunc.h)
     * @param values Out-parameter receiving one value per row
     * @return An error message, or an empty string on success
     */
    QString evaluateColumn(const QString &expr, std::vector<double> &values) const;
    /**
     * @brief What is wrong with a proposed column name
     * @param name The name to check
     * @return An error message, or an empty string when the name can be used
     */
    QString checkNewName(const QString &name) const;

    PlotData workingData;          ///< Working copy of the data; derived cols appended here
    PlotErrors workingErrors;      ///< Error bars parallel to the working data columns
    PlotBlockData blockData;       ///< Parsed blocks (empty for a flat file)
    int defaultXColumn = 0;        ///< x column preselected on the first build
    QList<int> defaultYColumns;    ///< y columns preselected on the first build
    bool resetColumnRoles = false; ///< next rebuild starts from the defaults again
    QList<QPair<QString, QString>> derivedColumns; ///< name/expression of the derived columns

    QGridLayout *colsLayout;    ///< Layout of the per-column rows (for dynamic addition)
    QButtonGroup *xgroup;       ///< Exclusive group of x selection radio buttons (id = column)
    QList<QCheckBox *> ychecks; ///< per-column y selection checkboxes
    QList<QLineEdit *> ynames;  ///< per-column name editors
    QLabel *sizeLabel;          ///< "N rows, M columns" summary above the column grid
    QTableWidget *preview;      ///< Preview of the first rows (nullptr if there are none)
    QLineEdit *deriveNameEdit;  ///< Name field for the new derived column
    QLineEdit *deriveExprEdit;  ///< Expression field for the new derived column

    QComboBox *kindCombo;      ///< Detected file format, overridable by the user
    QRadioButton *avgRadio;    ///< Reduce by averaging a range of blocks
    QSpinBox *firstSpin;       ///< First block of the averaged range (1-based)
    QSpinBox *lastSpin;        ///< Last block of the averaged range (1-based)
    QComboBox *errCombo;       ///< Which uncertainty the average reports
    QRadioButton *singleRadio; ///< Reduce by picking a single block
    QSpinBox *blockSpin;       ///< The single block to show (1-based)
    QLabel *rangeLabel;        ///< Timesteps covered by the averaged range
    QLabel *blockLabel;        ///< Timestep of the single selected block
    QLabel *statusLabel;       ///< What the current reduction actually did
};

#endif

// Local Variables:
// c-basic-offset: 4
// End:
