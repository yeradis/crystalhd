#!/bin/bash
#
# Crude script to export kernel driver bits to a kernel git tree,
# under drivers/staging/crystalhd/
# You still need to edit drivers/staging/{Kconfig,Makefile} by hand
#

if [ -z "$1" ]; then
  echo "You need to specify a path to a kernel tree"
  exit 1
fi

kernelsrc=$1
dest=$kernelsrc/drivers/staging/crystalhd
me=$PWD

if [ ! -e $dest ]; then
  mkdir $dest
fi

# Copy  files into place in kernel git tree
cp -a $me/driver/linux/*.c $dest/
cp -a $me/driver/linux/*.h $dest/

# copy userspace headers
cp -a $me/include/bc_dts_defs.h $dest/
cp -a $me/include/bc_dts_glob_lnx.h $dest/

# copy register defines for bcm70012 bcm70015
cp -a $me/include/link/bcm_70012_regs.h $dest/
cp -a $me/include/flea/bcm_70015_regs.h $dest/

# copy random header
cp -a $me/include/flea/DriverFwShare.h $dest/

# Now run unifdef over the source to strip out legacy compat
pushd $dest
perl -pi -e 's|KERNEL_VERSION.*|1|g' *.c *.h
for f in *.c *.h
do
  cp $f tmp-$f
  unifdef -DLINUX_VERSION_CODE=2 tmp-$f > $f
  rm -f tmp-$f
done

# Now show diff and diffstat
git diff -p --stat
