# LAMMPS-GUI Development Guide for Copilot

## Project Overview

**LAMMPS-GUI** is a graphical user interface for LAMMPS (Large-scale Atomic/Molecular Massively Parallel Simulator). It's a standalone Qt-based C++ application that provides:
- A syntax-highlighting code editor for LAMMPS input files
- Direct execution of LAMMPS simulations
- Real-time visualization and monitoring of simulation output
- Chart plotting capabilities
- Image/slideshow viewing of simulation snapshots

**Repository**: https://github.com/lammps/lammps-gui
**Documentation**: https://lammps-gui.lammps.org/
**Version**: 2.0.4 (see CMakeLists.txt line 4)
**License**: GNU GPL v2

### Key Statistics
- **Language**: C++ (C++17 standard)
- **Framework**: Qt (requires Qt 6.2+; Qt 6.10+ for QtGraphs support)
- **Build System**: CMake 3.20+
- **Codebase Size**: ~17.8k lines across 50 source files in `src/`
- **Total Files**: ~11,290 files (includes docs, resources)
- **Disk Size**: ~263 MB

## Architecture & File Layout

### Root Directory Structure
```
├── CMakeLists.txt          # Main build configuration (480 lines)
├── README.md               # Brief overview with CI badges
├── TODO.md                 # Feature roadmap
├── LICENSE                 # GPL v2 license
├── .clang-format           # C++ code formatting rules (LLVM-based, 100 char limit)
├── .gitignore              # Build artifacts, temp files
├── src/                    # C++ source files (50 files)
├── doc/                    # Sphinx documentation (RST format)
├── resources/              # Icons, QRC files, help text
├── plugin/                 # Plugin loader for dynamic LAMMPS library loading
├── packaging/              # Platform-specific packaging scripts
└── .github/                # CI workflows, issue templates
```

### Source Code Organization (`src/`)
**Main application files**:
- `main.cpp` - Application entry point, command-line parsing
- `lammpsgui.{cpp,h}` - Main window and central GUI logic (UI created programmatically in `setupUi()`)
- `lammpswrapper.{cpp,h}` - C++ interface to LAMMPS C library

**Editor components**:
- `codeeditor.{cpp,h}` - Custom editor widget with line numbers
- `highlighter.{cpp,h}` - LAMMPS syntax highlighting
- `linenumberarea.h` - Line number display
- `findandreplace.{cpp,h}` - Search/replace dialog

**Visualization components**:
- `imageviewer.{cpp,h}` - Image display with zoom/pan
- `chartviewer.{cpp,h}` - Chart display with pluggable backend (QtCharts or QtGraphs)
- `chartbackend.h` - Abstract interface for chart backends
- `qtchartsbackend.{cpp,h}` - QtCharts chart backend implementation
- `qtgraphsbackend.{cpp,h}` - QtGraphs chart backend implementation
- `slideshow.{cpp,h}` - Multi-image slideshow
- `rangeslider.{cpp,h}` - Custom slider widget for image sequences

**Dialogs & helpers**:
- `aboutdialog.{cpp,h}` - Custom About dialog with auto-scroll
- `preferences.{cpp,h}` - Settings dialog
- `setvariables.{cpp,h}` - Variable substitution dialog
- `tutorialwizard.{cpp,h}` - Tutorial setup wizard dialog
- `fileviewer.{cpp,h}` - File content viewer
- `logwindow.{cpp,h}` - Log output viewer
- `helpers.{cpp,h}` - Utility functions
- `stdcapture.{cpp,h}` - stdout/stderr capture
- `flagwarnings.{cpp,h}` - LAMMPS flag validation
- `qaddon.{cpp,h}` - Qt helper widgets (QHline, QColorCompleter, QColorValidator, VerticalLabel)
- `constants.h` - Application-wide constants (GuiConstants namespace)

**Networking**:
- `urldownloader.{cpp,h}` - HTTPS file download with SHA-256 checksum verification

**Runner**:
- `lammpsrunner.{cpp,h}` - Thread-based LAMMPS execution

**Note**: All classes are documented in the Architecture section of `doc/introduction.rst` with detailed descriptions organized into:
- Main Window and Application Control (LammpsGui, TutorialWizard)
- Editor Components (CodeEditor, LineNumberArea, Highlighter, FindAndReplace)
- LAMMPS Interface (LammpsWrapper, LammpsRunner)
- Visualization Components (ImageViewer, ChartWindow, ChartViewer, ChartBackend, QtChartsBackend, QtGraphsBackend, SlideShow, RangeSlider)
- Dialog Components (Preferences, SetVariables, FileViewer, LogWindow, AboutDialog)
- Support Components (URLDownloader, StdCapture, FlagWarnings, Qt helper widgets)

### Documentation (`doc/`)
- **Format**: reStructuredText (Sphinx) + Doxygen
- **Build Target**: `html` (creates `build-doc/doc/html/`)
- **Requirements**: `doc/requirements.txt` (Sphinx 6-8.2.3, Breathe, extensions)
- **Doxygen**: `Doxyfile.in` configures API documentation generation
- **Key Files**:
  - `index.rst` - Main documentation index with programmer's guide
  - `api_reference.rst` - Doxygen-generated API documentation
  - `introduction.rst` - Architecture overview of all classes
  - `testing.rst` - Testing infrastructure and test case documentation
  - `installation.rst`, `basic_usage.rst` - User-facing documentation

### Resources (`resources/`)
- `lammpsgui.qrc` - Qt resource collection file
- `icons/` - 80+ PNG icons, ICO/ICNS files
- `lammps_internal_commands.txt` - Command reference for auto-completion
- `help_index.table` - Help system index

### Packaging Scripts (`packaging/`)
- `build_linux_tgz.sh` - Linux tarball creation
- `build_macos_dmg.sh` - macOS DMG installer
- `lammps-gui.desktop` - Linux desktop entry
- `lammps-gui.appdata.xml` - Linux appdata metadata
- `org.lammps.lammps-gui.yml` - Flatpak manifest

### Test Infrastructure (`test/`)
- **Framework**: GoogleTest v1.17.0 (auto-fetched via CMake FetchContent)
- **CMakeLists.txt**: Test configuration and executable definitions
- **test_helpers.cpp**: Unit tests for utility functions (38 test cases; `mystrdup` was removed from production code so its tests are gone)
- **test_flagwarnings.cpp**: Unit tests for FlagWarnings syntax highlighter (7 test cases)
- **test_stdcapture.cpp**: Unit tests for StdCapture output capture (11 test cases)
- **test_shooter.py, test_xvfbsize.py, test_gui_edit.py**: Python-based GUI tests using PyAutoGUI and Xvfb
- **Enable**: Use `-D ENABLE_TESTING=ON` (OFF by default to speed up builds)

## Build System & Configuration

### CMake Build Modes

**1. Plugin Mode (Default, Standalone)**
```bash
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no
cmake --build build --parallel 2
```
- Loads LAMMPS library dynamically at runtime
- Does NOT require LAMMPS source or library at build time
- Default mode when building standalone

**2. Linked Mode (When Built with LAMMPS)**
```bash
cmake -S . -B build \
  -D LAMMPS_GUI_USE_PLUGIN=off \
  -D LAMMPS_SOURCE_DIR=/path/to/lammps/src \
  -D LAMMPS_LIBRARY=/path/to/liblammps.so \
  -D BUILD_DOC=no
cmake --build build
```
- Links directly to LAMMPS library
- Required when building as part of LAMMPS with `-D BUILD_LAMMPS_GUI=on`

**3. Documentation-Only Build**
```bash
cmake -S . -B build-doc -D BUILD_DOC_ONLY=yes
cmake --build build-doc --target doc
```
- Does NOT build the C++ application
- Only builds Sphinx HTML documentation
- Output: `build-doc/doc/html/`

### Important CMake Options
- `LAMMPS_GUI_USE_PLUGIN` - ON (default) for plugin mode, OFF for linked mode
- `LAMMPS_GUI_USE_QTCHARTS` - ON to use QtCharts even if QtGraphs is available, OFF (default) use QtGraphs when available
- `BUILD_DOC` - ON (default) builds docs with app, OFF skips docs
- `BUILD_DOC_ONLY` - ON builds only docs (no app), OFF (default) builds app
- `ENABLE_TESTING` - ON enables GoogleTest unit tests, OFF (default) disables tests
- `CMAKE_CXX_STANDARD` - 17 (required minimum), 23 supported
- `CMAKE_BUILD_TYPE` - Release or Debug

### Platform-Specific Notes

#### Linux (Ubuntu 22.04+)
**Required packages**:
```bash
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    libfontconfig1 \
    libglu1-mesa-dev \
    libomp-dev \
    mesa-common-dev \
    qt6-base-dev qt6-charts-dev \    # For Qt6
    doxygen \                         # For documentation
    python3 python3-venv              # For Sphinx documentation build
```

#### macOS (11+)
- Supports both arm64 and x86_64 architectures
- Multi-arch build: `-D CMAKE_OSX_ARCHITECTURES=arm64;x86_64`
- Compatibility: `-D CMAKE_OSX_DEPLOYMENT_TARGET=11.0`
- Creates app bundle automatically
- DMG target: `cmake --build build --target dmg`

#### Windows
- Supports both MSVC (Visual Studio 2022) and MinGW cross-compilation from Linux
- Plugin mode (`LAMMPS_GUI_USE_PLUGIN=yes`) is supported on Windows
- MSVC requires Visual C++ 14.36+
- Qt installation typically in `C:\Qt\`

### Build Targets
- `lammps-gui` (default) - Main executable
- `doc` - Build HTML documentation with Doxygen + Sphinx (alias for `html`)
- `html` - Build complete HTML documentation (Doxygen → XML → Breathe → Sphinx)
- `doxygen` - Run Doxygen only to generate XML (intermediate step)
- `pdf` - Build PDF documentation (requires pdflatex, latexmk)
- `spelling` - Run spell check on docs
- `tarball` - Create source tarball (requires Git)
- `tgz` - Linux binary tarball (plugin mode only)
- `dmg` - macOS DMG installer (macOS only, plugin mode only)
- `flatpak` - Flatpak bundle (currently not supported in plugin mode)

## Continuous Integration & Quality Checks

### GitHub Actions Workflows (`.github/workflows/`)

**1. `compile-linux-qt6.yml`** - Qt 6.x Build
- **Runs on**: Push/PR to `develop` branch
- **Platform**: ubuntu-latest (Qt 6.x)
- **Config**: Debug build, C++23, `-Wall -Wextra`
- **Validation**: Runs `lammps-gui --platform offscreen -v`
- **Duration**: ~2-5 minutes

**2. `build-html-docs.yml`** - Documentation Build
- **Runs on**: Push/PR to `develop` branch
- **Platform**: ubuntu-latest
- **Build**: `cmake -S . -B build-doc -D BUILD_DOC_ONLY=yes`
- **Target**: `cmake --build build-doc --target doc`
- **Duration**: ~3-5 minutes (includes pip install)

**3. `codeql-analysis.yml`** - Static Analysis
- **Runs on**: Push to `develop` (NOT on PRs)
- **Platform**: ubuntu-latest (Qt 6.x)
- **Tool**: GitHub CodeQL for C++
- **Config**: `.github/codeql/cpp.yml` (scans only `src/`)
- **Duration**: ~5-10 minutes

### Pre-Commit Validation Checklist
Before submitting a PR, ensure:
1. **Code compiles** with Qt6 and both QtGraphs (if available) and QtCharts
2. **Documentation builds** without errors (`cmake --build build-doc --target doc`)
3. **Application runs** (`./build/lammps-gui --platform offscreen -v` shows version)
4. **Code follows style**: Use `.clang-format` (LLVM-based, 100 char limit)
5. **No new compiler warnings** with `-Wall -Wextra`
6. **Tests pass** if modifying tested components (`ctest --test-dir build/test`)
7. **Doxygen comments** added for new public classes/methods
8. **CodeQL scans pass** (checked automatically on push to develop)
9. **Documentation updated**: User's guide, programmer's guide, and API reference updated for any new features or changed behavior

## Common Build Patterns & Issues

### Pattern 1: Clean Build (Documentation Only)
```bash
rm -rf build-doc
cmake -S . -B build-doc -D BUILD_DOC_ONLY=yes
cmake --build build-doc --target doc
# Output: build-doc/doc/html/index.html
```
**Time**: 3-5 minutes (includes virtualenv setup and pip install)
**Pitfall**: None, very reliable

### Pattern 2: Quick Application Build (No Docs, Plugin Mode)
```bash
mkdir -p build
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no
cmake --build build --parallel 2
./build/lammps-gui --platform offscreen -v
```
**Time**: 1-3 minutes (after Qt installed)
**Pitfall**: Requires Qt6 development packages installed

### Pattern 3: Full Build with Documentation
```bash
mkdir -p build
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=yes
cmake --build build --parallel 2
```
**Time**: 5-8 minutes
**Pitfall**: Longer due to virtualenv + Sphinx installation

### Common Build Errors & Solutions

**Error**: `Could NOT find Qt6`
- **Cause**: Qt development packages not installed
- **Solution**: Install Qt dev packages (see platform-specific notes above)
- **Alternative**: For docs only, use `-D BUILD_DOC_ONLY=yes`

**Error**: Sphinx import errors during doc build
- **Cause**: virtualenv creation or pip install failed
- **Solution**: Check Python 3.8+ is available; delete `build-doc/docenv` and retry
- **Workaround**: Run manually: `python3 -m venv docenv && docenv/bin/pip install -r doc/requirements.txt`

**Error**: `lammps-gui` command not found after build
- **Location**: Built executable is at `build/lammps-gui` (not installed by default)
- **To install**: `cmake --install build --prefix ~/.local`

## Development Workflow Best Practices

### Making Code Changes
1. **Always test with offscreen platform** first: `./build/lammps-gui --platform offscreen -v`
2. **Check for compiler warnings**: Build with `-D CMAKE_BUILD_TYPE=Debug` and `-Wall -Wextra`
3. **Format before commit**: Use `clang-format` with project's `.clang-format` config
4. **GPG sign commits**: All git commits must be GPG signed with a verifiable signature
5. **Test with QtGraphs and QtCharts** if possible (QtCharts and QtGraphs use some different code paths)
6. **Update user-facing docs** if changing user-facing features (files in `doc/` directory, especially `basic_usage.rst` and `installation.rst`)
7. **Update programmer's guide** when adding new features, classes, or changing architecture (`doc/introduction.rst`, `doc/api_reference.rst`, and the Programmer's Guide section in `doc/index.rst`)
8. **Add Doxygen comments** for new classes/methods using `/** @brief */` style
9. **Update introduction.rst** when adding new classes or major components
10. **Verify documentation builds**: Run `cmake --build build-doc --target doc` after any doc changes

### Adding New Source Files
1. Add to `PROJECT_SOURCES` list in `CMakeLists.txt` (around line 90)
2. If using Qt signals/slots, ensure `CMAKE_AUTOMOC=ON` is set (line 22)
3. Include file in `#include` directives in relevant source files

### Modifying Resources
- Icons: Add to `resources/icons/` and reference in `resources/lammpsgui.qrc`
- Help text: Update `resources/lammps_internal_commands.txt`
- After changes: Qt resource system auto-compiles via `qt6_add_resources()` (line 131)

### Documentation Updates
- **Format**: reStructuredText (`.rst` files in `doc/`) + Doxygen comments in C++ headers
- **Doxygen**: Add `/** @brief */` style comments to classes and methods in header files
- **API Reference**: Classes documented with Doxygen appear in `doc/api_reference.rst`
- **Architecture**: Update `doc/introduction.rst` when adding new classes or components
- **Spell check**: Run `cmake --build build-doc --target spelling`
- **Preview locally**: Open `build-doc/doc/html/index.html` in browser
- **CI validates**: Every PR builds docs automatically (including Doxygen → Breathe → Sphinx)

### Documentation Requirements When Adding New Features

**Always** update documentation when adding new features, changing behavior, or adding new classes:

1. **User's guide** (`doc/basic_usage.rst` or relevant `.rst` files): Document new UI features, commands, or workflows visible to end users.
2. **Programmer's guide** (`doc/index.rst` Programmer's Guide section and `doc/introduction.rst`): Describe new classes, their responsibilities, and how they fit into the architecture.
3. **API reference** (`doc/api_reference.rst`): Add a `doxygenclass` directive for every new public class with full Doxygen documentation in its header file.
4. **Doxygen comments**: Every new public class and method must have `/** @brief */` style documentation in the header file.
5. **Verify build**: After updating docs, always run `cmake --build build-doc --target doc` to confirm no errors.

### Doxygen Documentation Standards
Use Javadoc-style comments for C++ code documentation:

**Class documentation**:
```cpp
/**
 * @brief Brief one-line description
 *
 * Detailed description paragraph. Can span multiple lines.
 * List features and responsibilities.
 *
 * @see RelatedClass for related functionality
 */
class MyClass {
```

**Method documentation**:
```cpp
/**
 * @brief Brief description of what the method does
 * @param paramName Description of parameter
 * @return Description of return value
 */
void myMethod(int paramName);
```

**Member variables** (inline documentation):
```cpp
QString current_file;  ///< Brief description of member variable
```

**Example**: See `src/lammpsgui.h` for comprehensive Doxygen documentation of the main LammpsGui class with 60+ documented methods.

## Dependency Management

### Python Dependencies (Documentation)
- **File**: `doc/requirements.txt`
- **Version constraints**: Sphinx 6-8.2.3, docutils 0.18-0.23
- **Update frequency**: Weekly (Dependabot monitors)
- **Installation**: Automatic via CMake in virtualenv `build-doc/docenv/`

### GitHub Actions Dependencies
- **File**: Workflow YAML files use `actions/checkout@v6`, `codeql-action/init@v4`
- **Update frequency**: Weekly (Dependabot monitors)

### No Package Manager for C++
- Project has no `package.json`, `requirements.txt` for C++, `Cargo.toml`, etc.
- All C++ dependencies managed via system package manager (apt, brew, etc.)

## Testing & Validation

### Automated Unit Tests
The project includes a test suite using GoogleTest:

- **Test Directory**: `test/` contains test files and CMakeLists.txt
- **Test Framework**: GoogleTest v1.17.0 (fetched automatically via CMake)
- **Current Coverage**:
  - 38 test cases in `test_helpers.cpp` covering (note: `mystrdup` helper was removed from production code and its tests were removed accordingly):
    - Date comparison (`dateCompare`)
    - Line splitting with quote handling (`splitLine`)
    - Executable detection (`hasExe`)
    - Theme detection (`isLightTheme`)
    - Stdout silencing (`silenceStdout`/`restoreStdout`)
    - Directory purging (`purgeDirectory`)
  - 7 test cases in `test_flagwarnings.cpp` covering FlagWarnings syntax highlighter
  - 11 test cases in `test_stdcapture.cpp` covering StdCapture output capture
- **Command-line Tests**: Two CTest tests validate executable behavior:
  - `CommandLine.GetVersion` - Version string validation
  - `CommandLine.HasPlugin` - Build configuration verification
- **GUI Tests**: Python-based tests using PyAutoGUI and Xvfb virtual frame buffer
- **Enable Testing**: Use `-D ENABLE_TESTING=ON` during CMake configuration (OFF by default)

### Running Tests
```bash
# Build with tests enabled
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no -D ENABLE_TESTING=ON
cmake --build build --parallel 2

# Run all tests
ctest --test-dir build/test

# Run with verbose output
ctest --test-dir build/test -V

# Run specific test
ctest --test-dir build/test -R MyStrdup
```

### Manual Testing Checklist
When making changes:
1. Build succeeds on Linux with Qt6 using QtGraphs and QtCharts
2. Application launches: `./build/lammps-gui`
3. Can open/edit LAMMPS input files
4. Syntax highlighting works
5. Can run simulations (requires LAMMPS library in plugin mode)
6. Documentation builds without errors
7. No new compiler warnings
8. Run unit tests if modifying tested components

## Important Notes & Constraints

### ALWAYS Follow These Rules
1. **Never use `git push` or `gh` commands directly** - Use `report_progress` tool to commit/push
2. **Always use absolute paths** starting with `/home/runner/work/lammps-gui/lammps-gui/`
3. **Document build must complete in <300s** - It's a real constraint (virtualenv setup takes time)
4. **Plugin mode works on all platforms** - Including Windows (MSVC and MinGW cross-compile); the earlier restriction was removed in version 2.0.x
5. **C++17 minimum** - Can use C++23 for Qt6 builds, but C++17 for LAMMPS compatibility
6. **Work around deprecated Qt features** - If an API is deprecated in a recent version of Qt, implement a version check and use the deprecated feature only with older versions of Qt and the new API where available
7. **Always update documentation** - When adding new features, always update the user's guide, programmer's guide, and API reference (see Documentation Requirements above)

### Known Issues & Workarounds
- **No TBD/TODO in code**: Only one found at `src/lammpsgui.cpp` (update tutorial URL)
- **No HACK/FIXME comments**: Code is generally clean
- **Install prefix**: Defaults to `~/.local` (not `/usr/local`) for user installs without sudo

### Version Compatibility
- **CMake**: 3.20+ required (check: `cmake --version`)
- **Qt**: 6.2+ required
- **Python**: 3.8+ required (for documentation build)
- **LAMMPS library**: Minimum version "30 March 2026" (see `doc/installation.rst:82`)

## Quick Reference Commands

### Build Application Only (No Docs)
```bash
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no
cmake --build build --parallel 2
./build/lammps-gui --platform offscreen -v
```

### Build Documentation Only
```bash
cmake -S . -B build-doc -D BUILD_DOC_ONLY=yes
cmake --build build-doc --target doc
# Open build-doc/doc/html/index.html
```

### Clean Build
```bash
rm -rf build build-doc
# Then rebuild as needed
```

### Check for Warnings
```bash
cmake -S . -B build -D CMAKE_BUILD_TYPE=Debug \
  -D CMAKE_CXX_FLAGS_DEBUG="-Og -g -Wall -Wextra" \
  -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no
cmake --build build 2>&1 | grep -i "warning:"
```

### Run Tests
```bash
cmake -S . -B build -D LAMMPS_GUI_USE_PLUGIN=yes -D BUILD_DOC=no -D ENABLE_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build/test -V
```
## Code Review

When performing a code review, check any changes to the documentation
(in the `doc/` folder) to be written in American English and with plain
ASCII characters.

## Programmer's Guide and API Documentation

The project now includes comprehensive developer documentation:

- **Programmer's Guide**: `doc/index.rst` includes a dedicated section for developers
- **API Reference**: `doc/api_reference.rst` - Doxygen-generated class documentation
- **Architecture**: `doc/introduction.rst` - Architecture overview of all classes
- **Testing Guide**: `doc/testing.rst` - Test infrastructure and examples

**Note**: The initial version of the Programmer's Guide was created by a GitHub Copilot
Coding Agent. While comprehensive, not everything has been carefully verified yet. If you
spot errors or inconsistencies in the architecture or API documentation, please submit
a bug report.

## Trust These Instructions

These instructions have been thoroughly researched by examining:
- All workflow files, CMakeLists.txt, build scripts
- Documentation (README, installation.rst, TODO.md)
- Source code structure and build patterns
- Actual successful build of documentation
- Test infrastructure and examples

**Only search or explore further if**:
- These instructions are incomplete for your specific task
- You encounter errors not covered here
- The information appears outdated or incorrect
- You need details about specific source files not covered

For implementation details of specific features, refer to the source files in `src/`. For user-facing behavior, check the documentation in `doc/`. For build system internals, study `CMakeLists.txt` (well-commented, 480 lines). For API documentation, see `doc/api_reference.rst`.
