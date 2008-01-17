# Customize below to fit your system

# paths
PREFIX = /usr/local
  BIN = $(PREFIX)/bin
  MAN = $(PREFIX)/share/man
  ETC = $(PREFIX)/etc
  LIBDIR = $(PREFIX)/lib
  INCLUDE = $(PREFIX)/include

# Includes and libs
INCPATH = .:$(ROOT)/include:$(INCLUDE):/usr/include
LIBS = -L/usr/lib -lc -L$(ROOT)/lib

# Flags
include $(ROOT)/mk/gcc.mk
CFLAGS += $(DEBUGCFLAGS)
LDFLAGS += -g $(LIBS)
SOLDFLAGS += $(LDFLAGS)
STATIC = -static

# Compiler, Linker. Linker should usually *not* be ld.
CC = cc -c
LD = cc
# Archiver
AR = ar crs

AWKPATH = $$(which awk)
P9PATHS = ${PLAN9}:"'$$(HOME)/plan9'":/usr/local/plan9:/usr/local/9:/opt/plan9:/opt/9:/usr/plan9:/usr/9

INCX11 = -I/usr/X11R6/include
LIBX11 = -L/usr/X11R6/lib -lX11
LIBICONV = # Leave blank if your libc includes iconv (glibc does)
LIBIXP = $(ROOT)/libixp/libixp.a
LIBIXP = $(LIBDIR)/libixp.a

# Operating System Configurations

# *BSD
#LIBICONV = -liconv
# +Darwin
#STATIC = # Darwon doesn't like static linking

# Solaris
#CFLAGS = -fast $(INCS)
#LDFLAGS = $(LIBS) -R$(PREFIX)/lib -lsocket -lnsl
#CFLAGS += -xtarget=ultra

