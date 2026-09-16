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

#include "flagwarnings.h"

#include <QColor>
#include <QFont>
#include <QLabel>
#include <QTextDocument>
#include <QTimer>

FlagWarnings::FlagWarnings(QLabel *label, QTextDocument *parent) :
    QSyntaxHighlighter(parent), isWarning(QStringLiteral("^(ERROR|WARNING).*$")),
    isURL(QStringLiteral("^.*(https://docs.lammps.org/err[0-9]+).*$")), summary(label),
    document(parent)
{
    nwarnings = nlines = 0;
    oldwarnings = oldlines = -1;

    formatWarning.setForeground(QColorConstants::Red);
    formatWarning.setFontWeight(QFont::Bold);
    formatURL.setForeground(QColorConstants::Blue);
    formatURL.setFontWeight(QFont::Bold);
}

QString FlagWarnings::summaryText(int nwarnings, int nlines)
{
    return QString("%1 Warnings / Errors - %2 Lines").arg(nwarnings).arg(nlines);
}

void FlagWarnings::reset()
{
    nwarnings = nlines = 0;
    oldwarnings = oldlines = -1;
    if (summary) summary->setText(summaryText(0, 0));
}

void FlagWarnings::highlightBlock(const QString &text)
{
    // nothing to do for empty lines
    if (text.isEmpty()) return;

    // highlight errors or warnings
    auto match = isWarning.match(text);
    if (match.hasMatch()) {
        ++nwarnings;
        setFormat(match.capturedStart(0), match.capturedLength(0), formatWarning);
    }

    // highlight ErrorURL links; the cheap test first, since this runs on every
    // line of the log and the pattern has to scan the whole line to fail
    if (text.contains(QLatin1String("docs.lammps.org/err"))) {
        match = isURL.match(text);
        if (match.hasMatch()) {
            setFormat(match.capturedStart(1), match.capturedLength(1), formatURL);
        }
    }

    // A run appends its output in chunks of many lines at a time, and every
    // one of them lands here.  The summary label is refreshed once per chunk,
    // after the event loop gets control back, rather than once per line.
    if (document && summary && !summaryPending) {
        summaryPending = true;
        QTimer::singleShot(0, this, &FlagWarnings::updateSummary);
    }
}

void FlagWarnings::updateSummary()
{
    summaryPending = false;
    if (!document || !summary) return;
    nlines = document->lineCount();
    if ((nwarnings != oldwarnings) || (nlines != oldlines)) {
        oldwarnings = nwarnings;
        oldlines    = nlines;
        summary->setText(summaryText(nwarnings, nlines));
    }
}

// Local Variables:
// c-basic-offset: 4
// End:
