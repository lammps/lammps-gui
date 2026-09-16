*******
Dialogs
*******

.. index:: dialogs

Find and Replace
----------------

.. index:: Find and Replace
.. index:: dialogs; Find and Replace
.. index:: text search

.. image:: JPG/lammps-gui-find.png
   :align: right
   :scale: 33%

The *Find and Replace* dialog allows searching for and replacing
text in the *Editor* window.

The dialog can be opened either from the *Edit* menu or with the
keyboard shortcut `Ctrl-F`. You can enter the text to search for.

.. admonition:: Through three checkboxes the search behavior can be adjusted:

   - If checked, "Match case" does a case-sensitive search; otherwise
     the search is case-insensitive.

   - If checked, "Wrap around" starts searching from the start of the
     document, if there is no match found from the current cursor position
     until the end of the document; otherwise the search will stop.

   - If checked, the "Whole word" setting only finds full word matches
     (white space and special characters are word boundaries).

Clicking on the *Next* button will search for the next occurrence of the
search text and select / highlight it. Clicking on the *Replace* button
will replace an already highlighted search text and find the next one.
If no text is selected, or the selected text does not match the
selection string, then the first click on the *Replace* button will
only search and highlight the next occurrence of the search string.
Clicking on the *Replace All* button will replace all occurrences from
the cursor position to the end of the file; if the *Wrap around* box is
checked, then it will replace **all** occurrences in the **entire**
document.  Clicking on the *Done* button will dismiss the dialog.

------

.. _preferences:

Preferences
-----------

.. index:: preferences
.. index:: dialogs; Preferences
.. index:: settings
.. index:: configuration

The *Preferences* dialog allows customization of the behavior and
look of LAMMPS-GUI.  The settings are grouped and each group is
displayed within a tab.

.. |guiprefs1| image:: JPG/lammps-gui-prefs-general.png
   :width: 19%

.. |guiprefs2| image:: JPG/lammps-gui-prefs-accel.png
   :width: 19%

.. |guiprefs3| image:: JPG/lammps-gui-prefs-image.png
   :width: 19%

.. |guiprefs4| image:: JPG/lammps-gui-prefs-editor.png
   :width: 19%

.. |guiprefs5| image:: JPG/lammps-gui-prefs-charts.png
   :width: 19%

|guiprefs1|  |guiprefs2|  |guiprefs3|  |guiprefs4|  |guiprefs5|

General Settings
^^^^^^^^^^^^^^^^

.. index:: general settings
.. index:: preferences; general

.. admonition:: The following settings are available in this tab:

   - **Echo input to output buffer:** when checked, all input commands,
     including variable expansions, are echoed to the :ref:`Output window
     <logfile>`. This is equivalent to using ``-echo screen`` on the
     command-line.  There is no log *file* produced by default, since
     LAMMPS-GUI uses ``-log none``.
   - **Individual Windows** / **Combined Main Window:** selects how the
     Output, Charts, Image, Slide Show, Variables, and Command window
     views are presented.
     With *Individual Windows* each of them is a window of its own, placed
     and stacked freely.  With *Combined Main Window* they become panels
     docked around the editor: the editor keeps the center, the Charts,
     Image, and Slide Show views share a tabbed group on the right, and
     the Output, Variables, and Command window views share a group across
     the full width at the bottom.  The panels have fixed places and are not dragged around;
     they are shown and hidden from the *View* menu, and the splitters
     between them can be moved to change their proportions.  The windows
     opened on demand join them rather than floating above: a text viewer,
     an inspected data file or image, a slide show of image or movie files
     opened with *View Image or Movie File(s)...*, and a plot made with
     *Plot Data File...* all become further tabs of the group on the
     right, and are closed from their own *File* menu.  A panel that
     shares its area with another is named by its tab, one that is alone
     by a title bar.  The proportions are kept when the main window is
     resized and restored in the next session, and the main window
     remembers a size of its own for each of the two layouts.  Changing
     this setting relaunches LAMMPS-GUI.  A single session can be started
     in either layout without changing this setting, with the ``-j`` or
     ``-w`` :ref:`command-line flag <command-line-options>`.
     In the combined window the views
     do not remember individual window sizes, and a keyboard shortcut that
     a view shares with the main window (for example ``Ctrl+S``) is left to
     the main window, so only its menu entry remains for the view.
   - **Include citation details:** when checked, full citation info will be
     included in the Output window.  This is equivalent to using ``-cite
     screen`` on the command-line.
   - **Show Output window by default:** when checked, the screen output of
     a LAMMPS run will be collected in an Output window during the run.
   - **Show Charts window by default:** when checked, the thermodynamic
     output of a LAMMPS run will be collected and displayed in a Charts
     window as line graphs.
   - **Show Slide Show window by default:** when checked, a Slide Show
     window will be shown with images from a dump image command, if
     present, in the LAMMPS input.
   - **Open main window maximized:** when checked, LAMMPS-GUI starts with
     its main window filling the screen instead of restoring the size it
     had when it was last closed.  This applies to the combined main window
     only and is disabled for individual windows, where a maximized main
     window would cover the very windows it is meant to sit beside.
   - **Download tutorial solutions enabled:** this controls whether the
     "Download solutions" option is enabled by default when setting up
     a tutorial.
   - **Open tutorial webpage enabled:** this controls whether the "Open
     tutorial webpage in web browser" option is enabled by default when
     setting up a tutorial.
   - **Select Default Font:** Opens a font selection dialog where the type
     and size for the default font (used for everything but the editor and
     log) of the application can be set.
   - **Select Text Font:** Opens a font selection dialog where the type and
     size for the text editor and log font of the application can be set.
   - **Data update interval:** Allows the user to set, in milliseconds,
     the time interval between data updates during a LAMMPS run.  The
     default is to update the data (for the Charts and Output windows)
     every 10 milliseconds.  This is good for many cases.  Set this to 100
     milliseconds or more if LAMMPS-GUI consumes too many resources during
     a run.  For LAMMPS runs that run *very* fast (for example in tutorial
     examples), however, data may be missed; this can be corrected by
     lowering this interval.  However, this will make the GUI use more
     resources.  This setting may be changed to a value between 1 and 1000
     milliseconds.
   - **Charts update interval:** Allows the user to set, in milliseconds,
     the time interval between redrawing the plots in the :ref:`Charts
     window <charts>`.  The default is to redraw the plots every 500
     milliseconds.  This is just for the drawing; data collection is
     managed with the previous setting.
   - **HTTPS proxy setting:** Allows the user to enter a URL for an HTTPS
     proxy.  This may be needed when the LAMMPS input contains `geturl
     commands <https://docs.lammps.org/geturl.html>`_ or for downloading
     tutorial files from the *Tutorials* menu.  If the ``https_proxy``
     environment variable was set externally, its value is displayed but
     cannot be changed.
   - **Download timeout:** Sets the time, in seconds, after which a
     download (of tutorial files or the LAMMPS shared library) is aborted
     with an error message when no data has arrived.  The default is 10
     seconds.  Users with a slow internet connection may want to increase
     this value so that the download of larger files, for example the
     LAMMPS shared library, is not canceled prematurely.
   - **Path to LAMMPS Shared Library File:** this option is only visible
     when LAMMPS-GUI was compiled to load the LAMMPS library at runtime
     instead of being linked to it directly.  Using the *Browse...* button
     or by changing the text, a different shared library file with a
     different compilation of LAMMPS with different settings or from a
     different version can be loaded.  The accompanying *Download LAMMPS
     shared library...* button retrieves a pre-built LAMMPS shared library
     from the LAMMPS web server.  After changing this setting, LAMMPS-GUI
     needs to be re-launched.  This setting is remembered separately for
     each compiler toolchain LAMMPS-GUI was built with, so for example an
     MSVC build and a MinGW build on the same machine each keep their own
     library: a library built against a different C runtime loads and runs,
     but its screen output would silently bypass the Output window.
   - **Command window shell:** selects the command interpreter that the
     :ref:`Command window <commandwindow>` starts, from a list of the
     shells installed on the machine: on Unix-like systems the entries of
     ``/etc/shells`` that are actual shells (not ``nologin`` or a
     terminal multiplexer), on Windows ``cmd.exe``, PowerShell, and a
     ``bash.exe`` from an installation such as Git for Windows, when
     present.  A shell listed under more than one path (for example under
     both ``/bin`` and ``/usr/bin``) is offered only once.  The default is the user's default shell (``SHELL`` on
     Unix-like systems, ``COMSPEC`` on Windows).  A change takes effect
     when the next shell starts: when the Command window is first opened,
     or on its *File* > *Restart Shell*.

Accelerators
^^^^^^^^^^^^

.. index:: accelerators
.. index:: preferences; accelerators
.. index:: GPU acceleration
.. index:: thread parallelization

This tab enables selection of an accelerator package and modification of
some of its settings for use when running LAMMPS.  This is equivalent to
using the `-sf <https://docs.lammps.org/suffix.html>`_ and `-pk
<https://docs.lammps.org/package.html>`_ flags `on the command-line
<https://docs.lammps.org/Run_options.html>`_.  Only settings supported
by the LAMMPS library and local hardware are available.  The `Number of
threads` field allows setting the number of threads for the accelerator
packages that support using threads (OPENMP, INTEL, KOKKOS, and GPU).
Furthermore, the precision mode (double, mixed, or single) for the INTEL
package can be selected, and for the GPU package, whether the neighbor
lists are built on the GPU or the host (required for `pair style hybrid
<https://docs.lammps.org/pair_hybrid.html>`_) and whether only pair
styles should be accelerated (i.e., run PPPM entirely on the CPU, which
sometimes leads to better overall performance).  Whether settings can be
changed depends on which accelerator package is chosen (or "None").

.. _image_preferences:

Snapshot Image
^^^^^^^^^^^^^^

.. index:: snapshot image settings
.. index:: preferences; snapshot image
.. index:: image rendering

This tab allows setting defaults for the snapshot images displayed in
the :ref:`Image Viewer window <snapshot_viewer>`, such as its
dimensions, the zoom factor, and view angles.  The **Antialias** switch
will render images with double the number of pixels for width and height
and then smoothly scale the image back to the requested size.  This
produces higher quality images with smoother edges at the expense of
requiring more CPU time to render an initial image four times the size.
The **HQ Image mode** option turns on "Screen Space Ambient Occlusion
(SSAO)" mode when rendering images.  This is also more time consuming,
but produces a more 'spatial' representation of the system with shading
of atoms by their depth.  The **Shiny Image mode** option will render
objects with a shiny surface when enabled.  Otherwise, the surfaces will
be matte.  The **Show Box** option selects whether the system box is
drawn as a colored set of sticks.  Furthermore, the diameter of the
sticks and their color can be set. Similarly, the **Show Axes** option
selects whether a representation of the three system axes will be drawn
or not (as colored and labeled arrows).  In addition, the axes length
and diameter can be set in fractions of the image size.  The **VDW
Style** checkbox selects whether atoms are represented by space filling
spheres when checked or by smaller spheres and sticks.  The **Dynamic
Bonds** checkbox selects whether bonds between atoms shall be
automatically determined from the atom distances (for instance to
visualize simulations using force fields with implicit bonds), and the
corresponding "Bond Cutoff" text field allows the user to set the cutoff
used for that feature.  Finally, there are a couple of text fields to
select the two **Background Colors**.  If the two colors differ, there
will be a vertical background gradient starting with the "Background"
color at the bottom and ending with the "Background2" color at the top.

These settings correspond to the available settings for the LAMMPS `dump
image and corresponding dump_modify commands
<https://docs.lammps.org/dump_image.html>`_.

Editor Settings
^^^^^^^^^^^^^^^

.. index:: editor settings
.. index:: preferences; editor
.. index:: code formatting preferences

This tab allows adjusting settings of the :ref:`editor window <editor>`.
Specifically, the amount of padding to be added to LAMMPS commands,
types or type ranges, IDs (e.g., for fixes), and names (e.g., for groups).
The value set is the minimum width for the text element and it can be
chosen in the range between 1 and 32.

The three settings which follow enable or disable the automatic
reformatting when hitting the 'Enter' key, the automatic display of
the completion pop-up window, and whether auto-save mode is enabled.
In auto-save mode, the editor buffer is saved before a run or before
exiting LAMMPS-GUI.

The last setting enables or disables (default: enabled) the static
input check (see the *Check Input via Heuristics* entry of the
:ref:`Run menu <run_menu>`) that runs automatically before every run.
Only error-level findings, which would make LAMMPS reject the input,
open a dialog asking whether to run anyway; warnings are only noted in
the status bar.

Charts Settings
^^^^^^^^^^^^^^^

.. index:: charts settings
.. index:: preferences; charts
.. index:: plotting preferences

This tab allows adjusting settings of the :ref:`Charts window <charts>`.
Specifically, one can set the default chart title (if the title contains
'%f' it will be replaced with the name of the current input file), one
can select whether by default the raw data, the smoothed data, or both
will be plotted, the default smoothing parameters, the default size of
the chart graph in pixels, and whether you want to display major and
minor grid lines.

The *Raw data*, *Processed data*, and *Error bars* groups hold the
defaults of the per-chart *Chart Style...* dialog (see :ref:`Adjust chart
style <charts>`): the display style (*Lines*, *Points*, or *Lines +
Points*), the color (one of ten preset colors), the line width, and the
point size of the two data series, plus the color and line width of the
error bars that imported data can carry.  Changing a color here also applies to charts that are
already open, since a chart only stores a color of its own once one is
picked in its *Chart Style...* dialog; the widths and display styles are
picked up by charts created afterwards.
