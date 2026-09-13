---
title: 'LAMMPS-GUI: A Cross-Platform Graphical Tool to Learn and Explore Molecular Dynamics with LAMMPS'
tags:
  - C++
  - Qt
  - molecular dynamics
  - LAMMPS
  - simulation
  - scientific visualization
  - research software
  - education
authors:
  - name: Axel Kohlmeyer
    orcid: 0000-0001-6204-6475
    corresponding: true
    affiliation: 1
affiliations:
  - name: Institute for Computational Molecular Science, Temple University, Philadelphia, PA, USA
    index: 1
date: 27 June 2026
bibliography: paper.bib
---

# Summary

LAMMPS is a widely used, massively parallel classical molecular dynamics
(MD) engine for particle-based simulations at the atomic, mesoscopic,
and continuum scales [@thompson2022lammps]. Implemented in C++ as a
no-frills, console-mode application, it is highly portable and efficient,
but learning to use it requires mastering separate -- and often
platform-specific -- tools for editing inputs, plotting thermodynamic
data, and visualizing atomic configurations *before* one can begin to
learn MD itself.

LAMMPS-GUI is a cross-platform graphical application, written in C++17
using version 6 of the Qt framework [@qt_home], that combines these
tasks in one program: a syntax-highlighting input editor with
auto-completion and documentation lookup, live execution with real-time
output monitoring, interactive thermodynamic charts, a snapshot image
viewer, and a slide-show viewer. Rather than launching LAMMPS as an
external process, it embeds the engine through the C-language library
interface [@frantzdale2010library], running the simulation in a
concurrent worker thread so the interface stays responsive and the run
can be monitored, plotted, visualized, and cleanly stopped. By mirroring
the traditional "edit, run, observe, analyze" workflow, it lets users
move freely to the command-line executable -- essential once a workflow
outgrows a personal computer. Pre-compiled packages for Windows, macOS,
and Linux let most users start instantly, without compiling anything.

# Statement of need

LAMMPS-GUI grew out of teaching LAMMPS at tutorials and workshops, where
much of the available time went not to molecular dynamics but to
installing and operating the surrounding tool chain -- a text editor, a
plotting program, and a molecular visualization package -- most of which
differ between Windows, macOS, and Linux.  Earlier workarounds were
either too complex to sustain or broke when Apple moved to the ARM
architecture.  As a single, self-contained, cross-platform tool that
mirrors the console workflow, LAMMPS-GUI removes this barrier, letting
instructors teach one interface on all platforms and focus on LAMMPS and
molecular dynamics itself.

![A LAMMPS-GUI session with a simulation in flight, in the individual
window mode (left) and the combined window mode (right). The editor
shows syntax highlighting, line numbers, and a marker on the current
input line; the status bar shows CPU utilization and run progress; the
Image Viewer shows the starting geometry, the Output window the screen
output, and the Charts window a live plot of a thermodynamic column. In
the combined window mode these views are tabs beside and below the
editor.\label{fig:editor}](images/lammps-gui-screen.png){ width=99% }

# State of the field

Other approaches to making LAMMPS more accessible exist, but they target
different needs.  Commercial molecular-modeling packages provide
structure editors that export LAMMPS inputs and re-import results;
Atomify compiles a modified LAMMPS to WebAssembly and runs simulations
and visualization inside a web browser [@atomify_home]; and dedicated
visualization programs such as OVITO [@ovito2010] and VMD [@vmd1996]
render trajectories with a breadth and quality that LAMMPS-GUI does not
attempt to match.  None of these, however, integrates input editing,
live simulation, output monitoring, plotting, and visualization into one
application that follows the same edit-run-observe loop as the
standalone executable.  LAMMPS-GUI fills that gap.  It also imports
tutorial materials, such as the official LAMMPS tutorials
[@gravelle2025lammps] on molecular soft matter, with collections on
materials science and discrete element modeling in preparation.
Although aimed
primarily at beginners, features such as rapid prototyping of input
decks, debugging failing inputs by visualizing selected components, and
interactive construction of reproducible `dump image` commands also make
it useful to experienced researchers.

# Software design and functionality

LAMMPS-GUI follows an object-oriented design in which a central window
coordinates largely self-contained components, all access to LAMMPS
being funneled through a single adapter class.  Two viewing modes are
offered: the original individual window mode, where every component is
a window of its own, and a recently added combined window mode, where
the views are docked as tabbed panels beside and below the editor in
one main window, separated by movable splitters.  Key capabilities of
LAMMPS-GUI include:

- **Editing.** A LAMMPS-aware editor with syntax highlighting,
  per-command-category auto-completion, in-place documentation lookup,
  block comment/uncomment, and command reformatting.
- **Running.** Execution of the editor buffer through the library
  interface with no intermediate file I/O, a progress bar with estimated
  completion, support for the GPU, INTEL, KOKKOS, and OPENMP accelerator
  packages, and recovery from simulation errors -- reported as catchable
  exceptions -- without crashing.
- **Output and charts.** An output window that highlights warnings and
  errors and makes documentation URLs clickable, and a charts window that
  plots thermodynamic data read directly from the running simulation (not
  scraped from text) or from external files, with Savitzky-Golay smoothing
  [@savitzkygolay1964] and post-processing such as autocorrelation
  functions, Fourier transforms, static structure factors, and
  polynomial, Birch-Murnaghan equation-of-state, and custom nonlinear
  fits.
- **Visualization.** A snapshot image viewer that builds high-quality
  renderings via the LAMMPS `dump image` command, with interactive
  controls and the ability to copy the generated command back into the
  script for reproducible figures, plus a slide-show viewer that plays
  image sequences, imports frames from movie files, and exports movies.
- **Tutorials and convenience.** A guided wizard that downloads lesson
  inputs from several tutorial collections, a tabbed preferences dialog,
  hand-off of the current system to OVITO and VMD, and command-line flags
  exposing the charting, slide-show, and text viewers as standalone
  utilities.

What LAMMPS-GUI deliberately does not provide is a molecular editor or
builder.  Creating complex, physically sound initial configurations
with their topology has never been within the scope of LAMMPS, and the
GUI does not address it either.  The built-in lattice, region, and
molecule template commands cover many cases; beyond those, and
especially for the atom typing and partial charge assignment that
molecular force fields require, force-field specific external tools are
needed, for which the LAMMPS website maintains a curated list
[@lammps_home_prepost].

A recurring theme is the co-evolution of LAMMPS-GUI and LAMMPS:
front-end needs drove improvements to the engine and its library
interface -- catchable exceptions, a locked cache of live thermodynamic
data, and greatly expanded snapshot rendering, collected into a
dedicated GRAPHICS package [@kohlmeyer2025lammps], which brings
functionality long provided by dedicated visualization tools directly
to a running simulation rather than only to saved trajectory files.  In
its default *plugin mode*, LAMMPS-GUI loads the shared library at run
time with no link-time dependency, so one binary can pair with -- or
download -- different LAMMPS builds.

# Research impact statement

LAMMPS-GUI has shipped with LAMMPS since the August 2023 stable release,
first from within the LAMMPS source tree and since August 2025 as an
independently versioned package that the LAMMPS build system fetches on
request; the pre-compiled LAMMPS packages for Windows, macOS, and Linux
posted with each LAMMPS release bundle it [@lammps_releases].  It is the
primary tool of the peer-reviewed LAMMPS tutorials
[@gravelle2025lammps], which are written around it, and it has anchored
public hands-on training since the 2024 LAMMPS master class, including
the tutorial session of the 2025 LAMMPS Workshop and Symposium
[@lammps_workshop2025]; recordings of both live streams are online
[@templelammps_streams].  Its development has also fed improvements back
into the LAMMPS engine itself, as described above.  The project is
maintained as research software: a documentation site [@lammpsgui_home],
releases archived on Zenodo, continuous-integration builds and automated
tests on all three platforms, and user support through a dedicated tag
on the LAMMPS forum.

# AI usage disclosure

The design and architecture of LAMMPS-GUI were developed without the use
of generative AI.  However, AI-based coding assistants such as GitHub
Copilot and Claude Code were used more recently to support the
refactoring and modularization of the existing code base, and to assist
with editing this manuscript.  All AI-generated suggestions were
reviewed, tested, and revised by the author, who takes full
responsibility for the software and for the content of this paper.

# Acknowledgements

Financial support by Sandia National Laboratories under PO 2149742 and
PO 2407526, by the US National Science Foundation via Major Research
Infrastructure grants number 1625061 and 2216289, and by CCDC-ARL under
Cooperative Agreement Number W911NF-21-2-0007 is gratefully
acknowledged.  The author thanks the LAMMPS developers and the authors
of the LAMMPS tutorials for their feedback and for motivating many of
the features described here.

# References
