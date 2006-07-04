# window manager improved 2 utilities
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libcext
LDFLAGS += -L../libixp -lixp -L../libcext -lcext
X11LDFLAGS += -L../liblitz -llitz

X11SRC = wmiimenu.c wmiipsel.c wmiiwarp.c wmiikgrab.c
SRC = wmiir.c wmiisetsid.c
ALLSRC = ${SRC} ${X11SRC}

all: ${ALLSRC:.c=}
	@echo built wmii commands

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${SRC:.c=}: ${SRC:.c=.o}
	@echo LD $@
	@${CC} -o $@ $@.o ${LDFLAGS}

${X11SRC:.c=}: ${X11SRC:.c=.o}
	@echo LD $@
	@${CC} -o $@ $@.o ${X11LDFLAGS}

# Solaris
#	@${CC} -o $* $*.o ${LDFLAGS} -lsocket

clean:
	rm -f ${ALLSRC:.c=} *.o
