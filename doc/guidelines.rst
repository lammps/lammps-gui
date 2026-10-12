**********************
Development Guidelines
**********************

Code Style
==========

The project follows these coding conventions:

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters
- **Formatting**: Enforced by ``.clang-format`` configuration (LLVM-based)
- **Comments**: Use Doxygen-style documentation comments
- **Naming**:
  - Classes: CamelCase (e.g., ``CodeEditor``, ``ChartWindow``)
  - Qt overrides: camelCase (e.g., ``setFont``, ``eventFilter``)
  - Custom methods/slots: camelCase (e.g., ``stopRun``, ``saveAs``)
  - Members: camelCase (e.g., ``reformatOnReturn``)

Documentation
=============

Added functionality should be documented both in the User's Guide and
the Programmer's Guide section of the documentation.  The documentation
should have suitable ``.. index::`` directives to populate the index
with suitable keywords.  The documentation is written in
`reStructuredText
<https://www.sphinx-doc.org/en/master/usage/restructuredtext/index.html>`_
and imports documentation of the source code from `Doxygen
<https://doxygen.nl/>`_ using the `Breathe Sphinx plugin
<https://www.breathe-doc.org>`_.  The documentation should translate
cleanly to `HTML <https://en.wikipedia.org/wiki/HTML>`_ and `PDF
<https://en.wikipedia.org/wiki/PDF>`_ using the ``html`` or ``pdf``
build targets.  Additionally the build targets ``spelling`` and
``linkcheck`` are available to run a spell checker on the documentation
and validate external links.

All public classes and functions should have Doxygen documentation as
demonstrated below:

.. code-block:: cpp

   /**
    * @brief Brief description
    *
    * Detailed description if needed
    *
    * @param param1 Description of parameter
    * @return Description of return value
    */
   int myFunction(int param1);

Building
========

See :doc:`installation` for detailed build instructions. For development:

.. code-block:: bash

   # Debug build with Qt6
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
         -DLAMMPS_GUI_USE_PLUGIN=ON -DBUILD_DOC=OFF
   cmake --build build --parallel 2

Before submitting a pull request, run ``clang-format`` on any modified
source files so they conform to the project's ``.clang-format``
configuration:

.. code-block:: bash

   clang-format -i src/*.cpp src/*.h

When adding new ``.cpp`` or ``.h`` files to ``src/``, also add them to
the ``PROJECT_SOURCES`` list in ``cmake/Sources.cmake`` so they are
picked up by the build (Qt's ``AUTOMOC`` handles ``moc`` generation
automatically once the file is listed).  The top-level
``CMakeLists.txt`` contains only the configuration options and the
executable target; the remaining build logic is organized into include
files in the ``cmake/`` folder.

All documentation should be written in American English using plain
ASCII characters (no typographic quotes, em-dashes written as ``--``,
etc.).

.. _add_colormap:

Adding or modifying a color map
===============================

.. index:: color map

The dump-image color maps shown in the :ref:`Atoms/bonds settings dialog
<atom_settings>` are defined once, as a table in ``src/colormaps.cpp``.  A
single ``ColorMapDef`` -- a ``continuous`` flag plus a list of ``ColorMapStop``
entries (each a position in ``[0,1]`` and either a LAMMPS named color or an
explicit RGB triple in ``[0,1]``) -- drives both the ``dump_modify`` command
emitted by ``appendColorMapArgs()`` (in ``src/dumpimage.cpp``) and the preview
swatch built by ``addColorMapItems()`` (in ``src/imageviewersettings.cpp``), so
the two cannot disagree.

To add a color map:

#. Add an entry to the ``maps`` table in ``src/colormaps.cpp``.  Use ``rc(pos,
   r, g, b)`` for an explicit RGB stop (components in ``[0,1]``) or ``nm(pos,
   "name")`` for a LAMMPS named color.  Set the ``ColorMapDef`` flag to ``true``
   for an interpolated (continuous) map or ``false`` for a discrete sequence.
   Use the same 0..1 floating-point values that LAMMPS renders so the preview
   matches exactly.
#. Add the map's name to ``colorMapNames()`` in the same file, at the position
   where it should appear in the selection menu.
#. Regenerate the documentation preview image and add the new map to the
   color-map list in ``doc/visualization.rst``::

       python3 doc/colormaps_preview.py

#. Optionally extend ``test/test_dumpimage.cpp`` to pin the new map's stops.

Modifying an existing map is just editing its stops in the table; nothing else
needs to change.  Canonical stops for a matplotlib color map can be resampled
with a few lines of Python::

    import matplotlib.cm as cm
    m = cm.get_cmap("cividis")
    n = 5  # number of stops
    print([tuple(round(c, 3) for c in m(i / (n - 1))[:3]) for i in range(n)])

.. _add_tutorial:

Adding or updating a tutorial collection
========================================

.. index:: tutorial

The :ref:`Tutorials menu <tutorials>` is driven by a single table of
``TutorialCollection`` entries in ``src/tutorials.cpp``.  Each collection is
built by a small factory function (``molecular()``, ``matsci()``,
``granular()``) and registered in the ``collections()`` list; the menu and the
``TutorialWizard`` read everything they need from that table, so adding or
updating a tutorial is a data change in one file -- no menu or wizard code has
to be touched.

How the files are hosted
------------------------

Each collection's input and solution files live in their own public (e.g.
GitHub) repository, laid out as one ``tutorial<N>/`` folder per tutorial.  The
``filesUrl`` field is a two-argument download pattern where ``%1`` is the
tutorial number and ``%2`` is the file name, for example
``.../matsci-tutorials-inputs/main/tutorial%1/%2``.

Every ``tutorial<N>/`` folder contains a ``.manifest`` text file that lists the
files to download, one per line (``#`` comments and blank lines are ignored):

- The **first** plain file name is the initial template that is auto-loaded
  into the editor when the tutorial starts.
- Other plain file names are support files downloaded alongside it.
- Lines under ``solution/`` are downloaded only when the user requests the
  solution files.

Large data files that are shared between the tutorial folder and its
``solution/`` subfolder do not need to be duplicated: store the copy in
``solution/`` as a symbolic link to the parent-directory file.  The raw file
server then serves that link as a one-line text file containing the link
target (e.g. ``../Al_zhou.eam.alloy``); ``downloadTutorialFiles()`` detects a
``../<same-name>`` payload and copies the already-downloaded parent file in
place of the placeholder.

A manifest entry that is missing on the server (e.g. after a file was renamed
without updating the ``.manifest``) does not abort the download: the remaining
files are still fetched and a dialog then lists the missing files, with a
button that opens the repository's issue tracker (``filesRepoUrl`` +
``/issues``) so users can report them.  Only a failure to download the
``.manifest`` itself or a cancellation by the user stops the setup.

Incremental rollout
-------------------

Collections are released tutorial by tutorial.  Three fields on
``TutorialCollection`` control how an unfinished collection appears:

- ``available`` -- the number of leading tutorials that are launchable now.
  Entries at or beyond this index are shown in the menu but disabled, acting as
  a preview of what is coming.
- ``published`` -- when ``false``, the submenu title gets the ``status`` text
  appended as a suffix.
- ``status`` -- that suffix text, e.g. ``"coming soon"`` or ``"planned"``.

A collection with no tutorials defined yet (``count() == 0``) is shown as a
single disabled submenu.

Common changes
--------------

#. **Enable the next tutorial in a collection that is rolling out:** bump
   ``c.available`` in that collection's factory function once the tutorial's
   text and files are ready.

#. **Update a tutorial's menu/wizard text:** edit the corresponding entry in
   the ``titles``, ``slugs``, and ``blurbs`` lists (all parallel, one entry per
   tutorial) in the factory function.

#. **Add a whole new collection:** add a factory function that fills in
   ``key``, ``name``, ``dirPrefix``, ``author``, ``filesUrl``,
   ``filesRepoUrl``, ``webUrl``/``siteUrl``, ``logo``, the ``titles`` /
   ``slugs`` / ``blurbs`` lists, and the rollout fields, then append it to the
   ``collections()`` list.  Host the files in the ``tutorial<N>/`` +
   ``.manifest`` layout described above.

#. **Fully publish a collection:** set ``available`` to the tutorial count and
   ``published = true`` so the teaser suffix and the disabled entries go away.

Contributing
============

To contribute to LAMMPS-GUI:

1. Fork the repository on GitHub
2. Create a feature branch
3. Make your changes with proper documentation
4. Ensure code compiles with Qt 6.2 or later
5. Test your changes thoroughly
6. Submit a pull request

All contributions must:

- Follow the existing code style and pass ``clang-format``
- Include Doxygen documentation for new public APIs
- Register new source files in ``PROJECT_SOURCES`` in ``cmake/Sources.cmake``
- Not break existing functionality
- Have GPG-signed commits
