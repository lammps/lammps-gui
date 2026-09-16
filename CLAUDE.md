# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LAMMPS-GUI (v3.x) is a Qt6-based graphical interface for the LAMMPS molecular dynamics simulation software. It provides a code editor with syntax highlighting and auto-completion, live LAMMPS simulation execution, log/chart/image visualization, and an integrated tutorial system. The project is GPLv2+ licensed (note: `thirdparty/rangeslider/rangeslider.{cpp,h}` is third-party under the CeCILL-A license).

- Online documentation: https://lammps-gui.lammps.org/
- C++17, CMake ≥ 3.20, Qt6 (minimum 6.2; only the Gui, Widgets, Network, and Svg modules — charts are rendered natively, so no Qt Charts/Graphs/Quick)

## Build Commands

**Typical plugin-mode build** (LAMMPS library loaded dynamically at runtime — the default):
```bash
cmake -S . -B build -DLAMMPS_GUI_USE_PLUGIN=ON -DBUILD_DOC=OFF
cmake --build build -j$(nproc)
```

**Linked mode** (requires LAMMPS source tree and pre-built library):
```bash
cmake -S . -B build \
  -DLAMMPS_GUI_USE_PLUGIN=OFF \
  -DLAMMPS_SOURCE_DIR=/path/to/lammps/src \
  -DLAMMPS_LIBRARY=/path/to/liblammps.so
cmake --build build -j$(nproc)
```

Default install prefix is `$HOME/.local` (no root required).

**Documentation-only build** (no C++ compilation needed):
```bash
cmake -S . -B build-doc -DBUILD_DOC_ONLY=ON
cmake --build build-doc --target doc
# Output: build-doc/doc/html/index.html
```

## Testing

Tests are Linux-only and off by default. Enable them at configure time:
```bash
cmake -S . -B build -DLAMMPS_GUI_USE_PLUGIN=ON -DBUILD_DOC=OFF -DENABLE_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run a subset by name pattern (tests are registered under their GoogleTest suite
names, e.g. `HelpersTest.*`, not the executable names):
```bash
ctest --test-dir build -R HelpersTest --output-on-failure
ctest --test-dir build -R Framebuffer --output-on-failure   # GUI tests need Xvfb + screenshooter
```

**Test categories:**
- `test_*` executables — C++ unit tests (GoogleTest, fetched via FetchContent), one per tested module
- `CommandLine.*` — command-line flag smoke tests
- `Framebuffer.*` — Python/PyAutoGUI GUI tests run inside Xvfb; require `xvfb-run` and one of: `magick`, `import`, `xfce4-screenshooter`, or `gnome-screenshot`

## Code Style

All C++ source is formatted with **clang-format** using the config in `.clang-format` (LLVM base, 4-space indent, 100-column limit, custom brace wrapping). Before committing:
```bash
clang-format -i src/*.cpp src/*.h
```

File headers use `// -*- c++ -*-` Emacs mode line; maintain it on new files.

## Commit & Code Conventions

- **GPG-sign all commits.** Every commit must carry a verifiable GPG signature.
- **No `Co-Authored-By:` or `Claude-Session:` attribution in commit messages.** AI attribution belongs in pull request descriptions only.
- **Doxygen comments on all new public APIs.** Use `/** @brief ... */` Javadoc style for classes and methods; `///< description` for member variables. See `src/lammpsgui.h` for a comprehensive example.
- **New public classes need a `.. doxygenclass::` entry in `doc/api_reference.rst`.** The `helpers.h` block uses `.. doxygenfile:: helpers.h :sections: func`, which renders only *free functions*, so a new helper class (e.g. an RAII guard) is otherwise missing from the generated API docs.
- **Documentation changes in American English with plain ASCII characters** (no typographic quotes, em-dashes as `--`, etc.).
- **New `.cpp`/`.h` files must be added to `PROJECT_SOURCES`** in `cmake/Sources.cmake`. Qt's `AUTOMOC` handles `moc` generation automatically, but the file must be listed there. The top-level `CMakeLists.txt` holds only the configuration options and the executable target; the remaining build logic lives in include files under `cmake/` (Platform, Sources, Testing, Sanitizer, Documentation, Packaging).

## Architecture

### Core component relationships

```
main.cpp
  └─ LammpsGui (QMainWindow)           ← central coordinator
       ├─ CodeEditor (QPlainTextEdit)  ← input script editor
       │    ├─ Highlighter             ← LAMMPS syntax highlighting
       │    ├─ FindAndReplace          ← non-modal find/replace dialog
       │    └─ QCompleter × N         ← per-command-type auto-complete
       ├─ LammpsWrapper               ← thin C++ wrapper around LAMMPS C API
       ├─ LammpsRunner (QThread)       ← runs LAMMPS in background thread
       ├─ StdCapture                   ← redirects stdout→pipe to capture LAMMPS output
       ├─ LogWindow (QPlainTextEdit)   ← displays captured log; uses FlagWarnings highlighter
       ├─ ImageViewer (QDialog)        ← interactive dump-image viewer
       ├─ SlideShow (QDialog)          ← slideshow viewer for image sequences
       ├─ TutorialWizard (QWizard)     ← step-by-step tutorial setup wizard
       ├─ CommandWindow                ← shell prompt + scrollback; ShellPrompt input line, ShellAliases
       ├─ WindowLayout                 ← mediator: individual windows vs. docked panels in the main window
       ├─ ChartWindow                  ← thermo chart container; owns N ChartColumn data objects
       │    └─ ChartViewer             ← single rebindable view of the active ChartColumn
       │         └─ PlotWidget         ← QPainter 2D line/scatter renderer (sole chart backend)
       ├─ PlotDataDialog               ← column picker for plotted files; PlotBlockData parses fix ave/* blocks
       └─ Preferences (QDialog)        ← settings; stored via QSettings
```

### Key design points

**Plugin vs. linked mode.** When built with `LAMMPS_GUI_USE_PLUGIN=ON` (default), the executable has no link-time dependency on LAMMPS. `plugin/liblammpsplugin.c` provides `dlopen`-based dispatch; `LammpsWrapper` calls through function pointers loaded at startup. This lets the GUI ship as a standalone binary that can download or swap LAMMPS shared libraries.

**Native chart rendering.** Charts are drawn by a single self-contained renderer, `PlotWidget` (`src/plotwidget.{cpp,h}`), a `QWidget`+`QPainter` 2D line/scatter plotter that depends only on Qt Widgets — no Qt Charts, Qt Graphs, or QML. `ChartWindow` owns one `ChartColumn` per thermo column — the neutral `PlotSeries` data objects (`src/plotseries.h`) live there — plus a *single* `ChartViewer` that is rebound (via `setColumn()`) to whichever column is selected and renders it through `PlotWidget`; axis-layout math (nice ticks, label formatting) lives in the Qt-free `plotaxismath` (`src/plotaxismath.{cpp,h}`). Both the old one-`ChartViewer`-per-column layout and the `ChartBackend`/QtCharts/QtGraphs abstraction were removed once the native single-view renderer reached parity.

**Threading model.** LAMMPS simulations run on a `LammpsRunner` (QThread). `StdCapture` intercepts the LAMMPS library's stdout by replacing the file descriptor before `LammpsRunner::run()` starts. A `QTimer` in `LammpsGui` polls `StdCapture::getChunk()` to feed `LogWindow` without blocking the UI thread.

**Auto-completion.** `CodeEditor` maintains a separate `QCompleter` instance for each LAMMPS command category (fix styles, compute styles, pair styles, etc.). Completions are populated from style lists queried from `LammpsWrapper` after LAMMPS is initialized, plus static tables embedded as Qt resources.

**Resources.** `resources/lammpsgui.qrc` embeds icons, `help_index.table` (maps LAMMPS commands to doc URLs), `image_style.table` (dump image options), and `lammps_internal_commands.txt`. The `.table` files are plain text and have companion shell scripts (`update-help-index.sh`, `update-image-styles.sh`) to regenerate them from a LAMMPS source tree.

**Constants and settings keys.** `src/constants.h` holds two intentionally short, internal namespaces: `Cfg` (application-wide magic numbers and repeated string literals -- UI dimensions, update intervals, resource paths, version constants) and `Keys` (every persisted QSettings key and group name). New hardcoded values and any new QSettings key go there. Reference them qualified -- `Cfg::PREFERENCES_WIDTH`, `Keys::ZOOM` -- never via `using namespace`/aliases: the namespaces are deliberately terse so the qualifier stays cheap, and the `Keys::` prefix keeps the generic key names (e.g. `NAME`, `TYPE`, `ID`) readable and collision-free. A mistyped settings key is then a compile error, not a silently lost setting.

**Minimum LAMMPS version.** `Cfg::MIN_LAMMPS_VERSION` (see `src/constants.h` for the current value) is enforced at startup; the GUI warns and may refuse to run with older LAMMPS builds.

**Dialog widget wiring.** `ImageViewer` and the `Preferences` tabs connect widgets to slots via `setObjectName("...")` + later `findChild<T>("...")` rather than stored member pointers. Preserve object names exactly when refactoring these dialogs (a wrong/renamed name fails the lookup silently, with no compile error).

**Shared helpers (prefer over re-rolling).** Use the `StdoutSilencer` RAII guard (`helpers.h`) instead of manual `silenceStdout()`/`restoreStdout()` pairs; the `QtMessageSilencer` RAII guard (`helpers.h`) around a call whose Qt-internal warnings are expected and handled (note it cannot catch messages a library prints straight to stderr, such as libpng's `libpng error:` lines); `LammpsWrapper::lastErrorMessage()` instead of a hand-managed `getLastErrorMessage()` buffer; `LammpsGui::addMenuAction()` to build menu actions; `monoFontFromSettings()` for the configured fixed-width font; `styleDialogButtons()` to apply the bundled SVG icons to a `QDialogButtonBox`; `toolButtonSize()`/`styleToolButtons()` for square toolbar buttons; `applyWindowFlags()` for the shared output-window WM hints; `installViewMenuBar()`/`layoutViewMenuBar()` for an output view's own menu bar (they retire it via `retireViewMenuBar()` in the docked layout: on macOS a `QMenuBar` is a handle on the system-wide bar, so a hidden one left native inside the main window blanks the real menu bar -- hiding it is not enough); `styleMessageBoxButtons()` for a `QMessageBox`, sharing the icon table of `styleDialogButtons()`; `relaunchOrExit()` wherever a settings change needs a re-exec; the `chartstyle.h` palette and widget builders for anything that offers a series color, display mode, line width, or point size; `PlotDataDialog::fromFile()` to open a data file for plotting; inside `LammpsGui`, `abortRun()`, `closeOutputWindows()`, `closeLammpsInstance()`, `beginRunStatus()`, and `currentStep()` instead of their inlined bodies.

### String handling & modern C++ conventions

These are the settled conventions for new and refactored code; the staged
cleanup that brought the existing code into line is complete.

**QString is the canonical internal string type.** It already dominates
(~350 declarations vs. ~25 `std::string`). Keep `char *` and `std::string`
out of internal interfaces; pass and return `QString`.

**Confine all string conversions to `LammpsWrapper`** (the LAMMPS C API is
the only place `char *` is unavoidable). Do not sprinkle `toStdString()` /
`.c_str()` / `char buf[N]` at call sites. Two patterns already in the
wrapper are the templates to copy:
- *Input:* use either `const char *` or `const QString &`
  as `extractSetting()`, `extractGlobal()` or `command()`, `file()`.
  Do the former when only string constants are used as arguments.
- *Output:* return a `QString` and manage the buffer internally, as
  `lastErrorMessage()`, `idName()`, `styleName()`, and `variableInfo()` do
  (their `char *`-buffer variants are private implementation details behind
  the QString-returning public API).

Avoid `QString -> std::string -> QString` round-trips. `splitLine` now
parses the `QString` directly (via `utf16()`) and returns a `QStringList`;
the `toStdString()` calls that remain sit at genuine boundaries to
std::string-only subsystems (`LeptonMini`, `plotaxismath`, the LAMMPS
runner) rather than being gratuitous conversions.

**Match the existing modern-C++ baseline.** This code already uses
`nullptr`, `auto`, range-based `for`, `override`, `constexpr`, `= default`,
and an explicit Rule-of-5 (`= delete` / `= default` for all five special
members) on essentially every class; mirror that on new classes. Prefer
`std::make_unique` and smart pointers for owned non-QObject resources
(QObject parent/child ownership via `new` with a parent is still the Qt
idiom and is fine). Use `static_cast` rather than C-style casts, the
function-pointer `connect()` form (never `SIGNAL()`/`SLOT()` strings), and
`enum class` for new internal enumerations that do not need implicit `int`
interop with the LAMMPS API.

## AI-assistant feature (exploratory — temporary feature branch)

We are adding an AI assistant to the GUI frontend of this physics-simulation
software. Work is exploratory: build a **minimal** implementation first to probe
workflow options, then decide a fuller architecture and implement interactively.

**Before working on this feature, read `doc/ai-assistant-design.md`** — it is
the durable design memory (provider abstraction, RAG, reliability via
verification, tool-calling file generation, the wizard/expert-system model, the
probe verify-repair loop, and the case-based learning approach). Treat its
decisions and caveats as binding unless we explicitly revise them here.

### Non-negotiables for this feature

- The assistant produces a **starting point, not a validated solution.**
- Structural correctness comes from **vetted templates** and **executable
  verification (the simulator as oracle)** — never from the model's unaided
  judgment.
- **Treat all model-generated files as untrusted**; validate before loading.
- **Never hardcode or commit API keys.**
- Prefer **deterministic checks/lookup tables** for known cases; use the LLM for
  the fuzzy long tail and explanation.

### Source layout notes (non-obvious only)

`src/` is one `.cpp/.h` pair per widget/module with self-describing names; read
the file headers for specifics. What the names alone don't tell you:

- Qt-free modules (unit-testable without Qt): `plotaxismath`, `analysis`,
  `leastsquares`, `fitting`, `levmar`; `thirdparty/lepton_mini/` is a vendored
  JIT-less Lepton subset (namespace `LeptonMini`, MIT), built as the
  `lepton_mini` static library.
- `src/rangebandslider.{cpp,h}` (in-tree) is distinct from the third-party
  dual-handle `thirdparty/rangeslider/` (CeCILL-A license).
- `src/imageviewersettings.cpp` holds `ImageViewer`'s dialog builders (split out
  to keep `imageviewer.cpp` manageable); shared impl-detail symbols live in
  `src/imageviewer_internal.h`.  Likewise `src/plotdata_internal.h` declares
  the field/line scanners and helpers shared by `plotdata.cpp` and
  `plotblockdata.cpp`; both parsers walk the text with `QStringView` rather
  than splitting it into lists (a large file's cost was those allocations).
- `src/chartstyle.{cpp,h}` is the one home of the preset series palette and
  of the mode/color/width/size widget builders; `chartviewer.cpp` and
  `preferences.cpp` must not grow lists of their own again.
- `CommandWindow` is not a terminal emulator: no PTY, `TERM=dumb`, one
  persistent `$SHELL`/`%COMSPEC%` process, cwd tracked via a sentinel;
  `ShellAliases` restores aliases lost to non-interactive shells.
