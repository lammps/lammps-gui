*******
Testing
*******

The ``test`` directory contains some tests for the LAMMPS-GUI project
using either the `GoogleTest framework
<https://github.com/google/googletest/>`_ or the `Python unittest
framework <https://docs.python.org/dev/library/unittest.html>`_.

Overview
^^^^^^^^

The test suite uses CMake's CTest front end to select and run the
tests. Tests implemented with GoogleTest are automatically discovered
and can be run individually or as a complete suite for each test
program.  Tests running LAMMPS-GUI itself use the "virtual frame buffer"
X server (Xvfb) and are written in Python using the ``unittest``
Python module and the `PyAutoGUI module
<https://pyautogui.readthedocs.io/>`_

Building the Tests
^^^^^^^^^^^^^^^^^^

Tests are built as part of the main project build when using ``-D
ENABLE_TESTING=ON`` during CMake configuration (default setting is
`OFF`).  Due to technical requirements, testing is currently only
enabled for native Linux builds of LAMMPS-GUI.  In any other build
environment, the ``-D ENABLE_TESTING=ON`` setting is ignored.

Quick Build
===========

For running the tests, it is not necessary to build the documentation,
so its build can be skipped during configuration.

.. code-block:: bash

   cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=ON -D BUILD_DOC=OFF -D ENABLE_TESTING=ON
   cmake --build build --parallel 2

Disable Tests
=============

Tests are disabled by default.  If they have been enabled during CMake configuration
they can be disabled at a later point with:

.. code-block:: bash

   cmake -S . -B build -D ENABLE_TESTING=OFF

Running Tests
^^^^^^^^^^^^^

Below are some frequently used command line examples for running tests.
These examples assume that LAMMPS-GUI was compiled in the folder ``build``.
CTest can also be invoked from the top of the build tree (``--test-dir
build``); both forms work and select the same set of tests.

Run All Tests
=============

.. code-block:: bash

   ctest --test-dir build/test

Run Tests with Verbose Output
=============================

.. code-block:: bash

   ctest --test-dir build/test -V

List Available Tests
====================

The list of the names of all available tests can be obtained with:

.. code-block:: bash

   ctest --test-dir build/test -N


Run Specific Tests
==================

Individual tests can be selected in different ways.  Most common is the
use of regular expressions to select (``-R``) or exclude (``-E``) tests.
It is also possible to select tests by a range of Test numbers (``-I``)
from the -N test list output. Examples:

.. code-block:: bash

   ctest --test-dir build/test -R HelpersTest
   ctest --test-dir build/test -E Frame
   ctest --test-dir build/test -I 20,25

Current Test Coverage
^^^^^^^^^^^^^^^^^^^^^

The unit tests cover the utility functions, the widget-scoped keyboard
shortcuts, the stdout capture, the log window warning highlighter, the
dump-image command builder, the movie import and image cache of the Slide
Show window, the docked window layout, the Tab completion of the command
window's prompt, the plot data model with its file parsers and writers, the
block-structured ``fix ave/*`` file parsers and their reduction to a
plottable table, the column picker dialog, the chart axis-layout math, and
the Qt-free math toolkit (least squares and smoothing, autocorrelation, curve fitting, the
Levenberg-Marquardt solver, the vendored LeptonMini expression parser, and
the custom-function layer on top of them).  Command-line tests validate
basic executable behavior, and PyAutoGUI-based tests exercise the GUI
itself.  Future expansion will include more GUI component testing and
integration tests.

Test Organization
=================

Tests are organized into three main categories:

1. **Unit Tests**: Using GoogleTest framework to test individual functions
2. **Command-Line Tests**: Using command-line to validate basic executable behavior
3. **GUI Tests**: Tests using the Python unittest framework and
   PyAutoGUI to run LAMMPS-GUI inside a virtual frame buffer in a
   "remote controlled fashion".

Unit Tests
==========

test_helpers.cpp
----------------

Comprehensive tests for functions in ``src/helpers.h`` and ``src/helpers.cpp``.
This module contains test cases covering the utility functions used throughout
the application.

**Date Comparison (dateCompare)**
  Tests for the dateCompare function that compares version date strings
  in LAMMPS date format (e.g., "22 Jul 2025"):

  - Same dates (returns 0)
  - Different years (returns positive/negative)
  - Different months (returns positive/negative)
  - Different days (returns positive/negative)
  - Full month names vs. abbreviations
  - Invalid date formats
  - Edge cases (year boundaries, month boundaries)
  - December month ordering
  - Both dates invalid
  - Dates with "- Update" suffix

**Line Splitting (splitLine)**
  Tests for the splitLine function that parses command-line style input
  with proper quote handling:

  - Simple whitespace-separated tokens
  - Single-quoted strings
  - Double-quoted strings
  - Escaped quotes within strings
  - Mixed quoting styles
  - Triple-nested quotes
  - Multiple consecutive whitespace characters
  - Empty input
  - Quotes at string boundaries
  - Single word without spaces
  - Hash comment handling
  - Special characters

**Executable Detection (hasExe)**
  Tests for the hasExe function that checks if an executable exists in PATH:

  - Common system commands (sh, ls on Unix; cmd on Windows)
  - Non-existent commands
  - Empty string input
  - bash executable
  - Platform-specific behavior (conditional compilation)

**Theme Detection (isLightTheme)**
  Tests for the isLightTheme function that determines if the current
  Qt theme is light or dark:

  - Boolean return value validation
  - Consistency across calls
  - No crashes on theme query

**Stdout Silencing (silenceStdout/restoreStdout)**
  Tests for the stdout silencing functions that redirect stdout to suppress
  LAMMPS library output:

  - Silencing actually suppresses stdout
  - Restoring when not silenced is a no-op
  - Nested silencing and restoring
  - Silencing skipped during active StdCapture
  - Capture restores silenced stdout state
  - Silence and restore preserves stdout

**Directory Purging (purgeDirectory)**
  Tests for the purgeDirectory function that recursively removes directory
  contents:

  - Purging directory with files
  - Non-existent directory (no crash)
  - Empty directory

**Image File Detection (isImageFile)**
  Test for the extension-based check that decides whether a file is loaded
  as an image by the Slide Show window.

**Grayscale Conversion (grayscaleImage)**
  Tests for the helper that renders an image in grayscale, used for the
  "inactive" state of icons:

  - Color is removed while the alpha channel is preserved
  - Contrast is faded towards the midpoint

**Qt Message Silencing (QtMessageSilencer)**
  Tests for the :cpp:class:`QtMessageSilencer` RAII guard that collects Qt
  warning messages instead of printing them to the console:

  - Emitted warnings are collected instead of printed
  - Nothing is collected when nothing is emitted
  - The previous message handler is restored and guards can be nested

**Viewer Window Fit (viewerFitSize)**
  Tests for the pure window-fit computation used by the Image Viewer and
  Slide Show window auto-resize:

  - Content within the screen budget is fitted exactly (content plus frame)
  - An exact fit adds no scroll bar allowance
  - Width or height overflow clamps that axis and adds scroll bar room on
    the other axis
  - Overflow in both directions clamps to the budget
  - The added scroll bar room never exceeds the budget
  - A negative budget clamps to zero

test_shortcuts.cpp
------------------

Tests for the ``addShortcut()`` and ``scopeShortcut()`` helpers in
``src/helpers.{h,cpp}``.  The output windows repeat several main window
accelerators, which is harmless while each is a window of its own but
becomes an ambiguous binding once they are docked into the main window, so
both helpers scope a shortcut to its owning widget.  Test cases cover:

- A shortcut added with ``addShortcut()`` firing only while its widget has
  the focus
- ``scopeShortcut()`` binding an action's shortcut to its widget and
  tolerating null arguments
- Scoped shortcuts on sibling widgets not colliding with each other
- A standalone window keeping the shortcuts the main window also binds

test_windowlayout.cpp
---------------------

Tests for the :cpp:class:`WindowLayout` mediator in
``src/windowlayout.{h,cpp}``, which docks the output views into the main
window in the combined layout.  Test cases cover:

- Dock title bars reporting a valid size hint
- Closing a docked view closing its dock, closing a transient view removing
  its dock, and a floating view being left alone
- The saved dock arrangement being keyed by the Qt version, with the legacy
  key dropped when saving
- Closing a panel keeping the proportions of the remaining ones
- A transient view surviving its dock being torn down first
- A docked view keeping its monospace document font when it inherits the
  main window's font

test_shellprompt.cpp
--------------------

Tests for the :cpp:class:`ShellPrompt` input line of the command window in
``src/shellprompt.{h,cpp}``, driven together with its ``QCompleter`` the
way the window manager would.  Test cases cover:

- Tab keeping the focus, offering the matches, taking a single match
  without a list, and walking the matches forward and (with Shift-Tab)
  backward
- Enter taking a chosen match without running it and running the line when
  nothing is chosen or no list is up
- The completion request being announced before matching, so the list to
  complete from can be chosen for the line
- Tab on an empty line, on a read-only prompt, and with a modifier held
  offering nothing

test_stdcapture.cpp
-------------------

Tests for the :cpp:class:`StdCapture` class in
``src/stdcapture.{h,cpp}``, which redirects the C-level stdout file
descriptor through a pipe so that LAMMPS library output can be
collected and displayed in the *Output* window.  Test cases cover:

- Capturing simple single-line output
- Capturing multiple lines of output
- Behavior with empty output
- Re-using a single ``StdCapture`` instance for several capture cycles
- ``getChunk()`` returning incremental output while a capture is active
- ``getChunk()`` returning an empty string when not capturing
- Multiple successive ``getChunk()`` calls during one capture
- ``endCapture()`` being a safe no-op when no capture is active
- ``getBufferUse()`` reporting zero before any capture activity
- ``getBufferUse()`` reflecting the size of the most recent chunk
- The original ``stdout`` file descriptor being restored when capture ends

test_flagwarnings.cpp
---------------------

Tests for the :cpp:class:`FlagWarnings` syntax highlighter used in
the *Output* window to flag lines beginning with ``WARNING`` or
``ERROR`` and to update a running count displayed in the summary
label.  Test cases cover:

- Constructor initializes the warning count to zero
- ``WARNING`` and ``ERROR`` lines are correctly detected
- Normal output lines are not flagged
- The summary ``QLabel`` is updated when warnings appear
- Empty documents produce no spurious warnings
- Warnings are detected when they appear at the start of a line
- The running count is correct for multiple warnings

test_dumpimage.cpp
------------------

Tests for the GUI-free dump-image command builder (``src/dumpimage.{h,cpp}``)
that assembles the LAMMPS ``write_dump ... image`` command from a
``DumpImageParams`` snapshot of the Image Viewer state.  Test cases cover:

- Basic structure of the generated render and ``dump_modify`` arguments
- Pruning of all settings that match the LAMMPS defaults (color tables,
  transparency, background, up direction, sub-box style, axes center)
- The default BWR color map, named and reversed continuous maps, discrete
  bond color maps, and the canonical stops of the perceptual maps
- Element-based vs. type-based coloring (no color map for type coloring)
- Bond rendering: suppressed while VDW spheres are active, drawn when
  requested, auto-bond generation only when a pair style is defined
- ``noinit`` suppressed while a fix is active; disabled fixes are ignored
- Region outline points and bond coloring by computed values

test_movieimport.cpp
--------------------

Tests for the parsing and frame-counting helpers of the movie import
(``src/movieimport.{h,cpp}``).  These are free functions, so the tests run
without ``ffprobe`` or ``ffmpeg`` installed.  Test cases cover:

- ``parseFrameRate()``: rational (``30000/1001``) and plain numbers,
  invalid input
- ``selectedFrameCount()``: full range, range with interval, invalid ranges
- ``parseProbeOutput()``: frame count taken from the container, missing
  frame count or frame size, absent video stream, malformed JSON, and
  numeric fields given as JSON numbers instead of strings

test_imagecache.cpp
-------------------

Tests for the :cpp:class:`ImageCache` class (``src/imagecache.{h,cpp}``),
the temporary-directory backed store for converted images and extracted
movie frames owned by the Slide Show window.  Test cases cover:

- Qt-readable formats are loaded directly and never cached
- Exotic formats are converted at most once; changed source files are
  converted again
- Missing files return a null image; unreadable files are reported once
  and not retried until the file changes
- Usage totals track the converted images
- ``forget()`` drops the conversion of a deleted file; purging conversions
  keeps extracted movie frames and failure records
- Cache subdirectories are unique and sanitized
- ``clear()`` and the destructor remove the temporary directory

test_plotdata.cpp
-----------------

Tests for the column-oriented ``PlotData`` model and the parsers and
writers for external data files (``src/plotdata.{h,cpp}``).  Test cases
cover:

- Appending rows and columns to the model
- CSV import with and without a header line
- Whitespace-separated (``.dat``) import with a LAMMPS-style header
- LAMMPS YAML thermo output, including trailing commas, interleaved log
  lines, and a sequence of maps
- JSON import as array-of-rows and object-of-arrays, with error handling
  for unequal columns and malformed input
- Dispatch by file extension and content-based YAML detection in log files
- CSV, ``.dat``, and YAML export round-trips, including YAML quoting rules

test_plotblockdata.cpp
----------------------

Tests for the parsers of the block-structured output files written by the
``fix ave/*`` styles and for their reduction to a flat table
(``src/plotblockdata.{h,cpp}``).  Test cases cover:

- The native format of ``fix ave/time`` in vector mode, ``fix ave/histo``,
  ``fix ave/correlate``, and the comment-delimited blocks of
  ``fix ave/correlate/long``, including the column and per-block scalar
  names taken from the default header comments
- A single value in vector mode, whose block header is exactly as wide as
  its data rows
- ``overwrite`` (a single block), a header written again mid-file by a
  second fix instance, a last block cut short by an interrupted run, and
  blocks whose row count changes
- Format recognition from the block structure alone, for files whose
  headers were replaced with ``title1``/``title2``/``title3``
- ``fix ave/chunk``: a one-dimensional profile and a compressed-chunk-ID
  file getting the coordinate as their x axis, a cylindrical binning with
  a single axial bin recognized as one-dimensional from its values, and a
  real two-dimensional grid and non-binned chunks keeping the generic
  defaults
- Rejecting flat tables, including a ``fix ave/time`` scalar file whose
  header comment reads like a block file's
- The vector-mode YAML variant, with its synthesized row index, and the
  scalar-mode YAML shape being left to the flat parser
- Single-block and block-range reduction with hand-computed means,
  standard deviations, standard errors, and min/max spreads (whose bars
  are asymmetric and end exactly on the extreme values), clamped and
  reversed ranges, and blocks dropped for having a different shape
- The per-format import defaults, including a running average being
  recognized so that its blocks are not averaged

test_plotdatadialog.cpp
-----------------------

Tests for the :cpp:class:`PlotDataDialog` column picker in
``src/plotdatadialog.{h,cpp}``, run with the ``offscreen`` platform plugin.
Test cases cover:

- Derived columns referencing columns as ``{name}``, including names with
  brackets, and a bare name being rejected
- Renaming a column rewriting the references in already added derived
  columns, and unusable names being refused
- The block reduction keeping renames and derived columns
- ``PlotDataDialog::fromFile()`` picking the block or the flat parser for
  a file and reporting a file that neither can read

test_plotaxismath.cpp
---------------------

Tests for the Qt-free chart axis-layout helpers
(``src/plotaxismath.{h,cpp}``).  Test cases cover:

- ``niceTickInterval()``: 1-2-5-10 interval selection, scaling across
  powers of ten, degenerate ranges, and non-positive tick targets
- ``tickValues()``: even spacing, anchor alignment, snapping zero, endpoint
  inclusion despite floating-point rounding, reversed ranges, custom
  anchors
- ``tickDecimals()``: decimal counts for integer and fractional spacings
- ``formatAxisLabel()``: printf-style integer and floating-point
  specifiers, length-modifier normalization, literal prefix and suffix
  text, and fallback behavior for empty or placeholder-free formats

test_leastsquares.cpp
---------------------

Tests for the dense linear-algebra and smoothing routines
(``src/leastsquares.{h,cpp}``).  Test cases cover:

- Matrix transpose, multiplication, and inversion
- LU linear solve with single and multi-column right-hand sides
- Savitzky-Golay smoothing: moving-average behavior for constant data,
  exact preservation of linear and quadratic data at matching polynomial
  degrees, and noise reduction around a line

test_analysis.cpp
-----------------

Tests for the post-processing analyses (``src/analysis.{h,cpp}``).  Test
cases cover the normalized autocorrelation function: an exact small case,
lag zero being one, empty results for constant or too-short series,
clamping of the maximum lag, and anticorrelation of an alternating series.

test_fitting.cpp
----------------

Tests for the linear-least-squares curve fits (``src/fitting.{h,cpp}``).
Test cases cover recovering known polynomial models and evaluating the
fitted polynomial, recovering a known Birch-Murnaghan equation-of-state
model, and the failure paths for too few data points and non-positive
volumes.

test_levmar.cpp
---------------

Tests for the Levenberg-Marquardt nonlinear least-squares solver
(``src/levmar.{h,cpp}``).  Test cases cover recovering linear,
exponential-decay, and Gaussian models, fitting noisy data, rejecting
underdetermined problems (more parameters than residuals), and reporting a
failing initial model evaluation as an error instead of crashing.

test_lepton.cpp
---------------

Tests for the vendored LeptonMini expression parser
(``thirdparty/lepton_mini``).  Test cases cover expression evaluation,
error handling for invalid expressions, verification of symbolic
derivatives, custom functions, and expression optimization.

test_customfunc.cpp
-------------------

Tests for the custom-function evaluation and fitting layer
(``src/customfunc.{h,cpp}``) built on LeptonMini.  Test cases cover:

- Evaluating user expressions (polynomials, trigonometric functions,
  constants, custom variables) over a sample range
- Skipping non-finite points and clamping the sample count
- Error handling for empty expressions, syntax errors, and undefined
  variables
- Nonlinear custom fits recovering exponential-decay and quadratic models
- Fit-setup validation: variable/parameter clashes, duplicate parameters,
  undeclared symbols, and too few data points

Command-Line Tests
==================

These tests validate the ``lammps-gui`` executable behavior without starting
the full GUI. They run quickly and are useful for CI/CD pipelines.

CommandLine.GetVersion
-----------------------

**Purpose**: Verify version reporting consistency

This test runs::

  lammps-gui --platform offscreen -v

and validates that:

- The executable launches successfully
- Version output includes "LAMMPS-GUI (QT6)"
- Version number matches the ``PROJECT_VERSION`` CMake variable
- Process exits cleanly with status 0

**Environment**: ``OMP_NUM_THREADS=1`` to ensure consistent behavior

CommandLine.HasPlugin
----------------------

**Purpose**: Verify build configuration is reflected in help text

This test runs::

  lammps-gui --platform offscreen -h

and validates that help text is consistent with CMake configuration:

- **Plugin Mode** (``LAMMPS_GUI_USE_PLUGIN=ON``): Help text includes
  "-p, --pluginpath <path>" option
- **Linked Mode** (``LAMMPS_GUI_USE_PLUGIN=OFF``): Help text omits
  plugin path option

**Environment**: ``OMP_NUM_THREADS=1`` to ensure consistent behavior

GUI Tests
=========

These tests validate LAMMPS-GUI functionality using PyAutoGUI and Xvfb (virtual
frame buffer). They run the actual GUI application in a headless X server
environment, allowing automated interaction and screenshot capture.

**Important Note**: The argument for the screen number flag ``-n`` for ``xvfb-run``
*must* be different for each test, so that the tests may run in parallel.

Framebuffer.CreateScreenshot (test_shooter.py)
-----------------------------------------------

**Purpose**: Test the screenshot wrapper utility that abstracts different
screenshooter applications

**Test File**: ``test/test_shooter.py``

This test validates the ``shooter`` wrapper script that provides a unified
interface to various Linux screenshot utilities (ImageMagick's ``import``,
``magick import``, ``xfce4-screenshooter``, ``gnome-screenshot``).

The test runs:

.. code-block:: bash

   xvfb-run -n 11 -s "-screen 0 1024x768x24" -w 1 python test_shooter.py

within a virtual frame buffer and validates:

**ScreenshotChecks.testCreateImage**
  - The ``shooter`` command executes without errors
  - A PNG file is created at the specified path
  - The image dimensions match the virtual frame buffer size (1024x768)
  - The image format is PNG
  - The screenshot captures an all-black screen (expected for empty Xvfb)
  - Specific pixel values at multiple locations are (0,0,0) RGB

**Dependencies**:
  - PyAutoGUI - for screen size detection
  - Pillow (PIL) - for image file validation
  - One of: ImageMagick (``import`` or ``magick``), ``xfce4-screenshooter``,
    or ``gnome-screenshot``

**Setup/Teardown**:
  - ``setUp()``: Removes leftover ``shot.png`` from previous runs
  - ``tearDown()``: Cleans up ``shot.png`` after test completion

**Environment**: Virtual frame buffer at 1024x768x24, ``PYTHONUNBUFFERED=1``,
``PYTHONDONTWRITEBYTECODE=1``, ``OMP_NUM_THREADS=1``

Framebuffer.CheckSize (test_xvfbsize.py)
-----------------------------------------

**Purpose**: Verify PyAutoGUI functionality and Xvfb screen size configuration

**Test File**: ``test/test_xvfbsize.py``

This test validates that PyAutoGUI can properly interact with the virtual
frame buffer created by Xvfb, which is essential for GUI automation tests.

The test runs:

.. code-block:: bash

  xvfb-run -n 12 -s "-screen 0 1024x768x24" -w 1 python test_xvfbsize.py

within a virtual frame buffer and validates:

**PyAutoGUIChecks.testScreenSize**
  - PyAutoGUI correctly detects the screen dimensions
  - Screen width is 1024 pixels
  - Screen height is 768 pixels

**PyAutoGUIChecks.testMousePosition**
  - PyAutoGUI can detect the mouse cursor position
  - Initial mouse position is at screen center (512, 384)
  - ``pyautogui.moveTo()`` can move cursor to absolute positions
  - ``pyautogui.moveRel()`` can move cursor by relative offsets
  - Position queries return expected coordinates after moves

**Dependencies**:
  - PyAutoGUI - for screen size detection and mouse control

**Environment**: Virtual frame buffer at 1024x768x24, ``PYTHONUNBUFFERED=1``,
``PYTHONDONTWRITEBYTECODE=1``, ``OMP_NUM_THREADS=1``

Framebuffer.GUIEditorChecks (test_gui_edit.py)
-----------------------------------------------

**Purpose**: Test basic LAMMPS-GUI editor functionality using automated
GUI interactions

**Test File**: ``test/test_gui_edit.py``

This test validates fundamental editor operations in LAMMPS-GUI by launching
the application inside a virtual frame buffer and automating user interactions
with PyAutoGUI. The test uses screenshot comparison to verify visual state.

The test runs:

.. code-block:: bash

   xvfb-run -n 13 -s "-screen 0 1024x768x24" -w 1 python test_gui_edit.py

within a virtual frame buffer and validates:

**GUIEditorChecks.testExitShortcut**
  - LAMMPS-GUI launches and displays a white editor background
  - The ``Ctrl-Q`` keyboard shortcut exits the application cleanly
  - The process exits with status 0
  - Screenshots confirm the window was displayed and then closed

**GUIEditorChecks.testExitMenu**
  - The ``Alt-F`` menu shortcut opens the File menu
  - The ``Q`` key selects the Quit entry
  - The application exits cleanly with status 0
  - Screenshots confirm expected visual state

**GUIEditorChecks.testExitModCancelNo**
  - Text can be typed into the editor buffer
  - Exiting with a modified buffer shows a confirmation dialog
  - The ``Cancel`` option returns to the editor without exiting
  - The ``No`` option exits without saving

**Dependencies**:
  - PyAutoGUI - for keyboard and mouse automation
  - Pillow (PIL) - for screenshot validation
  - A supported screenshooter application

**Setup/Teardown**:
  - ``setUp()``: Launches LAMMPS-GUI, cleans up leftover test files
  - ``tearDown()``: Terminates the LAMMPS-GUI process

**Environment**: Virtual frame buffer at 1024x768x24, ``PYTHONUNBUFFERED=1``,
``PYTHONDONTWRITEBYTECODE=1``, ``OMP_NUM_THREADS=1``,
``LAMMPS_GUI=<path to executable>``

Test Fixtures and Utilities
============================

**HelpersTest Fixture**
  Base test fixture that creates a ``QCoreApplication`` instance for tests
  that require Qt functionality. The application is created once per test
  suite and reused across tests for efficiency.  Similar lightweight
  fixtures (``StdCaptureTest``, ``FlagWarningsTest``) are used by the
  matching unit test programs.

**Platform-Specific Testing**
  Tests use conditional compilation (``#ifdef _WIN32``) to adapt to
  platform differences in:

  - Path separators
  - Line endings
  - Available system executables
  - Default shell commands

Future Test Expansion
=====================

Planned additions to the test suite include:

**GUI Component Tests**
  - CodeEditor text manipulation
  - Syntax highlighter accuracy
  - Find/replace functionality
  - Auto-completion behavior

**LAMMPS Integration Tests**
  - LammpsWrapper command execution
  - Variable substitution
  - Error handling
  - Output capture

**File I/O Tests**
  - File opening/saving
  - Recent files management
  - Auto-save functionality
  - Data file inspection

**Preferences Tests**
  - Settings persistence
  - Default value initialization
  - Migration between versions

**Tutorial Tests**
  - Tutorial file generation
  - Directory setup
  - Resource extraction

Adding Tests
^^^^^^^^^^^^

Create a New Test File
======================

1. Create a new test file in the ``test/`` directory (e.g., ``test_newfile.cpp``)
2. Add the test executable to ``test/CMakeLists.txt``:

.. code-block:: cmake

   add_executable(test_newfile
     test_newfile.cpp
     ${CMAKE_SOURCE_DIR}/src/newfile.cpp
   )

   target_include_directories(test_newfile PRIVATE
     ${CMAKE_SOURCE_DIR}/src
   )

   target_link_libraries(test_newfile
     GTest::gtest_main
     Qt6::Widgets
   )

   gtest_discover_tests(test_newfile)


Add Tests to Existing File
==========================

Add new test cases using GoogleTest macros:

.. code-block:: cpp

   TEST_F(HelpersTest, NewTestName)
   {
       // Arrange
       std::string input = "test data";

       // Act
       auto result = function_to_test(input);

       // Assert
       EXPECT_EQ(result, expected_value);
   }

Dependencies
^^^^^^^^^^^^

- **GoogleTest**: Automatically fetched via CMake FetchContent (v1.17.0)
- **Qt6**: Required for Qt-dependent functions (Widgets component)
- **CTest**: Part of CMake, used for test execution

Notes
^^^^^

- Tests that require a Qt application context use a ``HelpersTest`` fixture that creates a ``QCoreApplication`` instance.
- Platform-specific tests (e.g., ``has_exe``) use conditional compilation to test appropriate commands on different operating systems.
- The test suite is designed to be easily extended with additional test files and test cases.
- GoogleTest is fetched automatically during CMake configuration, so no manual installation is required.

CI Integration
^^^^^^^^^^^^^^

The test suite integrates with existing CI workflows:
- Tests run as part of the standard build process when ``ENABLE_TESTING=ON``
- CTest provides standard output for CI systems
- Tests can be disabled for documentation-only builds
