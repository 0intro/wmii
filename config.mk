# Customize below to fit your system

# Paths
PREFIX = /usr/local
  BIN = $(PREFIX)/bin
  MAN = $(PREFIX)/share/man
  DOC = $(PREFIX)/share/doc/wmii
  ETC = $(PREFIX)/etc
  LIBDIR = $(PREFIX)/lib
  INCLUDE = $(PREFIX)/include

# Includes and libs
INCPATH = .:$(ROOT)/include:$(INCLUDE):/usr/include
LIBS = -L$(ROOT)/lib -L/usr/lib

# Flags
include $(ROOT)/mk/gcc.mk
CFLAGS += $(DEBUGCFLAGS) -O0
LDFLAGS += -g $(LIBS)
SOLDFLAGS += $(LDFLAGS)
SHARED = -shared -Wl,-soname=$(SONAME)
STATIC = -static

# Compiler, Linker. Linker should usually *not* be ld.
CC = cc -c
LD = cc
# Archiver
AR = ar crs

AWKPATH = $$(which awk)
P9PATHS = ${PLAN9}:"'$${HOME}/plan9'":/usr/local/plan9:/usr/local/9:/opt/plan9:/opt/9:/usr/plan9:/usr/9
# Your make shell. By default, the first found of /bin/dash, /bin/ksh,
# /bin/sh. Except with bsdmake, which assumes /bin/sh is sane. bash and zsh
# are painfully slow, and should be avoided.
#BINSH = /bin/ash

X11PACKAGES = xft
INCX11 = $$(pkg-config --cflags $(X11PACKAGES))
LIBICONV = # Leave blank if your libc includes iconv (glibc does)
LIBIXP = $(LIBDIR)/libixp.a

# Operating System Configurations

# KenCC
# Note: wmii *must* always compile under KenCC. It's vital for
# argument checking in formatted IO, and similar diagnostics.
#CFLAGS = -wF
#STATIC = # Implied
#CC=pcc -c
#LD=pcc

# *BSD
#LIBICONV = -L/usr/local/lib -liconv
# +Darwin
#STATIC = # Darwin doesn't like static linking
#SHARED = -dynamiclib
#SOEXT = dylib

# Solaris
#CFLAGS = -fast $(INCS)
#LDFLAGS = $(LIBS) -R$(PREFIX)/lib -lsocket -lnsl
#CFLAGS += -xtarget=ultra

