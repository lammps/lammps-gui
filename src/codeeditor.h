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

#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QHash>
#include <QMap>
#include <QPlainTextEdit>
#include <QString>
#include <QStringList>

#include "inputvariables.h"

class QAbstractItemView;
class QCompleter;
class QContextMenuEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QFont;
class QKeyEvent;
class QMimeData;
class QPaintEvent;
class QRect;
class QResizeEvent;
class QShortcut;
class QWidget;

class LammpsSyntax;

/**
 * @brief Custom text editor with LAMMPS syntax support and auto-completion
 *
 * The CodeEditor class extends QPlainTextEdit to provide specialized features
 * for editing LAMMPS input scripts:
 * - Line numbers in a margin area
 * - Syntax highlighting via Highlighter
 * - Context-aware auto-completion for LAMMPS commands
 * - Automatic indentation and formatting
 * - Context menu with LAMMPS-specific help
 * - Line highlighting for error visualization
 */
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget (typically the main window)
     */
    CodeEditor(QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~CodeEditor() override;

    CodeEditor()                              = delete;
    CodeEditor(const CodeEditor &)            = delete;
    CodeEditor(CodeEditor &&)                 = delete;
    CodeEditor &operator=(const CodeEditor &) = delete;
    CodeEditor &operator=(CodeEditor &&)      = delete;

    /**
     * @brief Set the syntax registry used for completion and help lookups
     * @param newsyntax Syntax registry (not owned)
     */
    void setSyntax(const LammpsSyntax *newsyntax) { syntax = newsyntax; }

    /**
     * @brief Paint line numbers in the line number area
     * @param event Paint event to handle
     */
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    /**
     * @brief Calculate width needed for line number area
     * @return Width in pixels
     */
    int lineNumberAreaWidth();

    /**
     * @brief Set editor font
     * @param newfont Font to use for editor text
     */
    void setFont(const QFont &newfont);

    /**
     * @brief Set cursor to specific text block
     * @param block Block number (line number) to position cursor
     */
    void setCursor(int block);

    /**
     * @brief Highlight a specific line (used for error indication)
     * @param block Block number to highlight
     * @param error true for the error (red) highlight, false for the normal (green) one
     */
    void setHighlight(int block, bool error);

    /**
     * @brief Enable/disable automatic reformatting on Enter key
     * @param flag true to enable, false to disable
     */
    void setReformatOnReturn(bool flag) { reformatOnReturn = flag; }

    /**
     * @brief Enable/disable automatic completion popup
     * @param flag true to enable, false to disable
     */
    void setAutoComplete(bool flag) { automaticCompletion = flag; }

    /**
     * @brief Reformat a line with proper indentation
     * @param line Line to reformat
     * @return Reformatted line
     */
    QString reformatLine(const QString &line);

    /**
     * @brief Set word list for LAMMPS command completion
     * @param words List of command names
     */
    void setCommandList(const QStringList &words);

    /**
     * @brief Set word list for fix style completion
     * @param words List of fix style names
     */
    void setFixList(const QStringList &words);

    /**
     * @brief Set word list for compute style completion
     * @param words List of compute style names
     */
    void setComputeList(const QStringList &words);

    /**
     * @brief Set word list for dump style completion
     * @param words List of dump style names
     */
    void setDumpList(const QStringList &words);

    /**
     * @brief Set word list for atom style completion
     * @param words List of atom style names
     */
    void setAtomList(const QStringList &words);

    /**
     * @brief Set word list for pair style completion
     * @param words List of pair style names
     */
    void setPairList(const QStringList &words);

    /**
     * @brief Set word list for bond style completion
     * @param words List of bond style names
     */
    void setBondList(const QStringList &words);

    /**
     * @brief Set word list for angle style completion
     * @param words List of angle style names
     */
    void setAngleList(const QStringList &words);

    /**
     * @brief Set word list for dihedral style completion
     * @param words List of dihedral style names
     */
    void setDihedralList(const QStringList &words);

    /**
     * @brief Set word list for improper style completion
     * @param words List of improper style names
     */
    void setImproperList(const QStringList &words);

    /**
     * @brief Set word list for kspace style completion
     * @param words List of kspace style names
     */
    void setKspaceList(const QStringList &words);

    /**
     * @brief Set word list for region style completion
     * @param words List of region style names
     */
    void setRegionList(const QStringList &words);

    /**
     * @brief Set word list for integration style completion
     * @param words List of integration style names
     */
    void setIntegrateList(const QStringList &words);

    /**
     * @brief Set word list for minimization style completion
     * @param words List of minimization style names
     */
    void setMinimizeList(const QStringList &words);

    /**
     * @brief Set word list for variable style completion
     * @param words List of variable style names
     */
    void setVariableList(const QStringList &words);

    /**
     * @brief Set word list for units style completion
     * @param words List of units style names
     */
    void setUnitsList(const QStringList &words);

    /**
     * @brief Set extra word list for completion
     * @param words List of extra words
     */
    void setExtraList(const QStringList &words);

    /**
     * @brief Set list of color names for completion
     * @param words List of color names
     */
    void setColorList(const QStringList &words);

    /**
     * @brief Set list of dump image keywords for completion
     * @param words List of dump image keywords
     */
    void setImageKwList(const QStringList &words);

    /**
     * @brief Update group ID list from the editor buffer
     */
    void setGroupList();

    /**
     * @brief Update variable name list from the editor buffer and LAMMPS instance
     */
    void setVarNameList();

    /**
     * @brief Update compute ID list from the editor buffer
     */
    void setComputeIDList();

    /**
     * @brief Update fix ID list from the editor buffer
     */
    void setFixIDList();

    /**
     * @brief Update file list from current directory
     */
    void setFileList();

    /**
     * @brief Set the variables list used to mark overridden index variable values
     *
     * Entries whose value overrides the definition in the input script (see
     * isOverridden()) get a thin frame drawn around the value text of their
     * definition line and a tooltip showing the overriding value.
     *
     * @param vars Current variable entries (parse results plus dialog edits)
     */
    void setVariableOverrides(const QList<VariableEntry> &vars);

    /**
     * @brief Constant for disabled highlighting
     */
    static constexpr int NO_HIGHLIGHT = 1 << 30;

protected:
    /**
     * @brief Handle resize events to update line number area
     * @param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Paint the editor and frame overridden index variable values
     * @param event The paint event
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief Handle tooltip events for overridden index variable values
     * @param event The event to handle
     * @return true if the event was handled
     */
    bool event(QEvent *event) override;

    /**
     * @brief Check if MIME data can be inserted (for drag-and-drop)
     * @param source The MIME data to check
     * @return true if data can be inserted
     */
    bool canInsertFromMimeData(const QMimeData *source) const override;

    /**
     * @brief Handle drag enter events
     * @param event The drag enter event
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief Handle drag leave events
     * @param event The drag leave event
     */
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    /**
     * @brief Handle drop events
     * @param event The drop event
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * @brief Handle context menu events
     * @param event The context menu event
     */
    void contextMenuEvent(QContextMenuEvent *event) override;

    /**
     * @brief Handle key press events (for auto-completion and formatting)
     * @param event The key event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * @brief Set LAMMPS documentation version for help links
     */
    void setDocver();

private slots:
    /**
     * @brief Update line number area width when block count changes
     * @param newBlockCount New number of text blocks
     */
    void updateLineNumberAreaWidth(int newBlockCount);

    /**
     * @brief Update line number area display
     * @param rect Rectangle to update
     * @param dy Vertical scroll amount
     */
    void updateLineNumberArea(const QRect &rect, int dy);

    /**
     * @brief Show help for word at cursor
     */
    void getHelp();

    /**
     * @brief Open help URL in browser
     */
    void openHelp();

    /**
     * @brief Open URL at cursor in browser
     */
    void openUrl();

    /**
     * @brief View file at cursor
     */
    void viewFile();

    /**
     * @brief Inspect file at cursor
     */
    void inspectFile();

    /**
     * @brief Reformat current line with proper indentation
     */
    void reformatCurrentLine();

    /**
     * @brief Trigger auto-completion popup
     */
    void runCompletion();

    /**
     * @brief Insert selected completion text
     * @param completion The text to insert
     */
    void insertCompletedCommand(const QString &completion);

    /**
     * @brief Comment out selected lines
     */
    void commentSelection();

    /**
     * @brief Uncomment selected lines
     */
    void uncommentSelection();

    /**
     * @brief Comment out current line
     */
    void commentLine();

    /**
     * @brief Uncomment current line
     */
    void uncommentLine();

private:
    /**
     * @brief Find help page and section for a command
     * @param page Output parameter for help page name
     * @param help Output parameter for help section
     */
    void findHelp(QString &page, QString &help);

    /**
     * @brief Pop up (or hide) the completion list of the active completer
     * @param prefix   Word (prefix) under the cursor to complete
     * @param oldPopup Popup of the previously active completer, hidden if different
     */
    void popupCompletion(const QString &prefix, QAbstractItemView *oldPopup);

    QWidget *lineNumberArea; ///< Widget for displaying line numbers
    QShortcut *helpAction;   ///< Keyboard shortcut for help

    const LammpsSyntax *syntax = nullptr; ///< Syntax registry (not owned)

    /// @brief Auto-completion objects for different LAMMPS command contexts
    QCompleter *currentComp, *commandComp, *fixComp, *computeComp, *dumpComp, *atomComp, *pairComp,
        *bondComp, *angleComp, *dihedralComp, *improperComp, *kspaceComp, *regionComp,
        *integrateComp, *minimizeComp, *variableComp, *unitsComp, *groupComp, *varnameComp,
        *fixidComp, *compidComp, *fileComp, *extraComp, *colorComp, *imagekwComp;

    /// @brief Overridden index variables by name (only entries where isOverridden() is true)
    QHash<QString, VariableEntry> variableOverrides;

    int highlight;            ///< Current highlighted line number, NO_HIGHLIGHT if none
    bool highlighterror;      ///< Highlighted line marks an error (red) instead of progress
    bool reformatOnReturn;    ///< Enable auto-reformatting on Enter
    bool automaticCompletion; ///< Enable auto-completion popup
    QString docver;           ///< LAMMPS documentation version string

    /// @brief Maps for LAMMPS command help pages
    QMap<QString, QString> cmdMap;      ///< Command to help page mapping
    QMap<QString, QString> fixMap;      ///< Fix style to help page mapping
    QMap<QString, QString> computeMap;  ///< Compute style to help page mapping
    QMap<QString, QString> pairMap;     ///< Pair style to help page mapping
    QMap<QString, QString> bondMap;     ///< Bond style to help page mapping
    QMap<QString, QString> angleMap;    ///< Angle style to help page mapping
    QMap<QString, QString> dihedralMap; ///< Dihedral style to help page mapping
    QMap<QString, QString> improperMap; ///< Improper style to help page mapping
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
