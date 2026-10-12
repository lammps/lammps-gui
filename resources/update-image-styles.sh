#!/bin/bash
# this updates the list of fixes and computes supporting dump image

if [ $# -lt 1 ]
then
    echo "usage: $0 <lammps-src-dir>"
    exit 1
fi

cd "$(dirname $0)" || exit 1

# list of files or computes with derived classes where the image() function is in the base class
ADDLIST="fix_wall_harmonic.h  fix_wall_harmonic_outside.h  fix_wall_lj1043.h  fix_wall_lj126.h  fix_wall_lj93.h  fix_wall_morse.h  fix_wall_reflect.h  fix_wall_table.h COLLOID/fix_wall_colloid.h EXTRA-FIX/fix_wall_reflect_stochastic.h"

STYLELIST=""
for s in $(grep -l 'image(.*int.*double.*)' "$1"/{fix,compute}_*.h "$1"/*/{fix,compute}_*.h)
do \
    [ "$1/fix_wall.h" != $s ] && STYLELIST="${STYLELIST} ${s}"
done

# append styles from derived classes with known image() function support
for s in $ADDLIST
do \
    STYLELIST="${STYLELIST} $1/${s}"
done

touch image_style.table
mv image_style.table image_style.oldtable
touch image_style.tmp

for s in $STYLELIST
do \
    sed -n -e '/^Compute/s/ComputeStyle(\(.*\),.*/compute \1/p' \
        -e '/^Fix/s/FixStyle(\(.*\),.*/fix \1/p' $s >> image_style.tmp
done
sort image_style.tmp > image_style.table
rm image_style.tmp
cmp image_style.table image_style.oldtable > /dev/null || touch lammpsgui.qrc
