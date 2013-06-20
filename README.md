# Crystal HD Hardware Decoder Driver on Ubuntu 13.04 Linux kernel 3.8.0-25
## Broadcom BCM70012 & BCM70015

After a lot a retries to get the rigth experience with the Crystal HD on Ubuntu, 

**1. Install required files**

    sudo apt-get install checkinstall git-core autoconf build-essential subversion dpkg-dev fakeroot pbuilder build-essential dh-make debhelper devscripts patchutils quilt git-buildpackage pristine-tar git yasm zlib1g-dev zlib-bin libzip-dev libx11-dev libx11-dev libxv-dev vstream-client-dev libgtk2.0-dev libpulse-dev libxxf86dga-dev x11proto-xf86dga-dev git libgstreamermm-0.10-dev libgstreamer0.10-dev automake libtool python-appindicator 
    
**2. Ge the source**

Get the driver source code from the git repository.

    git clone git://git.linuxtv.org/jarod/crystalhd.git   
    
**3. Compile driver, install libraries, and load driver**

Use make command to compile driver. If you have multiple core processor then use the “-j2″ or “-j4″ option (2 or 4 is the number of cores). This will speed up the make process.

    cd crystalhd/driver/linux
    autoconf
    ./configure
    make -j2
    sudo make install
    
**4. Install the libraries.**

    cd ../../linux_lib/libcrystalhd/
    make -j2
    sudo make install 
    
**5. Load the driver.**

    sudo modprobe crystalhd
    
**6. Reboot your system** , then check if 'crystalhd' is listed in the output of the following commands.

    lsmod
    dmesg | grep crystalhd
    
 Then you should see something like this:
 
    [    4.349765] Loading crystalhd v3.10.0
    [    4.349823] crystalhd 0000:02:00.0: Starting Device:0x1615
    [    4.351848] crystalhd 0000:02:00.0: irq 43 for MSI/MSI-X
    [  108.512135] crystalhd 0000:02:00.0: Opening new user[0] handle
    [  258.976583] crystalhd 0000:02:00.0: Closing user[0] handle via ioctl with mode 10200

Now is time to enjoy our FullHD content. 

I'm using XMBC , VLC (2.1.0), Mplayer2, GStreamer because they are using (they should) the Crystal HD decoder libraries.

For example , lets try VLC :

    vlc --codec=crystalhd ourgreatfullhdmedia.mkv
    
Now runs smoothly rigth ?

# After kernel update

Reinstall the driver.

    cd crystalhd/driver/linux
    sudo make install


Btw this instructions referred to http://knowledge.evot.biz/documentation/how-to-compile-and-install-the-broadcom-crystal-hd-hardware-decoder-bcm70012-70015-driver-on-ubuntu and fixed some issues appeared using a patch from M25 user at https://bbs.archlinux.org/viewtopic.php?pid=1253622#p1253622

So, the sources on this repository are updated with the fixes and patches in order to make your life easier.

