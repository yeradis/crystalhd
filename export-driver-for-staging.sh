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
cp -a $me/include/*.h $dest/
cp -a $me/include/link/bcm_70012_regs.h $dest/
# except these
rm -f $dest/vdec_info.h $dest/7411d.h, $dest/version_lnx.h

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
