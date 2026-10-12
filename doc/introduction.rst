********
Overview
********

LAMMPS-GUI is built using C++17 and the Qt Framework (Qt 6.2+).  The
application follows object-oriented design principles with separation of
concerns between different components:

- **Editor Components**: Handle text editing, syntax highlighting, and auto-completion
- **LAMMPS Interface**: Wraps the LAMMPS C library API
- **Visualization**: Displays images, charts, and simulation output
- **GUI Framework**: Main window, dialogs, and preferences

==================
 LAMMPS Interface
==================

LAMMPS-GUI can operate in two modes: **Plugin Mode** and **Linked
Mode**.  The mode is controlled by the
``-D LAMMPS_GUI_USE_PLUGIN=(ON|OFF)`` CMake configuration option.

**Plugin Mode** (default)
  LAMMPS is loaded dynamically at runtime from a shared library file
  (.so, .dll, .dylib).  This allows using different LAMMPS builds with
  different compilation settings and different LAMMPS versions without
  recompiling the GUI. The library loading is handled in
  :cpp:class:`LammpsWrapper` using platform-specific dynamic loading
  functions (``dlopen()`` on Unix/Linux/macOS, ``LoadLibrary()`` on
  Windows).  The path to the shared library file is auto-detected or
  configured via command line or preferences.

**Linked Mode**
  The LAMMPS library is linked at compile time.  Used by default when
  building LAMMPS-GUI as part of a LAMMPS CMake build with
  ``-D BUILD_LAMMPS_GUI=on``.  For standalone builds, the
  ``-D LAMMPS_SOURCE_DIR=<path to LAMMPS' src folder>`` and
  ``-D LAMMPS_LIBRARY=<path to LAMMPS shared or static library file>``
  settings are also required when configuring with CMake.  It may be
  necessary to adjust environment variables to find shared libraries
  (``LD_LIBRARY_PATH`` on Linux, ``DYLD_LIBRARY_PATH`` on macOS, or
  ``PATH`` on Windows) when linked to a shared library.

================
 Qt Integration
================

LAMMPS-GUI makes extensive use of Qt features:

**Signals and Slots**
  Used for inter-component communication, especially between GUI
  components and background threads.

**Qt Resource System**
  Icons and resources embedded via ``resources/lammpsgui.qrc``.  The icons
  are SVG and are rendered through the Qt6 Svg module (``Qt6::Svg``).

**Qt Models**
  Used for data display in various viewers and inspectors.

LAMMPS-GUI requires the Qt application development and GUI framework
version 6.2 or later, including the Widgets, Network, and Svg modules.
See the `Qt Documentation <https://doc.qt.io/>`_ for more details.

------------------

************
Architecture
************

=================
 Main Components
=================

The application architecture consists of several key components organized into
functional groups:

Main Window
-----------

**LammpsGui (lammpsgui.h/.cpp)**
  The main window class that coordinates all other components. It
  manages the editor, handles file operations, controls LAMMPS
  execution, and manages the overall application state. This is the
  central hub of the application that integrates all other components.
  The UI is built programmatically in ``setupUi()``, which delegates
  menu construction to ``createFileMenu()``, ``createEditMenu()``,
  ``createRunMenu()``, ``createViewMenu()``, ``createTutorialMenu()``,
  ``createAboutMenu()``, and the status bar to ``createStatusBar()``.
  Plugin discovery and accelerator setup are handled by
  ``setupPlugin()`` and ``setupAccelerators()``.
  See :cpp:class:`LammpsGui`

Editor Components
-----------------

**CodeEditor (codeeditor.h/.cpp)**
  Custom text editor widget based on `QPlainTextEdit
  <https://doc.qt.io/qt-6/qplaintextedit.html>`_, providing
  LAMMPS-specific features including syntax highlighting,
  auto-completion, line numbers, and context-sensitive help. The main
  editing surface for LAMMPS input scripts.  See :cpp:class:`CodeEditor`

**LineNumberArea (linenumberarea.h)**
  Widget that displays line numbers in the left margin of the
  CodeEditor.  Updates dynamically as text is added or removed.  See
  :cpp:class:`LineNumberArea`

**Highlighter (highlighter.h/.cpp)**
  Syntax highlighter for LAMMPS input scripts. Categorizes and colors
  different types of commands, keywords, variables, and comments using
  Qt's QSyntaxHighlighter framework.  See :cpp:class:`Highlighter`

**FindAndReplace (findandreplace.h/.cpp)**
  Dialog for searching and replacing text in the editor. Supports
  case-sensitive search, wrap-around search, and whole-word matching
  options.  See :cpp:class:`FindAndReplace`

LAMMPS Interface
----------------

**LammpsWrapper (lammpswrapper.h/.cpp)**
  C++ wrapper around the LAMMPS C library interface. Provides a clean
  C++ API and handles dynamic library loading in plugin mode. Manages
  LAMMPS initialization, command execution, and error handling.  See
  :cpp:class:`LammpsWrapper`

**LammpsRunner (lammpsrunner.h)**
  Worker thread for executing LAMMPS simulations without blocking the
  GUI.  Uses Qt's threading facilities to run simulations in the
  background, allowing the UI to remain responsive during long
  calculations.  See :cpp:class:`LammpsRunner`

Visualization Components
------------------------

**ImageViewer (imageviewer.h/.cpp)**
  Dialog for viewing and manipulating LAMMPS snapshot images created by
  the ``dump image`` command.  Supports interactive control of
  visualization parameters such as zoom, rotation, atom size, coloring,
  and rendering options.  Changes can be applied to regenerate the image
  using the LAMMPS library interface.  See :cpp:class:`ImageViewer`.
  This uses two internal helper classes:

  - **ImageInfo** - Stores settings for displaying graphics from a LAMMPS
    compute or fix in snapshot images.
  - **RegionInfo** - Stores settings for displaying a region in snapshot images.

**ChartWindow (chartviewer.h/.cpp)**
  Window for displaying thermodynamic data as charts.  Supports line plots
  and multiple data series.  It owns one ``ChartColumn`` per thermo column,
  which holds the neutral ``PlotSeries`` data objects of that column, and a
  single :cpp:class:`ChartViewer` that is rebound to whichever column is
  selected.  See :cpp:class:`ChartWindow`

**ChartViewer (chartviewer.h/.cpp)**
  Custom chart view widget that provides interactive features like zooming,
  smoothing, and panning for data visualization.  ChartViewer is a view over
  the active ``ChartColumn`` of its window and renders it with
  :cpp:class:`PlotWidget`.  See :cpp:class:`ChartViewer`.

**PlotDataDialog (plotdatadialog.h/.cpp)**
  Column picker for data files opened for plotting: selects the x and the y
  columns, renames columns, adds derived columns computed from expressions,
  and, for block-structured files, reduces the blocks to a single one or to
  the average of a range.  ``PlotDataDialog::fromFile()`` reads a file and
  builds the dialog matching its format.  See :cpp:class:`PlotDataDialog`

**PlotBlockData (plotblockdata.h/.cpp)**
  Parsers for the block-structured output files of the ``fix ave/*`` styles
  and their reduction to a flat :cpp:class:`PlotData` table.  The helpers
  shared with the flat-file parsers of ``plotdata.cpp`` are declared in
  ``plotdata_internal.h``.  See :cpp:class:`PlotBlockData`

**PlotWidget (plotwidget.h/.cpp)**
  Native ``QWidget`` + ``QPainter`` 2D line/scatter chart renderer.  It is
  the only chart backend and depends only on Qt Widgets -- no Qt Charts, Qt
  Graphs, or QML.  Axis-layout math (nice ticks, label formatting) lives in
  the Qt-free ``plotaxismath`` helpers.  See :cpp:class:`PlotWidget`.

**SlideShow (slideshow.h/.cpp)**
  Dialog for viewing multiple images as a slideshow or animation with
  navigation controls.  Supports converting an animation to a movie file
  when `FFmpeg <https://ffmpeg.org/>`_ or `ImageMagick
  <https://imagemagick.org/>`_ is available.  See :cpp:class:`SlideShow`

**RangeSlider (thirdparty/rangeslider/rangeslider.h/.cpp)** Custom
  slider widget with two handles for selecting a range of values. This
  is code written by Hoyoung Lee and distributed under the `CeCILL Free
  Software License <https://choosealicense.com/licenses/cecill-2.1/>`_.
  Used in :cpp:class:`ChartWindow` for selecting x- and y-direction plot
  ranges.  See :cpp:class:`RangeSlider`

**RangeBandSlider (rangebandslider.h/.cpp)**
  Horizontal ``QSlider`` that paints an active sub-range on its track,
  distinct from the third-party :cpp:class:`RangeSlider`.  See
  :cpp:class:`RangeBandSlider`

Dialog and Utility Components
-----------------------------

**LogWindow (logwindow.h/.cpp)**
  Window displaying captured output from LAMMPS simulations.  Updates in
  real-time as the simulation progresses and highlights warning and
  error messages.  Provides navigation to jump between warnings.  See
  :cpp:class:`LogWindow`

**Preferences (preferences.h/.cpp)**
  Dialog for configuring application settings including accelerator
  packages, editor appearance, snapshot settings, and chart
  preferences. Settings are made persistent across LAMMPS-GUI sessions
  using the `QSettings class <https://doc.qt.io/qt-6/qsettings.html>`_.
  See :cpp:class:`Preferences`.  The dialog is organized into five tabs,
  each implemented as a separate widget class:

  - :cpp:class:`GeneralTab` - General settings (LAMMPS library path, fonts, etc.)
  - :cpp:class:`AcceleratorTab` - LAMMPS accelerator package configuration
  - :cpp:class:`SnapshotTab` - Snapshot image viewer defaults
  - :cpp:class:`EditorTab` - Editor appearance and behavior settings
  - :cpp:class:`ChartsTab` - Chart viewer display settings

**SetVariables (setvariables.h/.cpp)**
  Dialog for editing LAMMPS index-style variable definitions. Allows
  users to define name-value pairs that are substituted in input scripts
  using ``${varname}`` syntax.  See :cpp:class:`SetVariables`

**FileViewer (fileviewer.h/.cpp)**
  Read-only text viewer dialog for displaying file contents. Used for
  viewing auxiliary files without allowing modifications.  See
  :cpp:class:`FileViewer`

**CommandWindow (commandwindow.h/.cpp)**
  Shell prompt with scrollback next to the simulation.  It runs one
  persistent shell process without a terminal, frames each command with a
  sentinel line that reports the exit status and the working directory,
  and offers commands that hand files to the slide show, the editor, or a
  plot.  See :cpp:class:`CommandWindow`.  It uses two helper classes:

  - :cpp:class:`ShellPrompt` - The input line with Tab completion of
    commands, file names, and earlier command lines.
  - :cpp:class:`ShellAliases` - The configurable aliases, which are
    defined in the shell at start since a non-interactive shell reads no
    startup files.

**TutorialWizard (tutorialwizard.h/.cpp)**
  Wizard dialog for interactive LAMMPS tutorials. Guides users through
  setting up tutorial directories and files, providing a structured
  learning experience.  See :cpp:class:`TutorialWizard`

**AboutDialog (aboutdialog.h/.cpp)**
  Custom About dialog that displays version information, LAMMPS
  configuration details, and available styles in two scrollable text
  areas.  The dialog automatically scrolls down when the content exceeds
  the visible area, pauses at the bottom, and then returns back to the
  top.  See :cpp:class:`AboutDialog`

Support Components
------------------

**WindowLayout (windowlayout.h/.cpp)**
  Mediator between the main window and its output views that implements
  the two window layouts: individual windows, or docked panels in a single
  main window with their proportions kept across resizes and sessions.
  The views themselves do not know which layout they are in.  See
  :cpp:class:`WindowLayout`

**URLDownloader (urldownloader.h/.cpp)**
  Utility class for downloading files over HTTPS.  Provides a
  synchronous download interface using ``QNetworkAccessManager`` with
  ``QEventLoop``.  Respects the ``https_proxy`` preference setting and
  the ``https_proxy`` environment variable.  After downloading a file,
  it checks for a ``SHA256SUMS`` file in the same remote directory and
  verifies the SHA-256 checksum if available.
  See :cpp:class:`URLDownloader`

**StdCapture (stdcapture.h/.cpp)**
  Utility class that captures stdout output from LAMMPS.  Redirects
  the C-level stdout file descriptor through a pipe to allow capturing
  output from the LAMMPS library without blocking the GUI thread.
  See :cpp:class:`StdCapture`

**FlagWarnings (flagwarnings.h/.cpp)**
  Syntax highlighter for LAMMPS warning and error messages in log
  output.  Detects and highlights WARNING/ERROR lines and URLs for
  documentation links.  Maintains a count of warnings and updates a
  summary label.  See :cpp:class:`FlagWarnings`

**QHline (qaddon.h/.cpp)**
  Simple horizontal line widget for visual separation in dialogs and
  forms.  See :cpp:class:`QHline`

**QColorCompleter (qaddon.h/.cpp)**
  Auto-completer for color name inputs, suggesting valid Qt color names
  as the user types.  See :cpp:class:`QColorCompleter`

**QColorValidator (qaddon.h/.cpp)**
  Validator for color input fields, ensuring they contain valid color
  names or hex color codes.  See :cpp:class:`QColorValidator`

**VerticalLabel (qaddon.h/.cpp)**
  Label widget that renders text rotated 90 degrees for vertical
  display.  See :cpp:class:`VerticalLabel`

Helper Functions
----------------

The :ref:`helpers module <helper_functions>` provides utility functions
used throughout the application.  The ``chartstyle.h/.cpp`` module next to
it holds the palette of preset series colors and the widget builders that
the Chart Style dialog and the charts preferences tab share, so that both
offer the same choices.  The helper functions include:

- Date comparison (``dateCompare`` for version comparisons)
- Command-line parsing (``splitLine`` with quote handling)
- System utilities (``hasExe`` for executable detection)
- UI utilities (``isLightTheme`` for theme detection,
  ``showUnsavedChangesDialog`` for standardized unsaved-changes prompts)
- Menu construction (``addMenuAction`` builds a menu action with an
  optional icon and a ``triggered()`` handler in one call; used to build
  the context and tool menus across the widget classes)
- Stdout management (``silenceStdout``/``restoreStdout`` for suppressing
  LAMMPS library output, with the :cpp:class:`StdoutSilencer` RAII guard
  for scope-based silencing, coordinated with :cpp:class:`StdCapture` via
  ``isStdoutSilenced`` and ``notifyCaptureState``)

**Constants (constants.h)**
  The ``Cfg`` namespace centralizes application-wide magic numbers and
  repeated string literals, such as default buffer sizes, minimum window
  dimensions, file limits, resource paths, and version constants, while
  the ``Keys`` namespace holds every persisted ``QSettings`` key and group
  name.  Using named constants avoids typos and makes maintenance easier.

===========
 Data Flow
===========

1. **User Input**: User edits LAMMPS input in CodeEditor with syntax highlighting
2. **Execution Request**: User triggers execution via menu or button
3. **Preparation**: LammpsGui creates/configures LammpsWrapper and prepares variables
4. **Threading**: Commands sent to LammpsRunner thread to avoid UI blocking
5. **Execution**: LammpsRunner executes commands via LammpsWrapper
6. **Output Capture**: Output captured via StdCapture for display
7. **Visualization**: Results displayed in LogWindow, ImageViewer, or ChartWindow
8. **Completion**: UI updated when execution completes, progress indicators cleared

===============================
 Settings and State Management
===============================

The application uses Qt's QSettings mechanism to persist:

- Recent files list
- Window geometry and state
- Editor preferences (font, colors)
- Accelerator settings
- LAMMPS plugin path
- Tutorial preferences

Settings are stored in platform-specific locations (the application name
includes the Qt major version, e.g. ``LAMMPS-GUI (QT6)``):

- Linux: ``~/.config/The LAMMPS Developers/LAMMPS-GUI (QT6).conf``
- macOS: ``~/Library/Preferences/org.lammps.LAMMPS-GUI (QT6).plist``
- Windows: Registry under ``HKEY_CURRENT_USER\Software\The LAMMPS Developers\LAMMPS-GUI (QT6)``

=================
 Threading Model
=================

The application uses Qt's event-driven architecture with threading:

- **Main Thread**: Handles all UI operations and user interactions
- **LAMMPS Thread**: LammpsRunner executes LAMMPS in a separate QThread
- **Communication**: Signals/slots for thread-safe communication between threads

This design keeps the UI responsive even during long-running simulations.
