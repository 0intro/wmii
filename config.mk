# Customize to fit your system

# paths
PREFIX = /usr/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

VERSION = 3-current

# includes and libs
LIBS = -L${PREFIX}/lib -L/usr/lib -lc -lm -L${X11LIB} -lX11

# Linux/BSD
CFLAGS = -ggdb -Wall -I. -I${PREFIX}/include -I/usr/include -I${X11INC} \
	-DVERSION=\"${VERSION}\"
LDFLAGS = -ggdb ${LIBS}

# Solaris
#CFLAGS = -fast -xtarget=ultra ${INCLUDES} -DVERSION=\"${VERSION}\"
# Note: - under Solaris add -D__EXTENSIONS__ to CFLAGS

AR = ar cr
CC = cc
RANLIB = ranlib
