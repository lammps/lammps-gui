# LAMMPS-GUI Instructions for GitHub Copilot

The instructions for coding agents are in `AGENTS.md` in the top-level folder: read
that file first.  It describes the project, the build and test commands, the code
conventions, and the architecture, and it is updated together with the code.  The
rules for contributors are in `doc/guidelines.rst` (the "Development Guidelines" page
of the manual).  This file only adds what applies to Copilot.

## Code Review

When reviewing a pull request, check that:

- C++ changes follow the `.clang-format` settings of the project (4 spaces, no tabs,
  at most 100 columns) and compile without new warnings with `-Wall -Wextra`.
- New public classes and functions have Doxygen comments (`/** @brief ... */`, member
  variables with a trailing `///<` comment), and a new public class has a
  `.. doxygenclass::` entry in `doc/api_reference.rst`.
- New `.cpp` and `.h` files start with the `// -*- c++ -*-` mode line and are listed
  in `PROJECT_SOURCES` in `cmake/Sources.cmake`.
- Internal interfaces use `QString`; conversions to `char *` or `std::string` for the
  LAMMPS library interface stay inside `LammpsWrapper`.
- New hard-coded values and repeated string literals go into the `Cfg` namespace and
  new settings keys into the `Keys` namespace in `src/constants.h`.
- The code works with Qt 6.2, the oldest supported version.  A Qt function that is
  deprecated in a newer Qt version is used only inside a `QT_VERSION` check, with its
  replacement for the newer versions.
- Added or changed features are documented in the User's Guide and in the
  Programmer's Guide (`doc/*.rst`), in American English and with plain ASCII
  characters.
- Changes to code that has unit tests in `test/` come with updated tests.
- Commit messages contain no AI attribution lines (`Co-Authored-By:` and similar).
  The use of AI tools is described in the text of the pull request instead.

## Copilot coding agent

- The environment is set up by `.github/workflows/copilot-setup-steps.yml`.
- Never run `git push` or `gh` commands directly: use the `report_progress` tool to
  commit and push.
- Use absolute paths below `/home/runner/work/lammps-gui/lammps-gui/`.
- Configure with `-D BUILD_DOC=OFF` unless the task is about the documentation.  A
  documentation build creates a Python virtual environment and installs Sphinx into
  it, and it has to finish within 300 seconds.
- A check that needs no display: `build/lammps-gui --platform offscreen -v` prints
  the version.
