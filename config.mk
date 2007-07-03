# Customize below to fit your system

# paths
PREFIX = /usr/local
BIN = ${PREFIX}/bin
MAN = ${PREFIX}/share/man
ETC = ${PREFIX}/etc
LIBDIR = ${PREFIX}/lib
INCLUDE = ${PREFIX}/include

# Includes and libs
INCPATH = .:${HOME}/libixp/include:${ROOT}/include:${INCLUDE}:/usr/include
LIBS = -L/usr/lib -lc -L${ROOT}/lib -L${HOME}/libixp/lib

# Flags
#include ${ROOT}/mk/gcc.mk
CFLAGS = -wF -DVARARGCK
LDFLAGS += ${LIBS}
STATIC = -static
MKDEP = cpp -M

# Compiler
CC = 8cc
# Linker (Under normal circumstances, this should *not* be 'ld')
LD = : 8l
# Archiver
AR = ar crs

AWKPATH = $$(which awk)
P9PATHS = ${PLAN9}:"'$${HOME}/plan9'":/usr/local/plan9:/usr/local/9:/opt/plan9:/opt/9:/usr/plan9:/usr/9

INCX11 = -I/usr/X11R6/include
LIBX11 = -L/usr/X11R6/lib -lX11
LIBICONV = # Leave blank if your libc includes iconv (glibc does)
LIBIXP = ${ROOT}/libixp/libixp.a
LIBIXP = ${LIBDIR}/libixp.a

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS} -R${PREFIX}/lib
#LDFLAGS += -lsocket -lnsl
#CFLAGS += -xtarget=ultra


