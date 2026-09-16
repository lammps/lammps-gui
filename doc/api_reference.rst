*************
API Reference
*************

The following sections provide detailed API documentation for the main
classes in LAMMPS-GUI.  Documentation is generated from `Doxygen comments
<https://doxygen.nl>`_ in the source code.


Main Window
===========

LammpsGui Class
---------------

.. doxygenclass:: LammpsGui
   :members:
   :protected-members:

-----

TutorialWizard Class
--------------------

.. doxygenclass:: TutorialWizard
   :members:
   :protected-members:

-----

Tutorial Collections
--------------------

The ``TutorialCollection`` struct (``src/tutorials.h``) is the single source of
truth for the tutorial collections offered in the *Tutorials* menu.  Each entry
describes one independently hosted collection (its files repository, web pages,
per-tutorial titles and blurbs, and how much of it is released).  The menu and
the ``TutorialWizard`` consume the table through the
``tutorialCollections()`` and ``tutorialCollection()`` accessors.  See
:ref:`add_tutorial` for how to add or update a tutorial or a whole collection.

.. doxygenstruct:: TutorialCollection
   :members:

-----

.. doxygenfunction:: tutorialCollections

.. doxygenfunction:: tutorialCollection

-----

Editor Components
=================

CodeEditor Class
----------------

.. doxygenclass:: CodeEditor
   :members:
   :protected-members:

-----

LineNumberArea Class
--------------------

.. doxygenclass:: LineNumberArea
   :members:
   :protected-members:

-----

Highlighter Class
-----------------

.. doxygenclass:: Highlighter
   :members:
   :protected-members:

-----

LammpsSyntax Class
------------------

.. doxygenclass:: LammpsSyntax
   :members:

-----

InputScanner Class
------------------

.. doxygenclass:: InputScanner
   :members:

-----

SyntaxChecker Class
-------------------

.. doxygenclass:: SyntaxChecker
   :members:

.. doxygenstruct:: LintIssue
   :members:

.. doxygenenum:: LintSeverity

-----

Syntax Engine Functions
-----------------------

.. doxygenfile:: lammpssyntax.h
   :sections: func

-----

Syntax Engine Types
-------------------

.. doxygenenum:: StyleCat

.. doxygenenum:: CmdCat

.. doxygenenum:: ArgRole

.. doxygenenum:: TokType

.. doxygenenum:: CompleterKind

.. doxygenstruct:: ArgSpec
   :members:

.. doxygenstruct:: CommandSpec
   :members:

.. doxygenstruct:: Token
   :members:

.. doxygenstruct:: LineTokens
   :members:

.. doxygenstruct:: CompletionTarget
   :members:

.. doxygennamespace:: SyntaxState

-----

LAMMPS Interface
================

LammpsWrapper Class
-------------------

.. doxygenclass:: LammpsWrapper
   :members:
   :protected-members:

-----

LammpsRunner Class
------------------

.. doxygenclass:: LammpsRunner
   :members:
   :protected-members:

-----

Visualization Components
========================

ChartWindow Class
-----------------

.. doxygenclass:: ChartWindow
   :members:
   :protected-members:

-----

ChartColumn Struct
------------------

``ChartWindow`` owns one ``ChartColumn`` per thermo column: a plain,
move-only data holder (``src/chartviewer.h``) that bundles the column's
``PlotSeries`` objects, cached data bounds, smoothing parameters, display
style, and overlay/reference-line state.  The single ``ChartViewer`` is
rebound (via ``setColumn()``) to whichever column is currently selected.

.. doxygenstruct:: ChartColumn
   :members:

-----

ChartViewer Class
-----------------

.. doxygenclass:: ChartViewer
   :members:
   :protected-members:

-----

PlotWidget Class
----------------

Native ``QWidget`` + ``QPainter`` 2D line/scatter chart renderer
(``src/plotwidget.h``).  It is the sole chart backend; ``ChartViewer`` feeds it
neutral ``PlotSeries`` / ``PlotAxis`` model objects plus the Qt-free
axis-layout helpers from ``plotaxismath`` -- no Qt Charts, Graphs, or QML.

.. doxygenclass:: PlotWidget
   :members:
   :protected-members:

-----

Chart Model and Axis Math
-------------------------

Neutral chart model value types (``src/plotseries.h``) consumed by
``PlotWidget``, and the Qt-free axis-layout helpers (``src/plotaxismath.h``:
nice-number ticks, tick values, and printf-style label formatting).

.. doxygenfile:: plotseries.h

.. doxygenfile:: plotaxismath.h

-----

ImageViewer Class
-----------------

.. doxygenclass:: ImageViewer
   :members:
   :protected-members:

-----

Dump Image Command Builder
--------------------------

The ``DumpImageParams`` struct and the ``buildDumpImageCommand()`` free
function (``src/dumpimage.h``) form a GUI-free, unit-testable core that
assembles the LAMMPS ``write_dump ... image ...`` command from a snapshot of
the viewer state.  ``ImageViewer`` populates the struct (resolving all LAMMPS
queries up front) and then calls the pure builder, which returns a
``DumpImageCommand`` holding the render and ``dump_modify`` argument strings;
``toWriteDumpCommand()`` composes the final one-shot ``write_dump`` command
from those pieces.

.. doxygenstruct:: DumpImageParams
   :members:

-----

.. doxygenfunction:: buildDumpImageCommand

-----

.. doxygenstruct:: DumpImageCommand
   :members:

-----

.. doxygenfunction:: toWriteDumpCommand

-----

Color Maps
----------

The dump-image color maps are defined once, as a table of ``ColorMapDef``
entries in ``src/colormaps.cpp``.  Both the command builder
(``appendColorMapArgs()`` in ``src/dumpimage.cpp``) and the settings-dialog
preview swatches (``addColorMapItems()`` in ``src/imageviewersettings.cpp``)
consume this single source of truth, so they cannot drift apart.  See
:ref:`add_colormap` for how to add or modify a map.

.. doxygenfile:: colormaps.h

-----

SlideShow Class
---------------

.. doxygenclass:: SlideShow
   :members:
   :protected-members:

-----

ImageCache Class
----------------

``SlideShow`` owns an ``ImageCache`` (``src/imagecache.h``) that holds the
temporary files it creates: the PNG copies of image formats that Qt cannot
decode natively, and the frames extracted from imported movie files.  A source
file is converted at most once, since the cache entries are validated against
the modification time and the size of the source file, and the whole cache
directory is removed when the slide show window is closed.

.. doxygenclass:: ImageCache
   :members:
   :protected-members:

-----

Movie Frame Import
------------------

Movie files are turned into a sequence of images before they can be shown in
the slide show viewer.  ``probeMovie()`` collects the properties of the video
stream by running ``ffprobe``, ``MovieImportDialog`` lets the user confirm the
extraction and select a frame range and interval, and ``extractMovieFrames()``
runs ``ffmpeg`` to decode the selected frames into the ``ImageCache``.  The
parsing and frame-counting helpers (``src/movieimport.h``) are free functions
so that they can be unit tested without running either program.

.. doxygenstruct:: MovieInfo
   :members:

-----

.. doxygenclass:: MovieImportDialog
   :members:
   :protected-members:

-----

.. doxygenfunction:: probeMovie

.. doxygenfunction:: parseProbeOutput

.. doxygenfunction:: parseFrameRate

.. doxygenfunction:: selectedFrameCount

.. doxygenfunction:: extractMovieFrames

-----

Dialog Components
=================

FindAndReplace Class
--------------------

.. doxygenclass:: FindAndReplace
   :members:
   :protected-members:

-----

SetVariables Class
------------------

.. doxygenclass:: SetVariables
   :members:
   :protected-members:

-----

ShellAliases Class
------------------

.. doxygenclass:: ShellAliases
   :members:
   :protected-members:

-----

ShellPrompt Class
-----------------

.. doxygenclass:: ShellPrompt
   :members:
   :protected-members:

-----

Index Variable Helpers
----------------------

The parse, merge, and override-detection helpers behind the *Set
Variables* dialog and the editor's override markers are free functions
over plain value types (``src/inputvariables.h``).

.. doxygenstruct:: VariableEntry
   :members:

.. doxygenstruct:: IndexVariableMatch
   :members:

.. doxygenfile:: inputvariables.h
   :sections: func

-----

PlotDataDialog Class
--------------------

.. doxygenclass:: PlotDataDialog
   :members:
   :protected-members:

-----

Preferences Class
-----------------

.. doxygenclass:: Preferences
   :members:
   :protected-members:

-----

GeneralTab Class
----------------

.. doxygenclass:: GeneralTab
   :members:
   :protected-members:

-----

AcceleratorTab Class
--------------------

.. doxygenclass:: AcceleratorTab
   :members:
   :protected-members:

-----

SnapshotTab Class
-----------------

.. doxygenclass:: SnapshotTab
   :members:
   :protected-members:

-----

EditorTab Class
---------------

.. doxygenclass:: EditorTab
   :members:
   :protected-members:

-----

ChartsTab Class
---------------

.. doxygenclass:: ChartsTab
   :members:
   :protected-members:

-----

AboutDialog Class
-----------------

.. doxygenclass:: AboutDialog
   :members:
   :protected-members:

-----

Utility Components
==================

CommandWindow Class
-------------------

.. doxygenclass:: CommandWindow
   :members:
   :protected-members:

-----

WindowLayout Class
------------------

.. doxygenenum:: ViewSlot

.. doxygenenum:: LayoutMode

.. doxygenclass:: WindowLayout
   :members:
   :protected-members:

-----

URLDownloader Class
-------------------

.. doxygenclass:: URLDownloader
   :members:
   :protected-members:

-----

DownloadProgress Class
----------------------

.. doxygenclass:: DownloadProgress
   :members:
   :protected-members:

-----

FileViewer Class
----------------

.. doxygenclass:: FileViewer
   :members:
   :protected-members:

-----

LogWindow Class
---------------

.. doxygenclass:: LogWindow
   :members:
   :protected-members:

-----

FlagWarnings Class
------------------

.. doxygenclass:: FlagWarnings
   :members:
   :protected-members:

-----

ImageInfo Class
---------------

.. doxygenclass:: ImageInfo
   :members:
   :protected-members:

-----

RegionInfo Class
----------------

.. doxygenclass:: RegionInfo
   :members:
   :protected-members:

-----

StdCapture Class
----------------

.. doxygenclass:: StdCapture
   :members:
   :protected-members:

-----

Qt Helper Widgets
-----------------

.. doxygenclass:: QHline
   :members:
   :protected-members:

-----

.. doxygenclass:: QColorCompleter
   :members:
   :protected-members:

-----

.. doxygenclass:: QColorValidator
   :members:
   :protected-members:

-----

.. doxygenclass:: RangeSlider
   :members:
   :protected-members:

-----

.. doxygenclass:: RangeBandSlider
   :members:
   :protected-members:

-----

.. doxygenclass:: VerticalLabel
   :members:
   :protected-members:

-----

StdoutSilencer Class
--------------------

.. doxygenclass:: StdoutSilencer
   :members:
   :protected-members:

-----

QtMessageSilencer Class
-----------------------

.. doxygenclass:: QtMessageSilencer
   :members:
   :protected-members:

-----

PlotData and File Parsers
-------------------------

Column-oriented numeric data model (``src/plotdata.h``) and the parsers for
external data files (whitespace/``.dat``, CSV, LAMMPS YAML, and JSON) used to
plot data from files.

.. doxygenfile:: plotdata.h

-----

Block-Structured fix ave/\* Files
---------------------------------

Data model and parsers (``src/plotblockdata.h``) for the block-structured
output files of ``fix ave/time`` in vector mode, ``fix ave/histo`` and
``fix ave/correlate``, which are reduced to a flat :cpp:class:`PlotData`
before they are plotted.

.. doxygenstruct:: PlotDataBlock
   :members:

.. doxygenstruct:: PlotBlockData
   :members:

.. doxygenfile:: plotblockdata.h
   :sections: func enum

-----

Least-Squares Toolkit
---------------------

Self-contained (Qt-free) dense linear-algebra and least-squares routines
(``src/leastsquares.h``) used by the chart smoothing and reusable for
polynomial and equation-of-state fits.

.. doxygenfile:: leastsquares.h

-----

Post-Processing Analyses
------------------------

Self-contained (Qt-free) post-processing analyses (``src/analysis.h``) used
by the chart post-processing dialog.

.. doxygenfile:: analysis.h

-----

Curve Fitting
-------------

Linear-least-squares curve fits (``src/fitting.h``) -- polynomial and
4-parameter Birch-Murnaghan equation of state -- built on the leastsquares
toolkit and used by the chart post-processing dialog.

.. doxygenfile:: fitting.h

-----

Nonlinear Least Squares
-----------------------

Compact, self-contained (Qt-free) Levenberg-Marquardt solver
(``src/levmar.h``) for nonlinear least-squares fits. The model is
supplied as a residual/Jacobian callback, so the core is independent of
how the model is expressed; it is driven by the custom-fit code with
LeptonMini expressions and their symbolic derivatives, and solves the
damped normal equations with the leastsquares LU solver.  LeptonMini is
based on the Lepton library by Peter Eastman and OpenMM contributors
distributed under the `MIT License
<https://choosealicense.com/licenses/mit/>`_.

.. doxygenfile:: levmar.h

-----

Custom-Function Evaluation and Fitting
--------------------------------------

Evaluation and nonlinear fitting of user-supplied mathematical
expressions (``src/customfunc.h``) via the vendored LeptonMini parser,
used for custom-function plotting and custom curve fits in the chart
post-processing dialog. The fit builds its Jacobian from LeptonMini's
analytic derivatives and minimizes with the Levenberg-Marquardt solver.
LeptonMini is based on the Lepton library by Peter Eastman and OpenMM
contributors distributed under the `MIT License
<https://choosealicense.com/licenses/mit/>`_.

.. doxygenfile:: customfunc.h

-----

.. _helper_functions:

Helper Functions
----------------

.. doxygenfile:: chartstyle.h
   :sections: func

.. doxygenfile:: helpers.h
   :sections: func
