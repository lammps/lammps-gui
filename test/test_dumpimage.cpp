// Unit tests for the pure dump-image command builder (src/dumpimage.cpp).
//
// These tests exercise buildCmd() without a GUI or a live LAMMPS
// instance: a DumpImageParams struct is populated with representative values
// and the resulting "write_dump ... image ..." command string is checked for
// the expected fragments.

#include "dumpimage.h"

#include <QColor>
#include <QString>

#include "gtest/gtest.h"

namespace {

// Build a DumpImageParams with sensible, neutral defaults that individual
// tests then tweak. With these defaults no fix/compute/region is active and
// do_vdw is false (vdwfactor < VDW_CUT); the light levels below differ from
// the LAMMPS defaults, so the lights keyword is emitted.
DumpImageParams makeParams()
{
    DumpImageParams p;

    p.group    = "all";
    p.dumpfile = "/tmp/foo.ppm";

    p.atomcustom  = false;
    p.useelements = false;
    p.usediameter = false;
    p.usesigma    = false;
    p.showatoms   = true;
    p.atomcolor   = "type";
    p.atomdiam    = "type";
    p.vdwfactor   = 0.5; // do_vdw == false
    p.atomSize    = 1.0;
    p.elements    = "element ";
    p.adiams      = "";

    p.showbodies     = false;
    p.body_flag      = 0;
    p.bodycolor      = "red";
    p.bodydiam       = 1.0;
    p.bodyflag       = 1;
    p.showlines      = false;
    p.line_flag      = 0;
    p.linecolor      = "red";
    p.linediam       = 1.0;
    p.showtris       = false;
    p.tri_flag       = 0;
    p.tricolor       = "red";
    p.triflag        = 1;
    p.tridiam        = 1.0;
    p.showellipsoids = false;
    p.ellipsoid_flag = 0;
    p.ellipsoidcolor = "red";
    p.ellipsoidflag  = 1;
    p.ellipsoidlevel = 2;
    p.ellipsoiddiam  = 1.0;

    p.nbondtypes   = 0;
    p.bond_flag    = 0;
    p.showbonds    = true;
    p.bondcolor    = "gray";
    p.bondbyvalue  = false;
    p.bonddiam     = "0.2";
    p.autobond     = false;
    p.haspairstyle = true;
    p.bondcutoff   = 2.0;

    p.xsize       = 800;
    p.ysize       = 600;
    p.zoom        = 1.5;
    p.shinyfactor = 0.6;
    p.antialias   = true;
    p.dimension   = 3;
    p.hrot        = 30;
    p.vrot        = 20;
    p.usessao     = false;
    p.ssaoval     = 0.6;
    p.ssaosamples = 0;

    p.usedepthcue    = false;
    p.depthcuefactor = 0.5;
    p.depthcuecolor  = "auto";
    p.depthcuestart  = "auto";
    p.usedefocus     = false;
    p.defocusfactor  = 0.5;
    p.defocusstart   = "auto";
    p.useoutline     = false;
    p.outlinewidth   = 2;
    p.outlinecolor   = "black";
    p.specular       = "auto";
    p.usemetal       = false;
    p.metalfactor    = 0.5;
    p.metalfinish    = "satin";

    p.showbox    = true;
    p.boxdiam    = 0.05;
    p.showsubbox = false;
    p.subboxdiam = 0.0;
    p.showaxes   = false;
    p.axesloc    = "0.0 0.0";
    p.axeslen    = 0.2;
    p.axesdiam   = 0.1;

    p.dynamiccenter = false;
    p.xcenter       = 0.5;
    p.ycenter       = 0.5;
    p.zcenter       = 0.5;

    p.xup = 0.0;
    p.yup = 0.0;
    p.zup = 1.0;

    p.ntypes       = 2;
    p.color_list   = {{"red", QColor(255, 0, 0)}, {"blue", QColor(0, 0, 255)}};
    p.boxcolor     = "white";
    p.backcolor    = "black";
    p.backcolor2   = "gray";
    p.usegradient  = true;
    p.axestrans    = 0.0;
    p.boxtrans     = 0.0;
    p.atomtrans    = 1.0;
    p.bondtrans    = 1.0;
    p.ambientlight = 0.2;
    p.keylight     = 0.7;
    p.filllight    = 0.3;
    p.backlight    = 0.2;
    p.gammaval     = 1.0;
    p.version      = 20260704;

    p.colormap        = "BWR";
    p.mapmin          = "auto";
    p.mapmax          = "auto";
    p.revcolormap     = false;
    p.bondcolormap    = "BWR";
    p.bondmapmin      = "auto";
    p.bondmapmax      = "auto";
    p.revbondcolormap = false;

    return p;
}

// reconstruct the one-shot write_dump command so the assertions below (which look
// for "write_dump ... image ... modify ..." fragments) keep working against the
// two-string builder
QString buildCmd(const DumpImageParams &p)
{
    return toWriteDumpCommand(buildDumpImageCommand(p), p.group, p.dumpfile);
}

TEST(DumpImageCommand, BasicStructure)
{
    const QString cmd = buildCmd(makeParams());

    EXPECT_TRUE(cmd.startsWith("write_dump all image '/tmp/foo.ppm'")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" size 800 600"));
    EXPECT_TRUE(cmd.contains(" zoom 1.5"));
    EXPECT_TRUE(cmd.contains(" shiny 0.6 "));
    EXPECT_TRUE(cmd.contains(" fsaa yes"));
    EXPECT_TRUE(cmd.contains(" view 30 20"));
    EXPECT_TRUE(cmd.contains(" box yes 0.05"));
    EXPECT_FALSE(cmd.contains(" subbox ")); // not shown -> pruned
    EXPECT_FALSE(cmd.contains(" axes "));   // not shown -> pruned
    EXPECT_FALSE(cmd.contains(" center ")); // default box center -> pruned
    EXPECT_FALSE(cmd.contains(" up "));     // default up direction -> pruned
    EXPECT_TRUE(cmd.contains(" modify"));
    EXPECT_TRUE(cmd.contains(" noinit")); // no active fix/compute
    EXPECT_TRUE(cmd.contains(" boxcolor white"));
    EXPECT_TRUE(cmd.contains(" backcolor black"));
    EXPECT_TRUE(cmd.contains(" backcolor2 gray"));
    EXPECT_TRUE(cmd.contains(" lights ")); // light levels differ from the defaults
}

TEST(DumpImageCommand, ColorTablePrunedToDeltas)
{
    auto p       = makeParams();
    p.color_list = deftypecolors; // identical to the LAMMPS built-in defaults
    p.ntypes     = 4;

    // an unmodified default table emits no color or acolor lines
    QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" color ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" acolor "));

    // changing one slot's RGB emits only that color line; the name is unchanged,
    // so the default type assignment still applies and no acolor is needed
    p.color_list[2].second = QColor(10, 20, 30); // the "blue" slot
    cmd                    = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color blue ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" color red")); // unchanged default -> pruned
    EXPECT_FALSE(cmd.contains(" acolor "));
}

TEST(DumpImageCommand, DefaultColormapIsBWR)
{
    auto p            = makeParams();
    p.atomcolor       = "vx"; // color atoms by a per-atom value so the map is emitted
    const QString cmd = buildCmd(p);

    // "auto" min/max are translated to "min"/"max"
    EXPECT_TRUE(cmd.contains(" color map1 0.000 0.227 0.427"));
    EXPECT_TRUE(cmd.contains(" color map2 0.459 0.055 0.075"));
    EXPECT_TRUE(cmd.contains(" amap min max cf 0.0 3 min map1 0.5 white max map2"))
        << cmd.toStdString();
}

TEST(DumpImageCommand, ExplicitColormapRange)
{
    auto p            = makeParams();
    p.atomcolor       = "vx";
    p.mapmin          = "0.0";
    p.mapmax          = "1.0";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" amap 0.0 1.0 cf 0.0 ")) << cmd.toStdString();
}

TEST(DumpImageCommand, NamedColormap)
{
    auto p            = makeParams();
    p.atomcolor       = "vx";
    p.colormap        = "Grayscale";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" amap min max cf 0.0 2 min black max white")) << cmd.toStdString();
}

TEST(DumpImageCommand, ReversedContinuousColormap)
{
    // reversing a continuous map flips the stop order and mirrors positions, so
    // Grayscale (black -> white) renders as white -> black
    auto p            = makeParams();
    p.atomcolor       = "vx";
    p.colormap        = "Grayscale";
    p.revcolormap     = true;
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" amap min max cf 0.0 2 min white max black")) << cmd.toStdString();
}

TEST(DumpImageCommand, ReversedRwbMatchesBwrDirection)
{
    // "RWB" is the unlisted reversed alias of the offered "BWR" map:
    // reversing RWB must put the blue stop (0.000 0.227 0.427) at the minimum
    auto p            = makeParams();
    p.atomcolor       = "vx";
    p.colormap        = "RWB";
    p.revcolormap     = true;
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map1 0.000 0.227 0.427")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" amap min max cf 0.0 5 min map1"));
}

TEST(DumpImageCommand, ReversedDiscreteBondColormap)
{
    // reversing a discrete (sequence) bond map flips the color order; Basic ends
    // with orange, so the reversed sequence must start with orange
    auto p            = makeParams();
    p.nbondtypes      = 1;
    p.showbonds       = true;
    p.bondbyvalue     = true;
    p.bondcolor       = "c_b";
    p.bondcolormap    = "Basic";
    p.revbondcolormap = true;
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" bmap min max sa 1.0 10 orange white purple")) << cmd.toStdString();
}

TEST(DumpImageCommand, ElementColoring)
{
    auto p        = makeParams();
    p.useelements = true;
    p.atomcustom  = false;
    p.elements    = "element C H ";
    p.adiams      = "adiam 1 1.7 adiam 2 1.2 ";

    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" element")); // atom-color token
    EXPECT_TRUE(cmd.contains("element C H"));
    EXPECT_TRUE(cmd.contains("adiam 1 1.7"));
}

TEST(DumpImageCommand, AtomSizeByVariable)
{
    auto p       = makeParams();
    p.atomcustom = true;
    p.atomdiam   = "v_scale";

    // the variable reference becomes the diameter attribute of the dump image
    // command instead of "type"
    QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" type v_scale ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" adiam "));

    // the variable also takes precedence over per-atom diameter data with
    // active VDW mode, where "diameter" would otherwise be selected
    p.usediameter = true;
    p.vdwfactor   = 2.0; // do_vdw true
    cmd           = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" type v_scale ")) << cmd.toStdString();
}

TEST(DumpImageCommand, NoAtoms)
{
    auto p            = makeParams();
    p.showatoms       = false;
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" atom no"));
}

TEST(DumpImageCommand, BondNoneWhenVdwActive)
{
    auto p            = makeParams();
    p.nbondtypes      = 1;
    p.vdwfactor       = 2.0; // do_vdw true -> bonds suppressed
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" bond none none "));
}

TEST(DumpImageCommand, BondDrawnWhenRequested)
{
    auto p            = makeParams();
    p.nbondtypes      = 1;
    p.vdwfactor       = 0.5; // do_vdw false
    p.showbonds       = true;
    p.bondcolor       = "gray";
    p.bonddiam        = "0.2";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" bond gray 0.2 ")) << cmd.toStdString();
}

TEST(DumpImageCommand, AutobondAppended)
{
    auto p            = makeParams();
    p.autobond        = true;
    p.haspairstyle    = true;
    p.bondcutoff      = 2.0;
    p.bonddiam        = "0.3";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" autobond 2 0.3")) << cmd.toStdString();
}

TEST(DumpImageCommand, AutobondSkippedWithoutPairStyle)
{
    auto p            = makeParams();
    p.autobond        = true;
    p.haspairstyle    = false;
    const QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" autobond "));
}

TEST(DumpImageCommand, ActiveFixSuppressesNoinit)
{
    auto p = makeParams();
    ImageInfo fix(true, "ave/time", CONSTANT, "blue", 0.5, 1.0, 2.0);
    p.fixes["myfix"] = &fix;

    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" fix myfix const 1 2")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" fcolor myfix blue"));
    EXPECT_TRUE(cmd.contains(" ftrans myfix 0.5"));
    EXPECT_FALSE(cmd.contains(" noinit"));
}

TEST(DumpImageCommand, DisabledFixIgnored)
{
    auto p = makeParams();
    ImageInfo fix(false, "ave/time", CONSTANT, "blue", 0.5, 1.0, 2.0);
    p.fixes["myfix"] = &fix;

    const QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains("myfix"));
    EXPECT_TRUE(cmd.contains(" noinit")); // no active fix/compute
}

TEST(DumpImageCommand, RegionPoints)
{
    auto p = makeParams();
    RegionInfo reg(true, POINTS, "red", 0.2, 0.5, 100);
    p.regions["myreg"] = &reg;

    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" region myreg red points 100 0.2")) << cmd.toStdString();
}

TEST(DumpImageCommand, DepthCueOutlineSpecular)
{
    // all the new rendering options default to off/auto and emit nothing
    auto p      = makeParams();
    QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" depthcue ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" defocus "));
    EXPECT_FALSE(cmd.contains(" outline "));
    EXPECT_FALSE(cmd.contains(" specular "));
    EXPECT_FALSE(cmd.contains(" ssaosamples "));
    EXPECT_FALSE(cmd.contains(" gamma "));

    p.usedepthcue    = true;
    p.depthcuefactor = 0.7;
    cmd              = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" depthcue yes 0.7 auto auto")) << cmd.toStdString();

    p.depthcuecolor = "white";
    p.depthcuestart = "0.25";
    cmd             = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" depthcue yes 0.7 white 0.25")) << cmd.toStdString();

    // defocus takes no color argument, only the strength and the start position
    p.usedefocus    = true;
    p.defocusfactor = 0.4;
    cmd             = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" defocus yes 0.4 auto")) << cmd.toStdString();

    p.defocusstart = "0.5";
    cmd            = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" defocus yes 0.4 0.5")) << cmd.toStdString();

    p.useoutline   = true;
    p.outlinewidth = 3;
    p.outlinecolor = "gray";
    cmd            = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" outline yes 3 gray")) << cmd.toStdString();

    p.specular = "tight";
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" specular tight")) << cmd.toStdString();
}

TEST(DumpImageCommand, MetalEffect)
{
    // metallic shading defaults to off and emits nothing
    auto p      = makeParams();
    QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" metal ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" metalfinish "));

    // enabling the effect emits the intensity; the default "satin" finish is pruned
    p.usemetal = true;
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" metal 0.5")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" metalfinish "));

    p.metalfactor = 0.8;
    p.metalfinish = "mirror";
    cmd           = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" metal 0.8 metalfinish mirror")) << cmd.toStdString();

    // an intensity of 0.0 is the LAMMPS default and disables the effect,
    // so the finish must be pruned along with it
    p.metalfactor = 0.0;
    cmd           = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" metal ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" metalfinish "));

    // out-of-range values (LAMMPS accepts 0.0 - 1.0 only) are clamped
    p.metalfactor = 1.5;
    cmd           = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" metal 1 metalfinish mirror")) << cmd.toStdString();
}

TEST(DumpImageCommand, GammaAdjustment)
{
    // the default gamma of 1.0 changes nothing and is pruned
    auto p      = makeParams();
    QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" gamma ")) << cmd.toStdString();

    p.gammaval = 1.4;
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" gamma 1.4")) << cmd.toStdString();

    // out-of-range values (LAMMPS accepts 0.1 - 10.0 only) are clamped
    p.gammaval = 20.0;
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" gamma 10")) << cmd.toStdString();

    p.gammaval = 0.01;
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" gamma 0.1")) << cmd.toStdString();
}

TEST(DumpImageCommand, SsaoSamplesOnlyWithSsao)
{
    auto p        = makeParams();
    p.ssaosamples = 16;

    // without ssao enabled the sample count has no effect and is pruned
    QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" ssaosamples ")) << cmd.toStdString();

    p.usessao = true;
    cmd       = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" ssao yes ")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" ssaosamples 16"));

    // values outside the LAMMPS 4-64 range are clamped
    p.ssaosamples = 100;
    EXPECT_TRUE(buildCmd(p).contains(" ssaosamples 64"));
    p.ssaosamples = 2;
    EXPECT_TRUE(buildCmd(p).contains(" ssaosamples 4"));
}

TEST(DumpImageCommand, ColorMapOmittedForTypeColoring)
{
    auto p = makeParams(); // atomcolor == "type"
    EXPECT_FALSE(buildCmd(p).contains(" amap ")) << "type coloring needs no map";
}

TEST(DumpImageCommand, AllDefaultsPruned)
{
    auto p         = makeParams();
    p.color_list   = deftypecolors; // == LAMMPS built-in defaults
    p.ntypes       = 4;
    p.atomcolor    = "type";
    p.boxcolor     = "gold";
    p.backcolor    = "black";
    p.backcolor2   = "white";
    p.usegradient  = false; // solid background; backcolor == LAMMPS default -> pruned
    p.axestrans    = 1.0;
    p.boxtrans     = 1.0;
    p.atomtrans    = 1.0;
    p.bondtrans    = 1.0;
    p.ambientlight = 0.0;
    p.keylight     = 0.9;
    p.filllight    = 0.45;
    p.backlight    = 0.9;
    p.gammaval     = 1.0;
    p.version      = 20260704;

    const QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" amap ")) << cmd.toStdString();
    EXPECT_FALSE(cmd.contains(" color "));
    EXPECT_FALSE(cmd.contains(" acolor "));
    EXPECT_FALSE(cmd.contains(" boxcolor"));
    EXPECT_FALSE(cmd.contains(" backcolor"));
    EXPECT_FALSE(cmd.contains(" axestrans"));
    EXPECT_FALSE(cmd.contains(" boxtrans"));
    EXPECT_FALSE(cmd.contains(" atrans"));
    EXPECT_FALSE(cmd.contains(" btrans"));
    EXPECT_FALSE(cmd.contains(" lights"));
    EXPECT_FALSE(cmd.contains(" gamma"));
    EXPECT_FALSE(cmd.contains(" defocus"));
    EXPECT_FALSE(cmd.contains(" subbox "));
    EXPECT_FALSE(cmd.contains(" axes "));
    EXPECT_FALSE(cmd.contains(" center "));
    EXPECT_FALSE(cmd.contains(" up "));
    EXPECT_TRUE(cmd.contains(" modify"));
}

TEST(DumpImageCommand, TransparencyEmittedWhenNotOpaque)
{
    auto p            = makeParams(); // axestrans = boxtrans = 0.0, atomtrans = 1.0
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" axestrans 0"));
    EXPECT_TRUE(cmd.contains(" boxtrans 0"));
    EXPECT_FALSE(cmd.contains(" atrans ")); // opaque -> pruned
}

TEST(DumpImageCommand, BondTransparencyIndependentOfAtoms)
{
    auto p            = makeParams();
    p.bond_flag       = 1;
    p.atomtrans       = 1.0; // atoms opaque -> atrans pruned
    p.bondtrans       = 0.5; // bonds translucent -> btrans emitted independently
    const QString cmd = buildCmd(p);
    EXPECT_FALSE(cmd.contains(" atrans "));
    EXPECT_TRUE(cmd.contains(" btrans * 0.5")) << cmd.toStdString();
}

TEST(DumpImageCommand, SolidBackgroundOmitsBackcolor2)
{
    auto p            = makeParams();
    p.usegradient     = false;
    p.backcolor       = "navy"; // a delta from the LAMMPS default
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" backcolor navy"));
    EXPECT_FALSE(cmd.contains(" backcolor2")); // never backcolor2 without the gradient
}

TEST(DumpImageCommand, SubboxAxesCenterEmittedWhenSet)
{
    auto p            = makeParams();
    p.showsubbox      = true;
    p.subboxdiam      = 0.01;
    p.showaxes        = true;
    p.xcenter         = 0.3; // moved off the default box center
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" subbox yes 0.01")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" axes "));
    EXPECT_TRUE(cmd.contains(" center s 0.3 0.5 0.5"));
}

TEST(DumpImageCommand, DynamicCenterAlwaysEmitted)
{
    auto p          = makeParams();
    p.dynamiccenter = true;

    // a dynamic center differs from the LAMMPS default "s 0.5 0.5 0.5"
    // even at the default fractions, so it must never be pruned
    EXPECT_TRUE(buildCmd(p).contains(" center d 0.5 0.5 0.5")) << buildCmd(p).toStdString();

    p.xcenter = 0.3;
    EXPECT_TRUE(buildCmd(p).contains(" center d 0.3 0.5 0.5"));
}

TEST(DumpImageCommand, UpDirectionEmittedWhenNotDefault)
{
    auto p = makeParams(); // 3d with the default up direction "0 0 1"
    EXPECT_FALSE(buildCmd(p).contains(" up "));

    p.yup = 1.0;
    p.zup = 0.0;
    EXPECT_TRUE(buildCmd(p).contains(" up 0 1 0"));

    // in 2d the default flips to "0 1 0", so the same vector is now pruned ...
    p.dimension = 2;
    EXPECT_FALSE(buildCmd(p).contains(" up "));

    // ... and the 3d default becomes a delta worth emitting
    p.yup = 0.0;
    p.zup = 1.0;
    EXPECT_TRUE(buildCmd(p).contains(" up 0 0 1"));
}

TEST(DumpImageCommand, BondColorByValueEmitsComputeAndBmap)
{
    auto p            = makeParams();
    p.nbondtypes      = 1;
    p.showbonds       = true;
    p.bondbyvalue     = true;
    p.bondcolor       = "c_imgviewer_bondval";
    p.bonddiam        = "0.3";
    p.bondcolormap    = "Plasma";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" bond c_imgviewer_bondval 0.3 ")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" bmap min max cf 0.0 "));
    EXPECT_TRUE(cmd.contains(" color bm1 ")); // bond map stops use the "bm" prefix
    EXPECT_FALSE(cmd.contains(" amap "));     // atoms colored by type -> no atom map
}

TEST(DumpImageCommand, AtomAndBondMapsUseDistinctColorNames)
{
    auto p            = makeParams();
    p.atomcolor       = "vx"; // atoms colored by value -> amap
    p.colormap        = "Viridis";
    p.nbondtypes      = 1;
    p.bondbyvalue     = true;
    p.bondcolor       = "c_b";
    p.bondcolormap    = "Plasma";
    const QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" amap ")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" bmap "));
    EXPECT_TRUE(cmd.contains(" color map1 ")); // atom (Viridis) custom stops
    EXPECT_TRUE(cmd.contains(" color bm1 "));  // bond (Plasma) custom stops, distinct names
}

TEST(DumpImageCommand, PerceptualColorMapsUseCanonicalStops)
{
    auto p      = makeParams();
    p.atomcolor = "vx"; // color atoms by value so the map is emitted

    p.colormap  = "Viridis";
    QString cmd = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map1 0.267 0.005 0.329")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" color map4 0.993 0.906 0.144"));

    p.colormap = "Inferno";
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map1 0.001 0.000 0.014")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" color map5 0.988 0.998 0.645"));

    p.colormap = "Magma";
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map3 0.716 0.215 0.475")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" color map5 0.987 0.991 0.750"));

    p.colormap = "Cividis";
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map1 0.000 0.135 0.305")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" color map5 0.996 0.909 0.218"));

    p.colormap = "Turbo";
    cmd        = buildCmd(p);
    EXPECT_TRUE(cmd.contains(" color map1 0.190 0.072 0.232")) << cmd.toStdString();
    EXPECT_TRUE(cmd.contains(" color map6 0.480 0.016 0.011"));
}

} // namespace
