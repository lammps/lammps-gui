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

#ifndef FLAGWARNINGS_H
#define FLAGWARNINGS_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QLabel;
class QTextDocument;

/**
 * @brief Syntax highlighter for LAMMPS warning and error messages
 *
 * FlagWarnings extends QSyntaxHighlighter to detect and highlight
 * warning and error messages in LAMMPS log output. It also detects
 * and highlights URLs, enabling easy navigation and documentation access.
 * The class maintains a count of warnings and updates a summary label.
 */
class FlagWarnings : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param label Optional label to display warning count summary
     * @param parent Text document to apply highlighting to
     */
    explicit FlagWarnings(QLabel *label = nullptr, QTextDocument *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~FlagWarnings() override = default;

    FlagWarnings()                                = delete;
    FlagWarnings(const FlagWarnings &)            = delete;
    FlagWarnings(FlagWarnings &&)                 = delete;
    FlagWarnings &operator=(const FlagWarnings &) = delete;
    FlagWarnings &operator=(FlagWarnings &&)      = delete;

    /**
     * @brief Get the current number of warnings detected
     * @return Number of warnings found in the document
     */
    int getNWarnings() const { return nwarnings; }

    /**
     * @brief Clear the warning and line counters
     *
     * The counters are accumulated across calls to highlightBlock() and are
     * never decremented, so they must be cleared explicitly when the attached
     * document is reused for new content.  Also resets the summary label to
     * its empty-document text.
     */
    void reset();

    /**
     * @brief The text of the summary label for the given counters
     * @param nwarnings Number of warnings/errors counted
     * @param nlines Number of lines counted
     * @return Formatted summary text
     *
     * The one place the format lives; also used to seed the label before any
     * highlighting has run.
     */
    static QString summaryText(int nwarnings, int nlines);

protected:
    /**
     * @brief Highlight a single block (line) of text
     * @param text Text to highlight
     *
     * Searches for warning/error patterns and URLs, applies formatting,
     * and updates warning count.
     */
    void highlightBlock(const QString &text) override;

private:
    /**
     * @brief Refresh the summary label from the counters
     *
     * Deferred to the event loop and coalesced, so that a batch of appended
     * lines refreshes the label once rather than once per line.
     */
    void updateSummary();

    QRegularExpression isWarning;  ///< Pattern for warning/error messages
    QRegularExpression isURL;      ///< Pattern for URLs
    QTextCharFormat formatWarning; ///< Format for warnings/errors
    QTextCharFormat formatURL;     ///< Format for URLs
    QLabel *summary;               ///< Label to display warning summary
    QTextDocument *document;       ///< Document being highlighted
    int nwarnings, oldwarnings;    ///< Current and previous warning count
    int nlines, oldlines;          ///< Current and previous line count
    bool summaryPending = false;   ///< A summary refresh is queued on the event loop
};
#endif
// Local Variables:
// c-basic-offset: 4
// End:
