# RangeSlider (vendored third-party widget)

`rangeslider.{cpp,h}` is a Qt widget providing a horizontal slider with two
handles (a low and a high limit), used by LAMMPS-GUI's chart window to select
the visible x and y data ranges.

## Origin and license

Copyright Hoyoung Lee (hoyoung.yi@gmail.com). This widget is **not** covered by
the GPLv2+ license of the rest of LAMMPS-GUI: it is distributed under the
**CeCILL-A** license (CEA/CNRS/INRIA, French law), which is GPL-compatible. The
full license notice is reproduced at the top of `rangeslider.h` and
`rangeslider.cpp`; the canonical license text is at http://www.cecill.info.

## Build integration

The two files are listed in `PROJECT_SOURCES` in the top-level `CMakeLists.txt`
and compiled directly into the `lammps-gui` executable (so Qt's AUTOMOC handles
the `Q_OBJECT` widget); `thirdparty/rangeslider` is added to the target include
path so `#include "rangeslider.h"` resolves. The directory is also on the
Doxygen `INPUT` path so the `RangeSlider` class stays in the API reference.
