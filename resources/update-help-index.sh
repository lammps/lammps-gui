#!/bin/bash
# this updates the help index table

if [ $# -lt 1 ]
then
    echo "usage: $0 <lammps-doc-dir>"
    exit 1
fi

cd "$(dirname $0)" || exit 1

mv help_index.table help_index.oldtable
grep '\.\. index::' "$1"/src/*.rst | sort \
    | sed -e 's/^.*src\/\([^/]\+\)\.rst:/\1.html /' \
          -e 's/\.\. \+index:: \+//' \
    | grep -v ^Developer_write_docs.html > help_index.table
cmp help_index.table help_index.oldtable > /dev/null || touch lammpsgui.qrc
