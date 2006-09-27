# Customize to fit your system

# paths
PREFIX = /usr/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

VERSION = 3.5

# includes and libs
LIBS = -L${PREFIX}/lib -L/usr/lib -lc
X11LIBS = -L${X11LIB} -lX11

# Linux/BSD
CFLAGS = -g -Wall -I. -I${PREFIX}/include -I/usr/include -I${X11INC} \
	-DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}
X11LDFLAGS = ${LDFLAGS} ${X11LIBS}

# Solaris
#CFLAGS = -fast -xtarget=ultra ${INCLUDES} -DVERSION=\"${VERSION}\"
#LIBS += -lnsl -lsocket

AR = ar cr
CC = cc
RANLIB = ranlib
