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

#ifndef IMAGEVIEWER_INTERNAL_H
#define IMAGEVIEWER_INTERNAL_H

// Implementation-detail symbols shared between imageviewer.cpp and
// imageviewersettings.cpp (the dialog builders). Not part of any public API.

#include <QColor>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QComboBox;
class QWidget;

// ---- shared UI constants -------------------------------------------------
inline const QString blank(" ");
inline constexpr double VDW_ON           = 1.6;
inline constexpr double VDW_OFF          = 0.5;
inline constexpr double VDW_CUT          = 1.0;
inline constexpr double SHINY_ON         = 0.6;
inline constexpr double SHINY_OFF        = 0.2;
inline constexpr double SHINY_CUT        = 0.4;
inline constexpr double ZOOM_MIN         = 0.1;
inline constexpr double ZOOM_MAX         = 10.0;
inline constexpr int DEFAULT_NPOINTS     = 100000;
inline constexpr double DEFAULT_DIAMETER = 0.2;
inline constexpr double DEFAULT_OPACITY  = 0.5;
inline constexpr int TITLE_MARGIN        = 10;
inline constexpr int CONTENT_MARGIN      = 5;
inline constexpr int LAYOUT_SPACING      = 6;
inline constexpr int MINIMUM_WIDTH       = 400;
inline constexpr int MINIMUM_HEIGHT      = 300;
inline constexpr int EXTRA_WIDTH         = 150;
inline constexpr int EXTRA_HEIGHT        = 100;
inline constexpr int RESET_ALL_COLORS    = 10;
inline constexpr int ICON_SIZE           = 48;

// ---- shared enumerations -------------------------------------------------
enum { FRAME, FILLED, TRANSPARENT, POINTS };
enum { TYPE, ELEMENT, CONSTANT };

// needs to be kept in sync with the dump image tri flag values
enum { NONE, TRIANGLES, CYLINDERS, BOTH };

// same list as in dump image
inline const QList<QPair<QString, QColor>> deftypecolors = {
    {{"red"}, {255, 0, 0}},           {{"forestgreen"}, {34, 139, 34}},
    {{"blue"}, {0, 0, 255}},          {{"gold"}, {255, 215, 0}},
    {{"cyan"}, {0, 255, 255}},        {{"magenta"}, {255, 0, 255}},
    {{"silver"}, {192, 192, 192}},    {{"orange"}, {255, 128, 0}},
    {{"lime"}, {0, 255, 0}},          {{"gray"}, {128, 128, 128}},
    {{"darkred"}, {139, 0, 0}},       {{"darkgreen"}, {0, 100, 0}},
    {{"darkblue"}, {0, 0, 139}},      {{"darkcyan"}, {0, 139, 139}},
    {{"darkmagenta"}, {139, 0, 139}}, {{"darkgray"}, {69, 69, 69}}};

// per-bond "compute bond/local" attributes offered for the bond color-by-value
// feature; a single attribute yields a per-bond vector referenced as c_<id>
inline const QStringList bondLocalAttrs = {"dist",   "dx",       "dy",    "dz",    "engpot",
                                           "force",  "fx",       "fy",    "fz",    "engvib",
                                           "engrot", "engtrans", "omega", "velvib"};

// reserved compute ID the GUI creates/destroys to color bonds by a per-bond value
inline const QString bondComputeId = QStringLiteral("imgviewer_bondcolor");

/**
 * @brief Store settings for displaying graphics from a fix or compute in a LAMMPS snapshot image
 */
class ImageInfo {
public:
    ImageInfo() = delete;
    /** Custom constructor */
    ImageInfo(bool _enabled, const QString &_style, int _colorstyle, const QString &_color,
              double _opacity, double _flag1, double _flag2) :
        enabled(_enabled), style(_style), colorstyle(_colorstyle), color(_color), opacity(_opacity),
        flag1(_flag1), flag2(_flag2)
    {
    }

    bool enabled;   ///< display graphics if true
    QString style;  ///< name of style
    int colorstyle; ///< color style for graphics: TYPE, ELEMENT, CONSTANT
    QString color;  ///< custom color of graphics objects for style == CONSTANT
    double opacity; ///< opacity of graphics objects for style == CONSTANT
    double flag1;   ///< Flag #1 for graphics
    double flag2;   ///< Flag #2 for graphics
};

/**
 * @brief Store settings for displaying a region in a LAMMPS snapshot image
 */
class RegionInfo {
public:
    RegionInfo() = delete;
    /** Custom constructor */
    RegionInfo(bool _enabled, int _style, const QString &_color, double _diameter, double _opacity,
               int _npoints) :
        enabled(_enabled), style(_style), color(_color), diameter(_diameter), opacity(_opacity),
        npoints(_npoints)
    {
    }

    bool enabled;    ///< display region if true
    int style;       ///< style of region object: FRAME, FILLED, TRANSPARENT, or POINTS
    QString color;   ///< color of region display
    double diameter; ///< diameter value for POINTS and FRAME
    double opacity;  ///< opacity for TRANSPARENT
    int npoints;     ///< number of points to be used for POINTS style region display
};

// ---- shared free helpers (defined in imageviewer.cpp) --------------------
QPixmap color_icon(const QColor &color);
QIcon gradient_icon(const QList<QPair<double, QColor>> &stops);
QIcon sequence_icon(const QList<QColor> &colors);
QJsonObject loadJsonColors(QWidget *parent);
void saveJsonColors(QWidget *parent, const QJsonArray &colors, const QJsonObject &lights);
void selectComboItem(QComboBox *box, const QString &text);

#endif
