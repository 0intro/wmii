# Customize to fit your system

# paths
PREFIX = ${HOME}/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man
9PREFIX = ${PREFIX}/9

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

VERSION = 3-current

# includes and libs
LIBS = -L${PREFIX}/lib -L/usr/lib -lc -lm -L${X11LIB} -lX11

# flags
# Note: - under Solaris add -D__EXTENSIONS__ to CFLAGS
CFLAGS = -g -Wall -I. -I${PREFIX}/include -I/usr/include -I${X11INC} \
	-DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}

AR = ar cr
CC = cc
RANLIB = ranlib
