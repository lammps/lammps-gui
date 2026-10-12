*************
Editor Window
*************

.. _editor:

.. index:: editor window
.. index:: text editor
.. index:: editing features
.. index:: syntax highlighting

The *Editor* window of LAMMPS-GUI has most of the usual functionality
that similar programs have: text selection via mouse or with cursor
moves while holding the Shift key, Cut (`Ctrl-X`), Copy (`Ctrl-C`),
Paste (`Ctrl-V`), Undo (`Ctrl-Z`), Redo (`Ctrl-Shift-Z`), Select All
(`Ctrl-A`).  When trying to exit the editor with a modified buffer, a
dialog will pop up asking whether to cancel the exit operation, or to
save or not save the buffer contents to a file.

.. index:: auto-save

The editor has an auto-save mode that can be enabled or disabled in the
*Preferences* dialog.  In auto-save mode, the editor buffer is
automatically saved before running LAMMPS or before exiting LAMMPS-GUI.

Context Specific Word Completion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index:: auto-completion
.. index:: word completion
.. index:: code completion

By default, LAMMPS-GUI displays a small pop-up frame with possible
choices for LAMMPS input script commands or styles after 2 characters of
a word have been typed.

.. image:: JPG/lammps-gui-complete.png
   :align: center
   :scale: 75%

The word can then be completed through selecting an entry by scrolling
up and down with the cursor keys and selecting with the 'Enter' key or
by clicking on the entry with the mouse.  The automatic completion
pop-up can be disabled in the *Preferences* dialog, but the completion
can still be requested manually by either hitting the `Shift+TAB` key or
by right-clicking with the mouse and selecting the option from the
context menu.  Most of the completion information is retrieved from the
active LAMMPS instance and thus it shows only available options that
have been enabled when compiling LAMMPS. That list, however, excludes
accelerated styles and commands; for improved clarity, only the
non-suffix versions of styles are shown.

Like the syntax highlighting, the completion and the context-specific
help are aware of ``&`` line continuations: on a continuation line,
completions and help are offered for the command that is being
continued.

Syntax Highlighting
^^^^^^^^^^^^^^^^^^^

.. index:: syntax highlighting
.. index:: line continuation
.. index:: unknown commands

The editor highlights LAMMPS input scripts following the same parsing
rules as LAMMPS itself: comments, single, double, and triple quoted
strings, ``$`` variable substitutions, and numbers are recognized, and
command names are colored by category.  Arguments are colored by their
role in the command, for example the ID, group-ID, and style name of a
`fix <https://docs.lammps.org/fix.html>`_ command.  IDs use the same
color whether they are being defined (``fix``, ``compute``, ``dump``)
or referenced (``fix_modify``, ``unfix``, and so on).  A few commands
embed a second command in their argument list; the keyword that starts
the embedded part is shown in the color of the command it stands for,
that is the ``modify`` keyword of the `write_dump
<https://docs.lammps.org/write_dump.html>`_ command is colored like
``dump_modify`` and the ``dump`` keyword of the `rerun
<https://docs.lammps.org/rerun.html>`_ command like ``read_dump``.  The
arguments following such a keyword are colored as arguments of the
embedded command, also when the command is spread over multiple lines
with ``&`` line continuations.  Lines joined with
the ``&`` line continuation character are highlighted in the context of
the command they continue, including style names or quoted strings that
are split across lines.

Sub-styles of hybrid styles are recognized the same way LAMMPS parses
them -- by checking against the known styles of the loaded library --
and are shown in their own color, both in the arguments of the
``*_style`` command and in the sub-style position of the corresponding
``*_coeff`` commands, where they also auto-complete.  Words in those
positions that are not a known style are treated as arguments of the
sub-styles and are never marked as unknown.  On `dump image
<https://docs.lammps.org/dump_image.html>`_ lines the dump image
keywords are marked, and on both ``dump image`` and ``dump_modify``
lines color names are displayed in their actual color; keywords and
color names auto-complete in the positions where they are expected.
Color names with too little contrast against the editor background
(for example ``yellow`` on a light theme) are placed on a small dark
or light chip so they remain readable.

Command and style names that are not known to the LAMMPS library that
LAMMPS-GUI has loaded -- for example due to a typo or because the
corresponding package was not included when the library was compiled --
are marked with a wavy underline.  The marker is not shown for the word
at the cursor position, so partially typed names are not flagged while
typing, and it is suppressed for names constructed with ``$``
substitutions.  When LAMMPS-GUI is running without a usable LAMMPS
library (for example in plugin mode before a library has been
selected), the known-name information is unavailable and no words are
marked.

Line Reformatting
^^^^^^^^^^^^^^^^^

.. index:: line reformatting
.. index:: code formatting
.. index:: indentation

The editor supports reformatting lines according to the syntax in order
to have consistently aligned lines.  This primarily means adding
whitespace padding to commands, type specifiers, IDs and names.  This
reformatting is performed manually by hitting the 'Tab' key.  It is
also possible to have this done automatically when hitting the 'Enter'
key to start a new line.  This feature can be turned on or off in the
*Preferences* dialog for *Editor Settings* with the
"Reformat with 'Enter'" checkbox. The amount of padding for multiple
categories can be adjusted in the same dialog.

Internally this functionality is achieved by splitting the line into
"words" and then putting it back together with padding added where the
context can be detected; otherwise a single space is used between words.

Context Specific Help
^^^^^^^^^^^^^^^^^^^^^

.. index:: context help
.. index:: documentation; inline help
.. index:: documentation; online
.. index:: LAMMPS documentation

.. |gui-popup1| image:: JPG/lammps-gui-popup-help.png
   :width: 44%

.. |gui-popup2| image:: JPG/lammps-gui-popup-view.png
   :width: 55%

|gui-popup1|  |gui-popup2|

A unique feature of LAMMPS-GUI is the option to look up the LAMMPS
documentation for the command in the current line.  This can be done by
either clicking the right mouse button or by using the `Ctrl-?` keyboard
shortcut.  When using the mouse, there are additional entries in the
context menu that open the corresponding documentation page in the
online LAMMPS documentation in a web browser window.  When using the
keyboard, the first of those entries is chosen.

If the current line defines an index style variable, the context menu
also has a *Set Variables...* entry that opens the corresponding
:ref:`dialog <set_variables>` from the *Edit* menu.

If the word under the cursor is a file, then additionally the context
menu has an entry to open the file in a read-only text viewer window.
If the file is a LAMMPS restart file, instead the menu entry offers to
:ref:`inspect the restart <inspect_restart>`.

The text viewer is a convenient way to view the contents of files that
are referenced in the input.  The file viewer also supports on-the-fly
decompression based on the file name suffix in a `similar fashion as
available with LAMMPS
<https://docs.lammps.org/Build_settings.html#read-or-write-compressed-files>`_.
If the necessary decompression program is missing or the file cannot be
decompressed, the viewer window will contain a corresponding message.

.. _inspect_restart:

Inspecting a Restart file
^^^^^^^^^^^^^^^^^^^^^^^^^

.. index:: restart file inspection
.. index:: restart files
.. index:: file inspection

When LAMMPS-GUI is asked to "Inspect a Restart", it will read the
restart file into a LAMMPS instance and then open three different
windows.  The first window is a text viewer with the output of an `info
command <https://docs.lammps.org/info.html>`_ with system information
stored in the restart.  The second window is a text viewer containing a
data file generated with a `write_data command
<https://docs.lammps.org/write_data.html>`_.  The third window is a
:ref:`Snapshot Image Viewer <snapshot_viewer>` containing a
visualization of the system in the restart.

.. |inspect1| image:: JPG/lammps-gui-inspect-data.png
   :width: 30%

.. |inspect2| image:: JPG/lammps-gui-inspect-info.png
   :width: 30%

.. |inspect3| image:: JPG/lammps-gui-inspect-image.png
   :width: 35%

|inspect1|  |inspect2|  |inspect3|

.. admonition:: Large Restart Files
   :class: warning

   If the restart file is larger than 250 MBytes, a dialog will ask for
   confirmation before continuing, since large restart files may require
   large amounts of RAM: the entire system must be read into memory.
   Thus restart files for large simulations that have been run on an HPC
   cluster may overload a laptop or local workstation. The *Show
   Details...* button will display a rough estimate of the additional
   memory required.

