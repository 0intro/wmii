# wmii version
VERSION != hg tip | awk -F '[: ]+' '/^changeset:/ { print "hg" $$2 }'
CONFVERSION = 3.5

# Customize below to fit your system

# paths
PREFIX = /usr/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I${PREFIX}/include -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -lm -L${X11LIB} -lX11 -L${PREFIX}/lib -lixp

# flags
#CFLAGS = -Os ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}
CFLAGS = -g -Wall ${INCS} -DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS} -R${PREFIX}/lib
#LDFLAGS += -lsocket -lnsl
#CFLAGS += -xtarget=ultra

# compiler and linker
CC = cc
