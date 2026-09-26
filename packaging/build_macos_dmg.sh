#!/bin/bash

APP_NAME=lammps-gui
VERSION="$1"
BUILD_DIR="${PWD}"
PACKAGING_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="${BUILD_DIR}/dmg-staging"
DMG_FILE="LAMMPS-GUI-macOS-multiarch-v${VERSION}.dmg"

# install/upgrade dmgbuild helper script
python3 -m pip install --upgrade --user pip
python3 -m pip install --upgrade --user dmgbuild

if ! python3 -c 'import dmgbuild' > /dev/null 2>&1
then
    echo "ERROR: dmgbuild is required. Install with: python3 -m pip install --user dmgbuild"
    exit 1
fi

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

echo "Stage a copy of the app bundle plus README and background image"
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

echo "Attach icons to LAMMPS-GUI executable and LAMMPS lib"
echo "read 'icns' (-16455) \"Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o MacOS/lammps-gui
SetFile -a C MacOS/lammps-gui
if [ -f Frameworks/liblammps.0.dylib ]; then
    Rez -a icon.rsrc -o Frameworks/liblammps.0.dylib
    SetFile -a C Frameworks/liblammps.0.dylib
fi
rm icon.rsrc
popd

echo "Create compressed disk image using dmgbuild"
python3 -m dmgbuild -s "${PACKAGING_DIR}/dmg_settings.py" \
    -D app="${STAGE_DIR}/LAMMPS-GUI.app" \
    -D readme="${STAGE_DIR}/README.txt" \
    -D background="${STAGE_DIR}/background.png" \
    "${APP_NAME}" "${DMG_FILE}"

echo "Attach icon to .dmg file"
echo "read 'icns' (-16455) \"lammps-gui.app/Contents/Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o ${DMG_FILE}
SetFile -a C ${DMG_FILE}
rm icon.rsrc

echo "Delete staging directory"
rm -rf "${STAGE_DIR}"

exit 0
