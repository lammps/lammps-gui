# dmgbuild settings for the LAMMPS-GUI drag-and-drop installer disk image.
#
# Used by build_macos_dmg.sh.
# dmgbuild writes the window layout directly into the image's .DS_Store.
#
# Paths are passed from the build script with -D:
#   app=...         staged LAMMPS-GUI.app bundle
#   background=...  background image (679x374 px at 96 dpi = 509x281 pt)
#   icon=...        optional .icns file used as volume icon

import os.path

for _key in ("app", "background"):
    if _key not in defines:
        raise ValueError(f"dmg_settings.py: missing -D {_key}=<path>")

_app = defines["app"]

# compressed read-only image, same as the former 'hdiutil convert -format UDZO'
format = "UDZO"
filesystem = "HFS+"

files = [_app]
symlinks = {"Applications": "/Applications"}
background = defines["background"]

# volume icon: stored inside the image, so unlike the custom icon attached
# to the .dmg file it is kept when the image is downloaded
icon = defines.get("icon")

# window width matches the background; the height adds ~24pt for the title bar
window_rect = ((100, 100), (510, 305))
default_view = "icon-view"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
sidebar_width = 0

# Icon view: free placement, 64pt icons, Finder's default 12pt labels
arrange_by = None
icon_size = 64
text_size = 12
show_icon_preview = False

# Icon centers left and right of the arrow in the background image.
# All icons must lie fully inside the window, or Finder adds a scrollbar
# and shifts them.
icon_locations = {
    os.path.basename(_app.rstrip("/")): (95, 216),
    "Applications": (420, 216),
}
