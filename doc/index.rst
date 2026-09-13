########################
LAMMPS-GUI Documentation
########################

.. toctree::
   :caption: LAMMPS-GUI Documentation

.. only:: html

   .. image:: _images/lammps-gui-banner.png
      :align: center
      :scale: 75%


****************
About LAMMPS-GUI
****************

LAMMPS-GUI is a graphical text editor with syntax highlighting,
auto-completion, inline help, and indentation support for `LAMMPS
<https://www.lammps.org/>`_ input files.  It is programmed using the `Qt
Framework <https://www.qt.io/>`_ and customized for running, monitoring,
and visualizing LAMMPS simulations.  It calls LAMMPS directly using the
`LAMMPS library interface
<https://docs.lammps.org/Library.html#lammps-c-library-api>`_ instead of
launching an external LAMMPS executable.  Therefore it can retrieve and
display information from LAMMPS *while it is running* and *immediately*
display visualizations created by a dump image command in the input.

The primary motivation for implementing LAMMPS-GUI is to facilitate
teaching LAMMPS to beginners using only LAMMPS-GUI and to have a
consistent behavior across major platforms like Linux, macOS, and
Windows.  This way one can focus on teaching LAMMPS and avoid having to
spend time explaining different tools (for editing inputs, plotting
graphs, visualizing systems) on the different platforms.  Also,
LAMMPS-GUI is fully integrated with a `collection of LAMMPS tutorials
<https://lammpstutorials.github.io>`_.

Many of the features in LAMMPS-GUI are useful beyond working on
tutorials.  For instance, it can streamline the process of prototyping
new simulation projects or debugging misbehaving simulations.  It also
has been found extremely useful in creating, debugging, and tweaking
`advanced visualizations with LAMMPS
<https://docs.lammps.org/Howto_viz.html>`_ using the built-in
visualization facility of the `dump image command
<https://docs.lammps.org/dump_image.html>`_.

--------

LAMMPS-GUI is Copyright (c) |copyright|, and its source code is
distributed under the terms of the `GNU General Public License v2.0
<https://choosealicense.com/licenses/gpl-2.0/>`_ or later.

Custom rangeslider widget for Qt written by Hoyoung Lee and
distributed under the `CeCILL Free Software License
<https://choosealicense.com/licenses/cecill-2.1/>`_.

Lepton library written by Peter Eastman and OpenMM contributors
distributed under the `MIT License <https://choosealicense.com/licenses/mit/>`_.

The LAMMPS-GUI documentation is available under the `Creative Commons
Attribution 4.0 International License
<https://choosealicense.com/licenses/cc-by-4.0/>`_.

--------

*******************
About this document
*******************

This document contains the documentation of LAMMPS-GUI and how to
compile, install, use, configure, and modify it.  Suggestions for new
features and reports of bugs are always welcome.  You can use the `same
channels as for LAMMPS itself
<https://docs.lammps.org/Errors_bugs.html>`_ for that purpose or submit
bug reports or pull requests in the `LAMMPS-GUI GitHub repository
<https://github.com/lammps/lammps-gui>`_.

------------------

.. raw:: html

   <h2>

This document describes LAMMPS-GUI version |version|.

.. raw:: html

   </h2>
   <hr>
   <h3>Test Status of the development branch:</h3>
   <p align="left">
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/compile-linux-qt6.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/compile-linux-qt6.yml/badge.svg" alt="Compile with Qt 6.x" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/compile-macos-dmg.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/compile-macos-dmg.yml/badge.svg" alt="Compile on macOS and build DMG" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/build-html-docs.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/build-html-docs.yml/badge.svg" alt="Build Documentation in HTML" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/compile-windows-msvc.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/compile-windows-msvc.yml/badge.svg" alt="Compile on Windows with MSVC" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/compile-mingw64-cross.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/compile-mingw64-cross.yml/badge.svg" alt="Cross-compile for Windows with MinGW64" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/build-linux-flatpak.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/build-linux-flatpak.yml/badge.svg" alt="Build LAMMPS-GUI as flatpak bundle" style="max-width: 100%;"></a>
   <a href="https://github.com/lammps/lammps-gui/actions/workflows/codeql-analysis.yml"><img src="https://github.com/lammps/lammps-gui/actions/workflows/codeql-analysis.yml/badge.svg" alt="CodeQL Code Analysis" style="max-width: 100%;"></a>
   <a href="https://scan.coverity.com/projects/akohlmey-lammps-gui"><img alt="Coverity Scan Build Status" src="https://scan.coverity.com/projects/33110/badge.svg"/></a>
   </p>

------------------

*****************
Citing LAMMPS-GUI
*****************

There is currently no citation specifically describing LAMMPS-GUI but a
manuscript has been submitted to `JOSS <https://joss.theoj.org/>`_.
Also, starting with version 3.0.0 LAMMPS-GUI releases are automatically
archived on `Zenodo <https://zenodo.org>`_:

.. code-block:: bibtex

   @software{lammps_gui_zenodo,
     author       = {Kohlmeyer, Axel},
     title        = {{LAMMPS-GUI}: A Cross-Platform Graphical Tool to
                      Learn and Explore Molecular Dynamics with LAMMPS},
     publisher    = {Zenodo},
     doi          = {10.5281/zenodo.21035505},
     url          = {https://doi.org/10.5281/zenodo.21035505},
   }

.. raw:: html

   <a href="https://joss.theoj.org/papers/59eed23e3cdee45c6585356fb7c23ca8"><img src="https://joss.theoj.org/papers/59eed23e3cdee45c6585356fb7c23ca8/status.svg"></a>
   <a href="https://doi.org/10.5281/zenodo.21035505"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.21035505.svg" alt="DOI"></a>

An introduction to LAMMPS-GUI is included in the following publication
in LiveCoMS for the LAMMPS tutorials that are linked from LAMMPS-GUI, so
the suggestion is to cite that publication for now:

   Gravelle, S., Alvares, C. M. S., Gissinger, J. R., &
   Kohlmeyer, A. (2025). A Set of Tutorials for the LAMMPS Simulation
   Package [Article v1.0]. Living Journal of Computational Molecular
   Science, 6(1), 3037. https://doi.org/10.33011/livecoms.6.1.3037

or in BibTeX format:

.. code-block:: bibtex

   @article{lammps_tutorials_2025,
     author={Gravelle, Simon and Alvares, Cecilia M. S. and Gissinger, Jacob R. and Kohlmeyer, Axel},
     title={A Set of Tutorials for the {LAMMPS} Simulation Package [Article v1.0]},
     journal={Living Journal of Computational Molecular Science},
     pages={3037},
     volume={6},
     number={1},
     year={2025},
     month={Sep.},
     url={https://livecomsjournal.org/index.php/livecoms/article/view/v6i1e3037},
     DOI={10.33011/livecoms.6.1.3037}
   }

------------------

.. raw:: latex

   \clearpage

************
User's Guide
************

.. toctree::
   :caption: Table of Contents
   :maxdepth: 2
   :numbered: 3
   :name: userdoc
   :includehidden:

   installation
   overview
   basic_usage
   output
   visualization
   editor
   menus
   dialogs
   shortcuts

------------------

.. raw:: latex

   \clearpage

******************
Programmer's Guide
******************

This guide provides documentation for developers who want to understand
the internals of LAMMPS-GUI or contribute to its development.

.. admonition:: AI Generated Content
   :class: note

   The initial version of the Programmer's Guide section was created by
   the `GitHub Copilot Coding Agent <https://docs.github.com/en/copilot>`_
   and not everything has been carefully checked.  It is therefore
   possible that it contains errors where the LLM has misinterpreted the
   LAMMPS-GUI source code.  If you spot any such errors or inconsistencies,
   please submit a bug report issue to point them out or -- even better --
   submit a pull request with the necessary corrections.

.. toctree::
   :caption: Table of Contents
   :maxdepth: 2
   :numbered: 3
   :name: progdoc
   :includehidden:

   introduction
   api_reference
   guidelines
   testing

----------

.. only:: html

   ****************
   Index and Search
   ****************

          * :ref:`genindex`
          * :ref:`search`

   ----------

   .. _webbrowser:
   .. admonition:: Web Browser Compatibility

     This website makes use of advanced features present in "modern" web
     browsers.  This leads to incompatibilities with older web browsers
     and specific vendor browsers (e.g. Internet Explorer on Windows)
     where parts of the pages are not rendered as expected (e.g. the
     layout is broken or mathematical expressions not typeset).

     The following web browser versions have been verified to work as
     expected on Linux, macOS, and Windows where available:

     - Safari version 11.1 and later
     - Firefox version 54 and later
     - Chrome version 54 and later
     - Opera version 41 and later
     - Edge version 80 and later

     Also Android version 7.1 and later and iOS version 11 and later have
     been verified to render this website as expected.
