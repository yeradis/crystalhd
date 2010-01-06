#
#  Broadcom "BCM970012" Crystal HD device interface library.
#
#
BCLIB_MINOR=0
BCLIB_MAJOR=1
BCLIB_NAME=libcrystalhd.so
BCLIB_SL=$(BCLIB_NAME).$(BCLIB_MAJOR)
BCLIB=$(BCLIB_NAME).$(BCLIB_MAJOR).$(BCLIB_MINOR)
LIBDIR=/usr/lib

AT   = @
ECHO = ${AT} echo
BCGCC = g++

ROOTDIR = ../..

INCLUDES = -I./ -I/usr/include -I$(ROOTDIR)/include
INCLUDES += -I$(ROOTDIR)/include/link


CPPFLAGS = -D__LINUX_USER__
# -D_USE_SHMEM_

CPPFLAGS += ${INCLUDES}
CPPFLAGS += -g -Wall -fPIC -shared
LDFLAGS = -Wl,-soname,${BCLIB_SL}

SRCFILES = 	libcrystalhd_if.cpp \
		libcrystalhd_int_if.cpp \
		libcrystalhd_fwcmds.cpp \
		libcrystalhd_priv.cpp \
		libcrystalhd_fwdiag_if.cpp

OBJFILES = ${SRCFILES:.cpp=.o}

all:help $(OBJFILES)
	$(BCGCC) $(CPPFLAGS) $(LDFLAGS) -o $(BCLIB) ${OBJFILES}
	ln -sf $(BCLIB) $(BCLIB_NAME)
	ln -sf $(BCLIB) $(BCLIB_SL)

help:
	${ECHO} OBJFILES = ${OBJFILES}
	${ECHO} SRCFILES = ${SRCFILES}
	${ECHO} LNM = ${BCLIB} ${BCLIB_SL}

clean:
	rm -f  ${BCLIB} ${BCLIB_SL} ${BCLIB_NAME} *.o
	rm -f  ${OBJFILES}

install:
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)/lib/firmware
	mkdir -p $(DESTDIR)/usr/include/libcrystalhd
	cp libcrystalhd_if.h $(DESTDIR)/usr/include/libcrystalhd/
	cp $(ROOTDIR)/include/bc_dts_defs.h $(DESTDIR)/usr/include/libcrystalhd/
	cp $(ROOTDIR)/include/bc_dts_types.h $(DESTDIR)/usr/include/libcrystalhd/
	cp $(ROOTDIR)/firmware/fwbin/70012/bcm70012fw.bin $(DESTDIR)/lib/firmware/
	install -m 755 $(BCLIB) $(DESTDIR)$(LIBDIR)
	(cd $(DESTDIR)$(LIBDIR); ln -sf $(BCLIB) $(BCLIB_NAME))
	(cd $(DESTDIR)$(LIBDIR); ln -sf $(BCLIB) $(BCLIB_SL))

