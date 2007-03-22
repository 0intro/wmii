# wmii version
VERSION = $$(hg tip --template 'hg{rev}' 2>/dev/null)
CONFVERSION = 3.5

# Customize below to fit your system

# paths
PREFIX = /usr/local
CONFPREFIX = ${PREFIX}/etc
MANPREFIX = ${PREFIX}/share/man
AWKPATH = /usr/bin/awk
P9PATHS = /usr/local/plan9 /usr/local/9 /opt/plan9 /opt/9 /usr/plan9 /usr/9

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I${PREFIX}/include -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -lm -L${X11LIB} -lX11
LIBIXP = -L${PREFIX}/lib -lixp

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
