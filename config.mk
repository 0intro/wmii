# Customize to fit your system

# paths
PREFIX = /usr/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man
9PREFIX = ${PREFIX}/9

INCDIR = ${PREFIX}/include
LIBDIR = ${PREFIX}/lib
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCLUDES = -I. -I${INCDIR} -I/usr/include -I${X11INC}
LIBS = -L${LIBDIR} -L/usr/lib -lc -lm -L${X11LIB} -lX11
VERSION = 20051214_experimental

# flags
CFLAGS = -O0 -g -Wall ${INCLUDES} -DVERSION=\"${VERSION}\"
#CFLAGS = -O0 -g -Wall -W ${INCLUDES} -DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}

# compiler
# Note: - under Solaris add -D__EXTENSIONS__ to CFLAGS
MAKE = make
AR = ar cr
CC = cc
RANLIB = ranlib
