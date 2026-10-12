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

#ifndef TUTORIALS_H
#define TUTORIALS_H

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Metadata for one collection of LAMMPS-GUI tutorials
 *
 * LAMMPS-GUI ships support for multiple, independently hosted tutorial
 * collections (e.g. the molecular soft-matter set and the materials-science
 * set).  Each collection is a numbered series of tutorials laid out identically
 * -- one `files/tutorial<N>/` folder per tutorial with a `.manifest` and an
 * optional `solution/` subfolder -- so the only thing that varies between
 * collections is this metadata.  The single source of truth is the table in
 * tutorials.cpp, consumed by the Tutorials menu and the TutorialWizard.
 */
struct TutorialCollection {
    QString key;          ///< stable identifier, e.g. "softmatter", "matsci"
    QString name;         ///< display name for the menu and wizard
    QString dirPrefix;    ///< working-directory folder prefix (e.g. "tutorial")
    QString author;       ///< short author/attribution line for the intro page
    QString filesUrl;     ///< raw files base, "...tutorial%1/%2" (number, filename)
    QString filesRepoUrl; ///< human-facing repository URL shown on the intro page
    QString webUrl;       ///< per-tutorial web page pattern, "...%1/%2.html" (number, slug)
    QString siteUrl;      ///< collection landing page (fallback web page)
    QString logo;         ///< logo resource; may contain "%1" for per-tutorial logos
    QStringList titles;   ///< short per-tutorial titles (count() entries)
    QStringList slugs;    ///< per-tutorial web-page slugs (count() entries, or empty)
    QStringList blurbs;   ///< per-tutorial HTML descriptions (count() entries)
    bool published;       ///< true once the whole collection is released (hides the teaser label)
    QString status;       ///< menu label for an unpublished collection ("coming soon", "planned")
    int available = 0;    ///< number of leading tutorials that are launchable now; the
                          ///< remaining count()-available entries appear as disabled teasers

    /** @brief Number of tutorials in this collection */
    int count() const { return blurbs.size(); }
    /** @brief Logo resource for tutorial @p n (1-based); resolves a "%1" pattern */
    QString logoFor(int n) const { return logo.contains("%1") ? logo.arg(n) : logo; }
};

/** @brief The available tutorial collections, in display order */
const QList<TutorialCollection> &tutorialCollections();

/** @brief Collection at @p index, clamped to a valid entry (0 if out of range) */
const TutorialCollection &tutorialCollection(int index);

#endif

// Local Variables:
// c-basic-offset: 4
// End:
