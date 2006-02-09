# window manager improved 2 utilities
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp -L../libcext -lcext

SRC_menu = wmiimenu.c
OBJ_menu = ${SRC_menu:.c=.o}

SRC_r = wmiir.c
OBJ_r = ${SRC_r:.c=.o}

SRC_plumb = wmiiplumb.c
OBJ_plumb = ${SRC_plumb:.c=.o}

SRC_warp = wmiiwarp.c
OBJ_warp = ${SRC_warp:.c=.o}

#all: wmiimenu wmiir wmiiplumb wmiiwarp
all: wmiir wmiiplumb wmiiwarp
	@echo built wmii commands

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

wmiimenu: ${OBJ_menu}
	@echo LD $@
	@${CC} -o $@ ${OBJ_menu} ${LDFLAGS}
# Solaris
#	@${CC} -o $@ ${OBJ_menu} ${LDFLAGS} -lsocket

wmiir: ${OBJ_r}
	@echo LD $@
	@${CC} -o $@ ${OBJ_r} -g -L${PREFIX}/lib -L/usr/lib -lc \
		-L../libixp -lixp -L../libcext -lcext
# Solaris
#	@${CC} -o $@ ${OBJ_r} -g -static -L${PREFIX}/lib -L/usr/lib -lc \
#		-L../libixp -lixp -L../libcext -lcext -lsocket

wmiiplumb: ${OBJ_plumb}
	@echo LD $@
	@${CC} -o $@ ${OBJ_plumb} ${LDFLAGS}

wmiiwarp: ${OBJ_warp}
	@echo LD $@
	@${CC} -o $@ ${OBJ_warp} ${LDFLAGS}

clean:
	rm -f wmiimenu wmiir wmiiplumb wmiiwarp *.o
