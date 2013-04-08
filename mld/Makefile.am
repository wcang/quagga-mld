
INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

sbin_PROGRAMS = mld
noinst_HEADERS = mld.h
mld_SOURCES = mld.c mld_zebra.c

mld_LDADD = ../lib/libzebra.la @LIBCAP@ 
