# window manager improved 2 utilities
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp -L../libcext -lcext

SRC = wmiimenu.c wmiipsel.c wmiir.c wmiisetsid.c wmiiwarp.c
OBJ = ${SRC:.c=.o}

all: ${OBJ}
	@echo built wmii commands

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<
	@echo LD $*
	@${CC} -o $* $*.o ${LDFLAGS}

# Solaris
#	@${CC} -o $* $*.o ${LDFLAGS} -lsocket

clean:
	rm -f wmiimenu wmiir wmiipsel wmiiwarp *.o
