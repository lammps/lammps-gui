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

#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSize>
#include <QString>
#include <QStringList>
#include <map>

class QAction;
class QMenuBar;
class QButtonGroup;
class QEvent;
class QGridLayout;
class QLabel;
class QObject;
class QRadioButton;
class QScrollArea;
class QShowEvent;
class LammpsWrapper;
class LammpsGui;
class ImageInfo;
class RegionInfo;
struct DumpImageParams;

/**
 * @brief Dialog for viewing and manipulating LAMMPS snapshot images
 *
 * This class provides an image viewer dialog for displaying LAMMPS snapshots
 * created by the `dump image` command. It allows interactive manipulation of
 * visualization parameters such as zoom, rotation, atom size, coloring, and
 * rendering options. Changes can be applied to regenerate the image using the
 * LAMMPS library interface.
 */
class ImageViewer : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param fileName Path to the image file to display
     * @param lammps Pointer to LammpsWrapper for regenerating images
     * @param lammpsgui Pointer to LammpsGui for sending signals
     * @param parent Parent widget
     */
    explicit ImageViewer(const QString &fileName, LammpsWrapper *lammps, LammpsGui *lammpsgui,
                         QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ImageViewer() override;

    ImageViewer()                               = delete;
    ImageViewer(const ImageViewer &)            = delete;
    ImageViewer(ImageViewer &&)                 = delete;
    ImageViewer &operator=(const ImageViewer &) = delete;
    ImageViewer &operator=(ImageViewer &&)      = delete;

private slots:
    void saveAs();            ///< Save image to file
    void copy();              ///< Copy image to clipboard
    void quit();              ///< Close dialog
    void getHelp();           ///< Open online help
    void setAtomSize();       ///< Set explicit atom display size
    void setBondSize();       ///< Set explicit bond display size
    void editSize();          ///< Edit image dimensions
    void resetView();         ///< Reset view to defaults
    void resetWindowSize();   ///< Resize window to fit the configured image size
    void toggleSsao();        ///< Toggle screen-space ambient occlusion
    void toggleAnti();        ///< Toggle anti-aliasing
    void toggleDepthcue();    ///< Toggle depth cueing
    void toggleDefocus();     ///< Toggle defocusing of distant objects
    void toggleShiny();       ///< Toggle shiny/specular rendering
    void toggleVdw();         ///< Toggle Van der Waals radii
    void toggleBond();        ///< Toggle bond display
    void vdwbondSync();       ///< Sync settings of VDW style and autobonds in Atoms dialog
    void acolorSync();        ///< Sync settings of atom color based on shaped object color
    void setBondcut();        ///< Set bond cutoff distance
    void toggleBox();         ///< Toggle simulation box display
    void toggleAxes();        ///< Toggle coordinate axes display
    void doZoomIn();          ///< Zoom in view
    void doZoomOut();         ///< Zoom out view
    void doRotLeft();         ///< Rotate view left
    void doRotRight();        ///< Rotate view right
    void doRotUp();           ///< Rotate view up
    void doRotDown();         ///< Rotate view down
    void doRecenter();        ///< Recenter view
    void cmdToClipboard();    ///< Copy dump command to clipboard
    void globalSettings();    ///< Configure global dump image settings
    void atomSettings();      ///< Configure atom and bond settings
    void fixSettings();       ///< Configure fix graphics display
    void regionSettings();    ///< Configure region display
    void colorSettings();     ///< Customize colors
    void resetColors();       ///< Restore original color list
    void loadColors();        ///< Load colors and lighting from JSON file
    void saveColors();        ///< Save colors and lighting to JSON file
    void changeGroup(int);    ///< Change atom group selection
    void changeMolecule(int); ///< Change molecule selection

public:
    /**
     * @brief Generate image using current settings
     *
     * Constructs and executes a LAMMPS dump image command with current
     * visualization parameters and updates the displayed image.
     */
    void createImage();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override; ///< Intercept Alt-keystrokes
    void showEvent(QShowEvent *event) override; ///< Redo the initial window fit once shown

private:
    void createActions();     ///< Setup menu actions
    void updateActions();     ///< Update action states
    void adjustWindowSize();  ///< Auto-resize window to fit image
    void readImageSettings(); ///< Read snapshot settings from QSettings and reset members
    void updateFixes();       ///< Update fix graphics information
    void updateRegions();     ///< Update region information
    void updatePeratom();     ///< Update per-atom information
    bool hasAutobonds();      ///< Check if autobonds are enabled

    /** @brief True when bond color-by-value applies: a bond/local attribute is
     *  selected, the atom style has real bonds, and AutoBonds is off (compute
     *  bond/local only works for real bonds) */
    bool bondByValueActive();

    /** @brief (Re)populate the bond Color selector; the compute bond/local
     *  "color by value" choices are added only when @p allowByValue */
    void rebuildBondColorChoices(QComboBox *bncolor, bool allowByValue);

    /// @name dump-image command preparation used by createImage()
    /// @{
    /// Gather widget state and LAMMPS-derived data into a DumpImageParams snapshot
    DumpImageParams gatherDumpImageParams(const QString &dumpfilename);
    /// Show/hide the atom-size widgets to match the resolved element/diameter state
    void syncAtomSizeWidgets();
    /// @}

    /// @name dialog row builders/readers used by the *Settings() slots
    /// @{
    /// Build one compute/fix table row per map entry (shared by fixSettings)
    void buildFixComputeRows(QGridLayout *layout, int &idx,
                             const std::map<std::string, ImageInfo *> &items,
                             const QMap<QString, QString> &helpmap);
    /// Read back a compute/fix table section starting at grid row @p offset
    void readFixComputeRows(QGridLayout *layout, int offset,
                            std::map<std::string, ImageInfo *> &items);
    /// Read back the region table rows into the regions map
    void readRegionRows(QGridLayout *layout);
    /// Read back the atom-type color table rows into color_list
    void readColorRows(QGridLayout *layout, int colorstart, int numtypes);
    /// @}

    /**
     * @brief Create an "atom/type/index" color selection combo box for a shape style
     * @param current Color property to preselect
     * @param name Object name used for later lookup via findChild()
     * @return Combo box connected to the acolorSync() slot
     */
    QComboBox *makeColorCombo(const QString &current, const QString &name);

    /**
     * @brief Create a shape-style radio button and add it to a button group and grid layout
     * @param group Button group the radio button joins
     * @param label Button label text
     * @param checked Whether the button starts checked
     * @param layout Grid layout to place the button in
     * @param row Grid row index
     * @param col Grid column index (post-incremented)
     * @return The created radio button
     */
    QRadioButton *addShapeButton(QButtonGroup *group, const QString &label, bool checked,
                                 QGridLayout *layout, int row, int &col);

private:
    QImage image;            ///< Currently displayed image
    QMenuBar *menuBar;       ///< Menu bar
    QLabel *imageLabel;      ///< Label displaying the image
    QScrollArea *scrollArea; ///< Scrollable area for image
    QSize lastFitSize;       ///< Scroll area size applied by the last auto-resize
    double atomSize;         ///< Explicit atom display size (as radius)
    double bondSize;         ///< Explicit bond display size (as diameter)

    QAction *saveAsAct; ///< Save As action
    QAction *copyAct;   ///< Copy action
    QAction *cmdAct;    ///< Copy command action

    QMap<QString, QString> fix_map;              ///< Fix style to help page mapping
    QMap<QString, QString> compute_map;          ///< Compute style to help page mapping
    QStringList image_computes;                  ///< list of computes supporting dump image
    QStringList image_fixes;                     ///< list of fixes supporting dump image
    QStringList atom_properties;                 ///< list of per-atom properties for coloring
    LammpsWrapper *lammps;                       ///< LAMMPS interface for image generation
    LammpsGui *lammpsgui;                        ///< Main widget pointer for receiving signals
    QString group;                               ///< Current atom group
    QString molecule;                            ///< Current molecule selection
    QString filename;                            ///< Image filename
    QString last_dumpargs;                       ///< Render args of the last image command
    QString last_modifyargs;                     ///< dump_modify args of the last image command
    QString renderdumpid = "WRITE_DUMP";         ///< Id of our render dump; renamed to a missing
                                                 ///< "fix graphics/labels" colorscale dump and
                                                 ///< cached so the detection runs only once
    int xsize, ysize;                            ///< Image dimensions in pixels
    int hrot, vrot;                              ///< Horizontal and vertical rotation angles
    int bodyflag;                                ///< bflag1 setting (triangle, cylinder or both)
    int ellipsoidflag;                           ///< eflag1 setting (triangle or cylinder)
    int ellipsoidlevel;                          ///< eflag2 setting (refinement level)
    int triflag;                                 ///< tflag1 setting (triangle, cylinder or both)
    double bodydiam;                             ///< bflag2 setting (diameter)
    double ellipsoiddiam;                        ///< eflag3 setting (diameter)
    double linediam;                             ///< linewidth setting (diameter)
    double tridiam;                              ///< tflag2 setting (diameter)
    double zoom;                                 ///< Zoom level
    double vdwfactor;                            ///< Van der Waals radius scaling factor
    double shinyfactor;                          ///< Shininess/specular factor
    double bondcutoff;                           ///< Bond cutoff distance
    double boxdiam;                              ///< Simulation box diameter
    double subboxdiam;                           ///< Simulation subbox diameter
    double boxtrans;                             ///< Transparency for box and subbox
    double axeslen;                              ///< Axes length
    double axesdiam;                             ///< Axes diameter
    double axestrans;                            ///< Axes transparency
    double ssaoval;                              ///< SSAO strength
    int ssaosamples;                             ///< SSAO sampling directions (0 = auto)
    double depthcuefactor;                       ///< Depth cueing strength
    QString depthcuecolor;                       ///< Depth cueing fog color or "auto"
    QString depthcuestart;                       ///< Depth cueing start fraction or "auto"
    double defocusfactor;                        ///< Defocus blur strength
    QString defocusstart;                        ///< Defocus start fraction or "auto"
    int outlinewidth;                            ///< Outline width in pixels
    QString outlinecolor;                        ///< Outline color
    QString specular;                            ///< Specular preset or "auto"
    double metalfactor;                          ///< Metallic shading strength
    QString metalfinish;                         ///< Metal surface finish preset
    double atomtrans;                            ///< Atom transparency
    double bondtrans;                            ///< Bond transparency
    double ambientlight;                         ///< ambient light setting
    double keylight;                             ///< key light setting
    double filllight;                            ///< fill light setting
    double backlight;                            ///< back light setting
    double gammaval;                             ///< gamma adjustment (1.0 = unchanged)
    QString axesloc;                             ///< Axes location
    QString boxcolor;                            ///< Color for box and subbox
    QString backcolor;                           ///< (lower) background color
    QString backcolor2;                          ///< (upper) background color
    QString atomcolor;                           ///< Custom atom color property
    QString atomdiam;                            ///< Custom atom diameter property
    QString colormap;                            ///< Name of selected color map
    QString mapmin;                              ///< Choice of minimum value for colormap
    QString mapmax;                              ///< Choice of maximum value for colormap
    bool revcolormap;                            ///< Reverse (mirror) the atom color map
    QString bondcolormap;                        ///< Name of selected bond color map
    QString bondmapmin;                          ///< Choice of minimum value for bond colormap
    QString bondmapmax;                          ///< Choice of maximum value for bond colormap
    bool revbondcolormap;                        ///< Reverse (mirror) the bond color map
    QString bondcolor;                           ///< Custom bond color property
    QString bonddiam;                            ///< Custom bond diameter property
    QString bodycolor;                           ///< Custom body color property
    QString ellipsoidcolor;                      ///< Custom ellipsoid color property
    QString linecolor;                           ///< Custom line color property
    QString tricolor;                            ///< Custom triangle color property
    double xcenter, ycenter, zcenter;            ///< View center coordinates
    bool dynamiccenter;                          ///< Use dynamic instead of static view center
    double xup, yup, zup;                        ///< Camera up direction vector
    bool atomcustom;                             ///< Use custom atom color settings
    bool usegradient;                            ///< Vertical background gradient
    bool showbox;                                ///< Show simulation box flag
    bool showsubbox;                             ///< Show subdomain boxes flag
    bool showaxes;                               ///< Show coordinate axes flag
    bool antialias;                              ///< Anti-aliasing enabled flag
    bool usessao;                                ///< SSAO enabled flag
    bool usedepthcue;                            ///< Depth cueing enabled flag
    bool usedefocus;                             ///< Defocus enabled flag
    bool useoutline;                             ///< Outline drawing enabled flag
    bool usemetal;                               ///< Metallic shading enabled flag
    bool showatoms;                              ///< Show atoms
    bool showbonds;                              ///< Show bonds if atom style supports it
    bool autobond;                               ///< Dynamic bonds from cutoff flag
    bool showbodies;                             ///< Show bodies if atom style supports it
    bool showellipsoids;                         ///< Show ellipsoids if atom style supports it
    bool showlines;                              ///< Show lines if atom style supports it
    bool showtris;                               ///< Show tris if atom style supports it
    bool useelements;                            ///< Use element properties flag
    bool usediameter;                            ///< Use diameter attribute flag
    bool usesigma;                               ///< Use sigma attribute flag
    std::map<std::string, ImageInfo *> computes; ///< Compute graphics settings
    std::map<std::string, ImageInfo *> fixes;    ///< Fix graphics settings
    std::map<std::string, RegionInfo *> regions; ///< Region settings
    QList<QPair<QString, QColor>> color_list;    ///< Per-type atom colors (not stored persistently)
    bool shutdown;                               ///< flag if class has entered the destructor
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
