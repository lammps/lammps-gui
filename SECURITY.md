# Security Policy

LAMMPS-GUI is designed as a user-level application as a front end to
conduct computer simulations for research using classical mechanics
using the LAMMPS MD code.  As such LAMMPS-GUI depends mostly on
functionality from LAMMPS and through it to some degree on users
providing correctly formatted input and LAMMPS needs to read and write
files based on uncontrolled user input.  LAMMPS-GUI itself will forward
input in its editor to LAMMPS, but also will create custom command
sequences and feed them to LAMMPS internally.  It tries to do this
in the most reasonable manner and avoiding unexpected behavior, but
given the flexibility of LAMMPS, there is always a risk (as with any
graphical application). 

Thus it is quite easy to crash LAMMPS and LAMMPS-GUI through malicious
input and do all kinds of file system manipulations.  And because of
that LAMMPS should **NEVER** be compiled or **run** as superuser, either
from a "root" or "administrator" account directly or indirectly via
"sudo" or "su".

Therefore what could be seen as a security vulnerability is usually
either a user mistake or a bug in the code.  Bugs can be reported in the
LAMMPS project [issue tracker on
GitHub](https://github.com/lammps/lammps-gui/issues).

# Version Updates

LAMMPS-GUI follows a continuous release development model, same as
LAMMPS.  We aim to keep the development version (`develop` branch)
always fully functional and employ a variety of automatic testing
procedures to detect failures of existing functionality from adding or
modifying features.  Since advanced functionality in LAMMPS-GUI depends
on features in LAMMPS, there is only a limited window of compatibility.
Modern versions of LAMMPS-GUI will check which LAMMPS version is used
and will refuse to work with too old versions and disable functionality
depending on more recent LAMMPS versions.


# Integrity of Downloaded Archives

For *all* files that can be downloaded from the "lammps.org" web server
we provide SHA-256 checksum data in files named SHA256SUM.  These
checksums can be used to validate the integrity of the downloaded
archives.  Please note that we also use symbolic links to point to the
latest or stable releases and the checksums for those files *will*
change (and so their checksums) because the symbolic links will be
updated for new releases.

# Immutable GitHub Releases

Starting with LAMMPS-GUI version 1.8.1 the LAMMPS-GUI releases published
on GitHub are configured as `immutable`.  This means that after the
release is published the release tag cannot be changed or any of the
uploaded assets, i.e. the source tarball, the PDF manual, and the
pre-compiled packages of LAMMPS-GUI.  GitHub will generate a release
attestation JSON file which can be used to verify the integrity of the
files provided with the release.  Starting with LAMMPS-GUI version
1.8.5 the packaged binaries also include a pre-compiled version of
the LAMMPS shared library.
