.. _shortcuts:

******************
Keyboard Shortcuts
******************

.. index:: keyboard shortcuts
.. index:: hotkeys
.. index:: keybindings

Almost all functionality is accessible from the menu of the editor
window or through keyboard shortcuts.  The following shortcuts are
available (On macOS use the Command key instead of Ctrl/Control).

.. list-table::
   :header-rows: 1
   :widths: 16 19 13 16 13 22

   * - Shortcut
     - Function
     - Shortcut
     - Function
     - Shortcut
     - Function
   * - Ctrl+N
     - New File
     - Ctrl+Z
     - Undo edit
     - Ctrl+Enter
     - Run Input
   * - Ctrl+O
     - Open File
     - Ctrl+Shift+Z
     - Redo edit
     - Ctrl+/
     - Stop Active Run
   * - Ctrl+Shift+F
     - View Text File
     - Ctrl+C
     - Copy text
     - Ctrl+Shift+V
     - Set Variables
   * - Ctrl+S
     - Save File
     - Ctrl+X
     - Cut text
     - Ctrl+I
     - Snapshot Image
   * - Ctrl+Shift+S
     - Save File As
     - Ctrl+V
     - Paste text
     - Ctrl+L
     - Slide Show
   * - Ctrl+Q
     - Quit Application
     - Ctrl+A
     - Select All
     - Ctrl+F
     - Find and Replace
   * - Ctrl+W
     - Close Window
     - TAB
     - Reformat line
     - Shift+TAB
     - Show Completions
   * - Ctrl+Shift+Enter
     - Run File
     - Ctrl+Shift+W
     - Show Variables
     - Ctrl+P
     - Preferences
   * - Ctrl+Shift+A
     - About GUI
     - Ctrl+Shift+H
     - Quick Help
     - Ctrl+Shift+G
     - LAMMPS-GUI Docs
   * - Ctrl+Shift+R
     - Inspect Restart File
     - Ctrl+Shift+O
     - View in OVITO
     - Ctrl+Shift+D
     - View in VMD
   * - Ctrl+Shift+L
     - Output Window
     - Ctrl+Shift+C
     - Charts Window
     - Ctrl+Shift+I
     - Image Window
   * - Ctrl+Shift+M
     - LAMMPS Manual
     - Ctrl+?
     - Context Help
     - Ctrl+Shift+T
     - LAMMPS Tutorial
   * - Ctrl+Shift+U
     - Update LAMMPS
     - Ctrl+Home
     - Go to Home
     - Ctrl+End
     - Go to End
   * - Ctrl+Shift+J
     - View Image or Movie File(s)
     - Ctrl+Shift+P
     - Plot Data File
     - Ctrl+E
     - Extend Run
   * - Ctrl+K
     - Check Input via Heuristics
     - Ctrl+Shift+K
     - Check Input via Dry Run
     - Ctrl+Shift+X
     - Command Window

Further keybindings of the editor window `are documented with the Qt
documentation
<https://doc.qt.io/qt-6/qplaintextedit.html#editing-key-bindings>`_.  In
case of conflicts the list above takes precedence.

All other windows only support a subset of keyboard shortcuts listed
above.  Typically, the shortcuts `Ctrl-/` (Stop Run), `Ctrl-W` (Close
Window), and `Ctrl-Q` (Quit Application) are supported.  Some sub-windows
also rebind shortcuts for window-specific actions:

- *Output* window: `Ctrl-N` jumps to the next warning or error,
  `Ctrl-S` saves the captured log to a file, `Ctrl-Y` exports any
  embedded YAML data, `Ctrl-Enter` runs the current input buffer.
- *Image Viewer* window: `Ctrl-S` saves the rendered image,
  `Ctrl-C` copies it to the clipboard, `Ctrl-D` copies the
  ``dump image`` / ``dump_modify`` commands to the clipboard.
- *Slide Show* window: `Ctrl-S` saves the currently displayed image,
  `Ctrl-C` copies it to the clipboard, `Ctrl-E` exports the
  image sequence to a movie file.

With the *Combined Main Window* layout the sub-windows are panels
docked into the main window, and a shortcut a panel shares with the
main window (for example `Ctrl-S`) is left to the main window; the
panel's own function remains available from its menu entry.

That layout also has keyboard shortcuts for moving between the panels:

- `F6` moves the keyboard focus to the next panel and `Shift-F6` to the
  previous one, walking the editor, the group on the right and the group
  across the bottom in turn and wrapping around at either end.  A panel
  that shares a tab group with others is reached this way when its tab
  is the one in front; the transient panels -- a text viewer, an
  inspected file, a plot of a data file -- are walked like any other.
- The shortcut that opens a panel also returns to it: pressing it while
  the panel is on screen but is not the one being worked in moves the
  focus there instead of hiding it, and pressing it again -- with the
  focus in the panel by then -- hides it as before.  With individual
  windows the shortcut toggles the window as it always has, since there
  the stacking and the focus belong to the window manager.
