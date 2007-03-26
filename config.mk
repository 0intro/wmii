# Customize below to fit your system

# paths
PREFIX = /usr/local
BIN = ${PREFIX}/bin
MAN = ${PREFIX}/share/man
ETC = ${PREFIX}/etc
LIBDIR = ${PREFIX}/lib
INCLUDE = ${PREFIX}/include

# Includes and libs
INCS = -I. -I${ROOT}/include -I${INCLUDE} -I/usr/include
LIBS = -L/usr/lib -lc

# Flags
CFLAGS = -g -Wall ${INCS} -DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}

# Compiler
CC = cc -c
# Linker (Under normal circumstances, this should *not* be 'ld')
LD = cc
# Other
AR = ar cr
RANLIB = ranlib

AWKPATH = /usr/bin/awk
P9PATHS = /usr/local/plan9 /usr/local/9 /opt/plan9 /opt/9 /usr/plan9 /usr/9

INCX11 = -I/usr/X11R6/include
LIBX11 = -L/usr/X11R6/lib -lX11
LIBIXP = ${ROOT}/libixp/libixp.a

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS} -R${PREFIX}/lib
#LDFLAGS += -lsocket -lnsl
#CFLAGS += -xtarget=ultra
