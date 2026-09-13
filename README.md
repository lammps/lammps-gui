# LAMMPS-GUI - The graphical interface for learning and running LAMMPS

Please see the online documentation at https://lammps-gui.lammps.org/

[![status](https://joss.theoj.org/papers/59eed23e3cdee45c6585356fb7c23ca8/status.svg)](https://joss.theoj.org/papers/59eed23e3cdee45c6585356fb7c23ca8)


## Test Status of the development branch

[![Compile and run tests on Linux with Qt 6.x](https://github.com/lammps/lammps-gui/actions/workflows/compile-linux-qt6.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/compile-linux-qt6.yml)
[![Build LAMMPS-GUI as flatpak bundle](https://github.com/lammps/lammps-gui/actions/workflows/build-linux-flatpak.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/build-linux-flatpak.yml)
[![Compile on macOS and build DMG](https://github.com/lammps/lammps-gui/actions/workflows/compile-macos-dmg.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/compile-macos-dmg.yml)
[![Cross-compile for Windows with MinGW64](https://github.com/lammps/lammps-gui/actions/workflows/compile-mingw64-cross.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/compile-mingw64-cross.yml)
[![Compile on Windows with MSVC](https://github.com/lammps/lammps-gui/actions/workflows/compile-windows-msvc.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/compile-windows-msvc.yml)
[![CodeQL Code Analysis](https://github.com/lammps/lammps-gui/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/codeql-analysis.yml)
[![Build Documentation in HTML](https://github.com/lammps/lammps-gui/actions/workflows/build-html-docs.yml/badge.svg)](https://github.com/lammps/lammps-gui/actions/workflows/build-html-docs.yml)

## LAMMPS-GUI vs. LAMMPS

LAMMPS-GUI used to "live" in the "tools/lammps-gui" folder of the LAMMPS source distribution
and the LAMMPS git repository.  This made it easy to build LAMMPS-GUI together with LAMMPS.
However, LAMMPS-GUI has matured to the point, that it is easier to maintain it as a separate
package and make its release schedule independent from LAMMPS releases.  It is still possible
to build LAMMPS-GUI as part of a LAMMPS build same as before, but that will automatically
first download the LAMMPS-GUI sources from this repository.

## Citation

There is currently no citation specifically describing LAMMPS-GUI, but an introduction
is contained in the publication in LiveCoMS for the LAMMPS Tutorials that are linked
from LAMMPS-GUI, so the suggestion is to cite that publication for now:

```
@article{lammps_tutorials_2025,
  author={Gravelle, Simon and Alvares, Cecilia M. S. and Gissinger, Jacob R. and Kohlmeyer, Axel},
  title={A Set of Tutorials for the {LAMMPS} Simulation Package [Article v1.0]},
  journal={Living Journal of Computational Molecular Science},
  pages={3027},
  volume={6},
  number={1},
  year={2025},
  month={Sep.},
  url={https://livecomsjournal.org/index.php/livecoms/article/view/v6i1e3037},
  DOI={10.33011/livecoms.6.1.3037}
}
```

## License

LAMMPS-GUI is distributed under the GNU public license version 2 or later (GPLv2+).
