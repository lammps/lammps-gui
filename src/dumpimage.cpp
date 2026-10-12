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

#include "dumpimage.h"

#include "constants.h"

#include "colormaps.h"

#include <QRegularExpression>
#include <algorithm>

// LAMMPS dump_image built-in defaults. The builder emits a color, color map,
// light, or transparency setting only when it differs from these, so the
// generated command stays compact without changing the rendered image: the
// GUI's own defaults were reconciled to match LAMMPS (the per-type color table
// in deftypecolors mirrors the LAMMPS color database). Deliberate GUI
// divergences (the backcolor2 gradient and shiny) are emitted unconditionally.
constexpr double DEF_TRANS     = 1.0; // fully opaque
constexpr double DEF_AMBIENT   = 0.0;
constexpr double DEF_KEYLIGHT  = 0.9;
constexpr double DEF_FILLLIGHT = 0.45;
constexpr double DEF_BACKLIGHT = 0.9;
constexpr double DEF_GAMMA     = 1.0; // no gamma adjustment
const QString DEF_BOXCOLOR     = QStringLiteral("gold");
const QString DEF_BACKCOLOR    = QStringLiteral("black");

// Append region visualization arguments to the dump-image command.
static void appendRegionArgs(QString &cmd, const DumpImageParams &p)
{
    for (const auto &reg : p.regions) {
        if (reg.second->enabled) {
            QString id(reg.first.c_str());
            const QString &color = reg.second->color;
            switch (reg.second->style) {
                case FRAME:
                    cmd += " region " + id + blank + color;
                    cmd += " frame " + QString::number(reg.second->diameter);
                    cmd += " hull_points " + QString::number(reg.second->npoints);
                    break;
                case FILLED:
                    cmd += " region " + id + blank + color + " filled";
                    cmd += " hull_points " + QString::number(reg.second->npoints);
                    break;
                case TRANSPARENT:
                    cmd += " region " + id + blank + color;
                    cmd += " transparent " + QString::number(reg.second->opacity);
                    cmd += " hull_points " + QString::number(reg.second->npoints);
                    break;
                case POINTS:
                default:
                    cmd += " region " + id + blank + color;
                    cmd += " points " + QString::number(reg.second->npoints);
                    cmd += blank + QString::number(reg.second->diameter);
                    break;
            }
            cmd += blank;
        }
    }
}

// Append the per-fix/compute draw styles. Returns true if any fix or compute is
// active (the caller suppresses "noinit" in that case).
static bool appendFixComputeStyles(QString &cmd, const DumpImageParams &p)
{
    bool dofixes = false;
    for (const auto &comp : p.computes) {
        if (comp.second->enabled) {
            dofixes = true;
            QString id(comp.first.c_str());
            switch (comp.second->colorstyle) {
                case TYPE:
                    cmd += " compute " + id + blank + "type ";
                    break;
                case ELEMENT:
                    cmd += " compute " + id + blank + "element ";
                    break;
                case CONSTANT: // FALLTHROUGH
                default:
                    cmd += " compute " + id + blank + "const ";
                    break;
            }
            cmd += QString::number(comp.second->flag1) + blank +
                   QString::number(comp.second->flag2) + blank;
        }
    }
    for (const auto &fix : p.fixes) {
        if (fix.second->enabled) {
            dofixes = true;
            QString id(fix.first.c_str());
            switch (fix.second->colorstyle) {
                case TYPE:
                    cmd += " fix " + id + blank + "type ";
                    break;
                case ELEMENT:
                    cmd += " fix " + id + blank + "element ";
                    break;
                case CONSTANT: // FALLTHROUGH
                default:
                    cmd += " fix " + id + blank + "const ";
                    break;
            }
            cmd += QString::number(fix.second->flag1) + blank + QString::number(fix.second->flag2) +
                   blank;
        }
    }
    return dofixes;
}

// Append the definition of a color map. @p kw is the dump_modify keyword
// ("amap" for atoms, "bmap" for bonds); @p pfx prefixes the custom color-stop
// names so an atom map and a bond map with different gradients do not collide in
// the global color namespace (atoms use "map", bonds use "bm").  When @p reverse
// is set the map is mirrored: the stop order is flipped, and for continuous maps
// the positions are remapped (pos -> 1 - pos) so they stay ascending and the
// gradient is inverted (e.g. "RWB" reversed renders identically to "BWR").
static void appendColorMapArgs(QString &cmd, const QString &kw, const QString &colormap,
                               const QString &mapmin, const QString &mapmax, const QString &pfx,
                               bool reverse)
{
    const ColorMapDef &def = colorMapDef(colormap);
    const QString mmin     = (mapmin == "auto") ? QStringLiteral("min") : mapmin;
    const QString mmax     = (mapmax == "auto") ? QStringLiteral("max") : mapmax;

    // work on a local copy of the stops so the optional reversal never mutates
    // the shared definition returned by colorMapDef()
    QList<ColorMapStop> stops = def.stops;
    if (reverse) {
        std::reverse(stops.begin(), stops.end());
        if (def.continuous)
            for (auto &s : stops)
                s.pos = 1.0 - s.pos;
    }

    // Resolve each stop to a color token: a LAMMPS named color used verbatim, or
    // a custom color "<pfx><n>" defined once via "color <pfx><n> R G B".  Distinct
    // RGB values share a single custom color, numbered in order of first use.
    QStringList colorref; // per-stop color token, parallel to stops
    QStringList rgbseen;  // distinct "R G B" strings; index + 1 -> custom number
    for (const auto &s : stops) {
        if (!s.name.isEmpty()) {
            colorref.append(s.name);
            continue;
        }
        const QString rgb =
            QStringLiteral("%1 %2 %3").arg(s.r, 0, 'f', 3).arg(s.g, 0, 'f', 3).arg(s.b, 0, 'f', 3);
        int idx = rgbseen.indexOf(rgb);
        if (idx < 0) {
            rgbseen.append(rgb);
            idx = rgbseen.size() - 1;
            cmd += " color " + pfx + QString::number(idx + 1) + " " + rgb;
        }
        colorref.append(pfx + QString::number(idx + 1));
    }

    const int n = stops.size();
    if (def.continuous) {
        cmd += QString(" %1 %2 %3 cf 0.0 %4").arg(kw, mmin, mmax).arg(n);
        for (int i = 0; i < n; ++i) {
            const QString pos = (i == 0)       ? QStringLiteral("min")
                                : (i == n - 1) ? QStringLiteral("max")
                                               : QString::number(stops[i].pos);
            cmd += " " + pos + " " + colorref[i];
        }
    } else {
        cmd += QString(" %1 %2 %3 sa 1.0 %4").arg(kw, mmin, mmax).arg(n);
        for (const auto &c : colorref)
            cmd += " " + c;
    }
}

// Append the per-fix/compute color and transparency overrides.
static void appendFixComputeColors(QString &cmd, const DumpImageParams &p)
{
    for (const auto &comp : p.computes) {
        if (comp.second->enabled) {
            QString id(comp.first.c_str());
            const QString &color = comp.second->color;
            cmd += " ccolor " + id + blank + color;
            cmd += " ctrans " + id + blank + QString::number(comp.second->opacity);
            cmd += blank;
        }
    }
    for (const auto &fix : p.fixes) {
        if (fix.second->enabled) {
            QString id(fix.first.c_str());
            const QString &color = fix.second->color;
            cmd += " fcolor " + id + blank + color;
            cmd += " ftrans " + id + blank + QString::number(fix.second->opacity);
            cmd += blank;
        }
    }
}

DumpImageCommand buildDumpImageCommand(const DumpImageParams &p)
{
    QString d; // render options (after "image <N> <file>")
    QString m; // dump_modify options

    const int hhrot   = (p.hrot > 180) ? 360 - p.hrot : p.hrot;
    const bool do_vdw = p.vdwfactor > VDW_CUT;

    // atom color
    const QString atomColorTok = (!p.atomcustom && p.useelements) ? QStringLiteral("element")
                                                                  : p.atomcolor;
    d += blank + atomColorTok;

    // atom diameter
    if (!p.atomcustom) {
        if (p.usediameter && do_vdw)
            d += blank + "diameter";
        else
            d += " type";
    } else {
        if ((p.atomdiam == "diameter") && p.usediameter && do_vdw)
            d += blank + "diameter";
        else if (p.atomdiam.startsWith("v_"))
            d += blank + p.atomdiam;
        else
            d += " type";
    }

    if (!p.showatoms) d += " atom no";
    if (p.showbodies && (p.body_flag == 1)) {
        d += QString(" body %1 %2 %3").arg(p.bodycolor).arg(p.bodydiam).arg(p.bodyflag);
    } else if (p.showlines && (p.line_flag == 1))
        d += QString(" line %1 %2").arg(p.linecolor).arg(p.linediam);
    else if (p.showtris && (p.tri_flag == 1))
        d += QString(" tri %1 %2 %3").arg(p.tricolor).arg(p.triflag).arg(p.tridiam);
    else if (p.showellipsoids && (p.ellipsoid_flag == 1)) {
        d += QString(" ellipsoid %1 %2 %3 %4")
                 .arg(p.ellipsoidcolor)
                 .arg(p.ellipsoidflag)
                 .arg(p.ellipsoidlevel)
                 .arg(p.ellipsoiddiam);
    }
    d += QString(" size %1 %2").arg(p.xsize).arg(p.ysize);
    d += QString(" zoom %1").arg(p.zoom);
    d += QString(" shiny %1 ").arg(p.shinyfactor);
    d += QString(" fsaa %1").arg(p.antialias ? "yes" : "no");
    if (p.nbondtypes > 0) {
        if (do_vdw || !p.showbonds)
            d += " bond none none ";
        else
            d += QString(" bond %1 %2 ").arg(p.bondcolor).arg(p.bonddiam);
    }
    if (p.dimension == 3) {
        d += QString(" view %1 %2").arg(hhrot).arg(p.vrot);
    }
    if (p.usessao) d += QString(" ssao yes %1 %2").arg(Cfg::SSAO_SEED).arg(p.ssaoval);
    // depth cueing, defocus, and outlines default to "no" in LAMMPS, so emit
    // them only when on
    if (p.usedepthcue)
        d += QString(" depthcue yes %1 %2 %3")
                 .arg(p.depthcuefactor)
                 .arg(p.depthcuecolor, p.depthcuestart);
    if (p.usedefocus) d += QString(" defocus yes %1 %2").arg(p.defocusfactor).arg(p.defocusstart);
    if (p.useoutline) d += QString(" outline yes %1 %2").arg(p.outlinewidth).arg(p.outlinecolor);
    if (p.showbox)
        d += QString(" box yes %1").arg(p.boxdiam);
    else
        d += " box no 0.0";
    // subbox and axes default to "no" in LAMMPS, so emit them only when shown
    if (p.showsubbox) d += QString(" subbox yes %1").arg(p.subboxdiam);

    if (p.showaxes) d += QString(" axes %1 %2 %3").arg(p.axesloc).arg(p.axeslen).arg(p.axesdiam);

    if (p.autobond && p.haspairstyle) {
        // use custom bond diameter value, if present
        QRegularExpression validnum(R"((^\d+\.?\d*|^\d*\.?\d+))");
        auto match = validnum.match(p.bonddiam);
        if (match.hasMatch()) {
            d += blank + "autobond" + blank + QString::number(p.bondcutoff) + blank + p.bonddiam;
        } else {
            d += blank + "autobond" + blank + QString::number(p.bondcutoff) + " 0.5";
        }
    }

    appendRegionArgs(d, p);

    const bool dofixes = appendFixComputeStyles(d, p);

    // center defaults to the static box center "s 0.5 0.5 0.5"; a dynamic center
    // differs from that default even at unmoved fractions, so it is always
    // emitted, while a static center is emitted only when moved
    if (p.dynamiccenter)
        d += QString(" center d %1 %2 %3").arg(p.xcenter).arg(p.ycenter).arg(p.zcenter);
    else if ((p.xcenter != 0.5) || (p.ycenter != 0.5) || (p.zcenter != 0.5))
        d += QString(" center s %1 %2 %3").arg(p.xcenter).arg(p.ycenter).arg(p.zcenter);

    // the camera up direction defaults to "0 0 1" in 3d and "0 1 0" in 2d
    const double defyup = (p.dimension == 2) ? 1.0 : 0.0;
    const double defzup = (p.dimension == 2) ? 0.0 : 1.0;
    if ((p.xup != 0.0) || (p.yup != defyup) || (p.zup != defzup))
        d += QString(" up %1 %2 %3").arg(p.xup).arg(p.yup).arg(p.zup);

    // ---- dump_modify options ----

    // change global color definitions first so they apply everywhere, but emit
    // only those that differ from the LAMMPS built-in defaults: deftypecolors
    // mirrors the LAMMPS color database, so an unmodified table emits nothing
    const int numcolors    = p.color_list.size();
    const int ndefcolors   = deftypecolors.size();
    const bool prunecolors = (numcolors == ndefcolors);

    for (int i = 0; i < numcolors; ++i) {
        if (prunecolors && (p.color_list[i].first == deftypecolors[i].first) &&
            (p.color_list[i].second == deftypecolors[i].second))
            continue;
        m += QString(" color %1 %2 %3 %4")
                 .arg(p.color_list[i].first)
                 .arg(p.color_list[i].second.redF())
                 .arg(p.color_list[i].second.greenF())
                 .arg(p.color_list[i].second.blueF());
    }
    // assign type colors only where the name differs from the default LAMMPS
    // assignment for that type (type i -> deftypecolors[(i - 1) % ndefcolors])
    for (int i = 1; i <= p.ntypes; ++i) {
        const QString curname = p.color_list[(i - 1) % numcolors].first;
        const QString defname = deftypecolors[(i - 1) % ndefcolors].first;
        if (curname == defname) continue;
        m += QString(" acolor %1 %2").arg(i).arg(curname);
    }

    if (p.boxcolor != DEF_BOXCOLOR) m += " boxcolor " + p.boxcolor;

    // background: with the gradient enabled (the GUI default, a deliberate
    // divergence from the solid LAMMPS default) the backcolor/backcolor2 pair is
    // emitted together. With the gradient off the background is solid and only
    // backcolor is emitted, and only when it differs from the LAMMPS default, so
    // backcolor2 is never emitted without backcolor.
    if (p.usegradient) {
        m += " backcolor " + p.backcolor;
        m += " backcolor2 " + p.backcolor2;
    } else if (p.backcolor != DEF_BACKCOLOR) {
        m += " backcolor " + p.backcolor;
    }

    if (p.axestrans != DEF_TRANS) m += QString(" axestrans %1").arg(p.axestrans);
    if (p.boxtrans != DEF_TRANS) m += QString(" boxtrans %1").arg(p.boxtrans);
    if (p.atomtrans != DEF_TRANS) m += QString(" atrans * %1").arg(p.atomtrans);
    if ((p.bond_flag == 1) && (p.bondtrans != DEF_TRANS))
        m += QString(" btrans * %1").arg(p.bondtrans);

    const bool lightsdefault = (p.ambientlight == DEF_AMBIENT) && (p.keylight == DEF_KEYLIGHT) &&
                               (p.filllight == DEF_FILLLIGHT) && (p.backlight == DEF_BACKLIGHT);
    if (!lightsdefault)
        m += QString(" lights %1 %2 %3 %4")
                 .arg(p.ambientlight)
                 .arg(p.keylight)
                 .arg(p.filllight)
                 .arg(p.backlight);

    // the specular highlight width and the SSAO sample count default to being
    // derived from the shiny factor and the SSAO strength; emit only overrides
    if (p.specular != QStringLiteral("auto")) m += " specular " + p.specular;

    // metallic shading defaults to off (metal 0.0) and the finish to "satin";
    // the finish has no effect without the effect enabled, so both are pruned
    if (p.usemetal && (p.metalfactor > 0.0)) {
        m += QString(" metal %1").arg(qBound(0.0, p.metalfactor, 1.0));
        if (p.metalfinish != QStringLiteral("satin")) m += " metalfinish " + p.metalfinish;
    }
    if (p.usessao && (p.ssaosamples > 0))
        m += QString(" ssaosamples %1").arg(qBound(4, p.ssaosamples, 64));

    // a gamma value of 1.0 renders the summed up light contributions unchanged
    if (p.gammaval != DEF_GAMMA) m += QString(" gamma %1").arg(qBound(0.1, p.gammaval, 10.0));

    if (p.useelements) m += blank + p.elements + blank + p.adiams + blank;
    if (p.usesigma) m += blank + p.adiams + blank;
    if (!p.useelements && !p.usesigma && (p.atomSize != 1.0)) m += blank + p.adiams + blank;

    // apply the selected color map only when atoms are colored by a per-atom
    // value; for type/element coloring the map is unused, so omit it
    const bool atomByValue = (atomColorTok != "type") && (atomColorTok != "element");
    if (atomByValue)
        appendColorMapArgs(m, "amap", p.colormap, p.mapmin, p.mapmax, "map", p.revcolormap);
    if (p.bondbyvalue)
        appendColorMapArgs(m, "bmap", p.bondcolormap, p.bondmapmin, p.bondmapmax, "bm",
                           p.revbondcolormap);

    appendFixComputeColors(m, p);

    return {d, m, dofixes};
}

QString toWriteDumpCommand(const DumpImageCommand &c, const QString &group, const QString &file)
{
    QString cmd = "write_dump " + group + " image '" + file + "'" + c.dumpargs;
    if (!c.dofixes) cmd += " noinit";
    cmd += " modify" + c.modifyargs;
    return cmd;
}

// Local Variables:
// c-basic-offset: 4
// End:
