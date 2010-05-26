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
INCLUDES = -I. -I$(ROOT)/include -I$(INCLUDE) -I/usr/include
LIBS = -L$(ROOT)/lib -L/usr/lib

TERMINAL = xterm

# Flags
include $(ROOT)/mk/gcc.mk
CFLAGS += $(DEBUGCFLAGS) -O0
LDFLAGS += -g

# Compiler, Linker. Linker should usually *not* be ld.
CC = cc -c
LD = cc
# Archiver
AR = ar crs

X11PACKAGES = x11 xinerama xrender
INCX11 = $$(pkg-config --cflags $(X11PACKAGES))
LIBIXP = $(LIBDIR)/libixp.a

# Enable RTLD. Only necessary for Xft support.
CFLAGS += -DHAVE_RTLD
LDFLAGS += -ldl # Comment this out on BSD systems.

SOLDFLAGS += $(LDFLAGS)
SHARED = -shared -Wl,-soname=$(SONAME)
STATIC = -static

# Your make shell. By default, the first found of /bin/dash, /bin/ksh,
# /bin/sh. Except with bsdmake, which assumes /bin/sh is sane. bash and zsh
# are painfully slow, and should be avoided.
#BINSH = /bin/ash

## Operating System Configurations

# KenCC
# Note: wmii *must* always compile under KenCC. It's vital for
# argument checking in formatted IO, and similar diagnostics.
#CFLAGS = -wF
#STATIC = # Implied
#CC=pcc -c
#LD=pcc

# Darwin
#STATIC = # Darwin doesn't like static linking
#SHARED = -dynamiclib
#SOEXT = dylib

# Solaris
#SHELL = /bin/bash
#CFLAGS = -fast $(INCS)
#LDFLAGS = $(LIBS) -R$(PREFIX)/lib -lsocket -lnsl
#CFLAGS += -xtarget=ultra

