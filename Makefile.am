AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I build-aux
SUBDIRS=scripts doc test

noinst_LTLIBRARIES = libcommunicate.la
libcommunicate_la_SOURCES = communicate.c

lib_LTLIBRARIES=libfakeroot.la
libfakeroot_la_SOURCES=libfakeroot.c statconv/glibc/linux/alpha/stats.h wrapdef.h  wrapstruct.h communicate.h
libfakeroot_la_LDFLAGS=-release 0
libfakeroot_la_DEPENDENCIES=wrapdef.h wrapstruct.h libcommunicate.la $(LTLIBOBJS)
libfakeroot_la_LIBADD = libcommunicate.la $(LTLIBOBJS)

if FAKEROOT_SQLITE
faked_sqlite_SOURCES = faked.c
faked_sqlite_CPPFLAGS = -DFAKEROOT_SQLITE $(SQLITE_CFLAGS)
faked_sqlite_LDADD = libcommunicate.la
faked_sqlite_LDFLAGS = $(SQLITE_LIBS)
FAKED_SQLITE = faked-sqlite
endif

bin_PROGRAMS = faked $(FAKED_SQLITE)
faked_SOURCES = faked.c
faked_LDADD = libcommunicate.la

simple_SOURCES=simple.c
noinst_PROGRAMS=simple

CLEANFILES= wrapdef.h wrapstruct.h wrapped.h wraptmpf.h
DISTCLEANFILES = fakerootconfig.h

EXTRA_DIST=wrapawk wrapfunc.inp                        \
           debian/rules debian/changelog debian/control  \
	   message.h \
	   DEBUG BUGS \
	   setenv.c

CLEAN_FILES=fakerootconfig.h

wrapped.h wrapdef.h wrapstruct.h wraptmpf.h:wrapawk wrapfunc.inp
	awk -f $(srcdir)/wrapawk < $(srcdir)/wrapfunc.inp

libfakeroot.lo:libfakeroot.c wrapdef.h wrapstruct.h wraptmpf.h

fakerootconfig.h: ./config.status
	CONFIG_FILES= CONFIG_HEADERS= /bin/sh ./config.status


#Local variables:
#mode: makefile
#End:
