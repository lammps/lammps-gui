#!/bin/bash

APP_NAME=lammps-gui
VERSION="$1"
BUILD_DIR="${PWD}"
PACKAGING_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="${BUILD_DIR}/dmg-staging"
DMG_FILE="LAMMPS-GUI-macOS-multiarch-v${VERSION}.dmg"
PYTHON="${PYTHON:-python3}"

# dmgbuild creates the disk image and its Finder window layout without
# scripting the Finder, so it also works over ssh or without a GUI session.
# Install it for the current user, if the selected Python cannot import it.
if ! "${PYTHON}" -c 'import dmgbuild' > /dev/null 2>&1
then
    "${PYTHON}" -m pip install --user dmgbuild
fi
if ! "${PYTHON}" -c 'import dmgbuild' > /dev/null 2>&1
then
    echo "ERROR: dmgbuild is required. Install with: ${PYTHON} -m pip install --user dmgbuild"
    exit 1
fi

# Run dmgbuild. Versions before 1.6.7 (the last one available for Python 3.9 is
# 1.6.5) also store the background image location as a bookmark, which keeps
# the Finder on macOS 26.2 and later from showing the background. dmgbuild
# 1.6.7 dropped the bookmark; do the same for older versions.
run_dmgbuild()
{
    "${PYTHON}" - "$@" << 'PYEOF'
import sys
from importlib.metadata import version
import dmgbuild.core
from dmgbuild.__main__ import main

try:
    old = tuple(int(v) for v in version("dmgbuild").split(".")[:3]) < (1, 6, 7)
except ValueError:
    old = False
if old:
    class NoBookmark:
        @staticmethod
        def for_file(path):
            return None
    dmgbuild.core.Bookmark = NoBookmark
sys.exit(main())
PYEOF
}

echo "Delete old files, if they exist"
rm -f ${APP_NAME}.dmg ${APP_NAME}-rw.dmg LAMMPS-GUI-macOS-multiarch*.dmg \
   ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib
rm -rf "${STAGE_DIR}"

# download pre-compiled LAMMPS shared library if plugin-mode LAMMPS-GUI binary
if $(./${APP_NAME}.app/Contents/MacOS/lammps-gui -h | grep -q pluginpath); then
    mkdir -p ${APP_NAME}.app/Contents/Frameworks
    curl -L -o ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib https://download.lammps.org/lammps-gui/liblammps.0.dylib
    chmod 0755 ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib
fi

echo "Bundle Qt frameworks and plugins with macdeployqt"
macdeployqt ${APP_NAME}.app

echo "Stage a copy of the app bundle and the background image"
mkdir -p "${STAGE_DIR}"
ditto ${APP_NAME}.app "${STAGE_DIR}/LAMMPS-GUI.app"
pushd "${STAGE_DIR}"
mv LAMMPS-GUI.app/Contents/Resources/LAMMPS_DMG_Background.png background.png
cd LAMMPS-GUI.app/Contents
echo "Codesign bundled plugins"
codesign --force -s - PlugIns/*/*.dylib
echo "Codesign bundled frameworks"
codesign --force -s - Frameworks/Qt*.framework/Versions/A/Qt*
echo "Codesign LAMMPS-GUI executable"
codesign --force -s - MacOS/lammps-gui

echo "Attach LAMMPS-GUI icon to executable and LAMMPS icon to LAMMPS lib"
echo "read 'icns' (-16455) \"Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o MacOS/lammps-gui
SetFile -a C MacOS/lammps-gui
if [ -f Frameworks/liblammps.0.dylib ]; then
    echo "read 'icns' (-16455) \"${PACKAGING_DIR}/lammps.icns\";" > icon.rsrc
    Rez -a icon.rsrc -o Frameworks/liblammps.0.dylib
    SetFile -a C Frameworks/liblammps.0.dylib
fi
rm icon.rsrc
popd

echo "Create compressed disk image using dmgbuild"
run_dmgbuild -s "${PACKAGING_DIR}/dmg_settings.py" \
    -D app="${STAGE_DIR}/LAMMPS-GUI.app" \
    -D background="${STAGE_DIR}/background.png" \
    -D icon="${BUILD_DIR}/${APP_NAME}.app/Contents/Resources/lammps-gui.icns" \
    "${APP_NAME}" "${DMG_FILE}"

echo "Attach icon to .dmg file"
echo "read 'icns' (-16455) \"${APP_NAME}.app/Contents/Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o ${DMG_FILE}
SetFile -a C ${DMG_FILE}
rm icon.rsrc

echo "Delete staging directory"
rm -rf "${STAGE_DIR}"

exit 0
