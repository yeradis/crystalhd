## History

There are various versions of the [Broadcom
CrystalHD](http://kodi.wiki/view/Broadcom_Crystal_HD) (BCM70012 and
BCM70015) drivers floating around the web.

Here are the ones I've found, roughly in order of most obsolete/broken to newest:

1. Staging driver from [Linux kernel v3.16 version](https://git.kernel.org/cgit/linux/kernel/git/gregkh/staging.git/tree/drivers/staging/crystalhd?h=v3.16)
  — this version was [removed in 2014 from v3.17](http://lkml.iu.edu/hypermail/linux/kernel/1408.0/01475.html) due to
  the fact that it was unmaintained and obsolete; it only supported the
  BCM70012 chip, for example.

  * See this LKML kernel thread from 2013 where Steven Newbury and Greg K-H discuss this code: https://lkml.org/lkml/2013/10/27/103

2. The [Debian version](http://anonscm.debian.org/cgit/pkg-multimedia/crystalhd.git),
   which appears to be based on a ~2010 version of the code from the
   mainline kernel, and like it only supports the BCM70012 chip.

3. Jarod Wilson's tree, last updated in 2012: http://git.linuxtv.org/cgit.cgi/jarod/crystalhd.git/

4. [**@yeradis**](http://github.com/yeradis)'s tree, forked from Jarod's tree, and last updated in 2013: https://github.com/yeradis/crystalhd

5. [**@dbason**](http://github.com/dbason)'s tree, forked from Yeradis's tree, and last updated in 2016: https://github.com/dbason/crystalhd

**Only the last version can be built and used without error on a modern kernel.** ([**@dlenski**](http://github.com/dlenski) is using it with kernel 4.4.0 and BCM70015.)


