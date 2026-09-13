#!/bin/bash

APP_NAME=lammps-gui
VERSION="$1"

echo "Delete old files, if they exist"
rm -f ${APP_NAME}.dmg ${APP_NAME}-rw.dmg LAMMPS-GUI-macOS-multiarch*.dmg \
   ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib

# download pre-compiled LAMMPS shared library if plugin-mode LAMMPS-GUI binary
if $(./${APP_NAME}.app/Contents/MacOS/lammps-gui -h | grep -q pluginpath); then
    mkdir -p ${APP_NAME}.app/Contents/Frameworks
    curl -L -o ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib https://download.lammps.org/lammps-gui/liblammps.0.dylib
    chmod 0755 ${APP_NAME}.app/Contents/Frameworks/liblammps.0.dylib
fi

echo "Create initial dmg file with macdeployqt"
macdeployqt ${APP_NAME}.app -dmg
echo "Create writable dmg file"
hdiutil convert ${APP_NAME}.dmg -format UDRW -o ${APP_NAME}-rw.dmg

echo "Mount writeable DMG file in read-write mode. Keep track of device and volume names"
DEVICE=$(hdiutil attach -readwrite -noverify ${APP_NAME}-rw.dmg | grep '^/dev/' | sed 1q | awk '{print $1}')
VOLUME=$(df | grep ${DEVICE} | sed -e 's/^.*\(\/Volumes\/\)/\1/')
sleep 2

echo "Create link to Application folder and move README and background image files"

pushd "${VOLUME}"
ln -s /Applications .
mv ${APP_NAME}.app/Contents/Resources/README.txt .
mkdir  .background
mv ${APP_NAME}.app/Contents/Resources/LAMMPS_DMG_Background.png .background/background.png
mv ${APP_NAME}.app LAMMPS-GUI.app
cd LAMMPS-GUI.app/Contents
echo "Codesign bundled plugins"
codesign --force -s - PlugIns/*/*.dylib

echo "Attach icons to LAMMPS console and GUI executables and lib"
echo "read 'icns' (-16455) \"Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o MacOS/lammps-gui
SetFile -a C MacOS/lammps-gui
if [ -f Frameworks/liblammps.0.dylib ]; then
    Rez -a icon.rsrc -o Frameworks/liblammps.0.dylib
    SetFile -a C Frameworks/liblammps.0.dylib
fi
rm icon.rsrc
popd

echo 'Tell the Finder to resize the window, set the background,'
echo 'change the icon size, place the icons in the right position, etc.'
echo '
    tell application "Finder"
    tell disk "'${APP_NAME}'"

      -- wait for the image to finish mounting
      set open_attempts to 0
      repeat while open_attempts < 4
        try
          open
            delay 1
            set open_attempts to 5
          close
        on error errStr number errorNumber
          set open_attempts to open_attempts + 1
          delay 10
        end try
      end repeat
      delay 5

      -- open the image the first time and save a .DS_Store
      -- just the background and icon setup
      open
        set current view of container window to icon view
        set theViewOptions to the icon view options of container window
        set background picture of theViewOptions to file ".background:background.png"
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 64
        delay 5
      close

      -- next set up the position of the app and Applications symlink
      -- plus hide all window decorations
      open
        update without registering applications
        tell container window
          set sidebar width to 0
          set statusbar visible to false
          set toolbar visible to false
          set the bounds to { 100, 40, 868, 640 }
          set position of item "'LAMMPS-GUI'.app" to { 190, 216 }
          set position of item "Applications" to { 576, 216 }
          set position of item "README.txt" to { 190, 400 }
        end tell
        update without registering applications
        delay 5
      close

      -- one last open and close to check the results
      open
        delay 5
      close
    end tell
    delay 1
  end tell
' | osascript

sync

echo "Unmount modified disk image and convert to compressed read-only image"
hdiutil detach "${DEVICE}"
hdiutil convert "${APP_NAME}-rw.dmg" -format UDZO -o "LAMMPS-GUI-macOS-multiarch-v${VERSION}.dmg"

echo "Attach icon to .dmg file"
echo "read 'icns' (-16455) \"lammps-gui.app/Contents/Resources/lammps-gui.icns\";" > icon.rsrc
Rez -a icon.rsrc -o LAMMPS-GUI-macOS-multiarch-v${VERSION}.dmg
SetFile -a C LAMMPS-GUI-macOS-multiarch-v${VERSION}.dmg
rm icon.rsrc

echo "Delete temporary disk images"
rm -f "${APP_NAME}-rw.dmg"
rm -f "${APP_NAME}.dmg"

exit 0
