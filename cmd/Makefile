# window manager improved 2 utilities
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libwmii -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp \
	-L../libwmii -lwmii -L../libcext -lcext
LDFLAGS2 += ${LIBS} -L../liblitz -llitz -L../libixp2 -lixp \
	-L../libwmii -lwmii -L../libcext -lcext

SRC_bar = wmiibar.c
OBJ_bar = ${SRC_bar:.c=.o}

SRC_menu = wmiimenu.c
OBJ_menu = ${SRC_menu:.c=.o}

SRC_r2 = wmiir2.c
OBJ_r2 = ${SRC_r2:.c=.o}

SRC_r = wmiir.c
OBJ_r = ${SRC_r:.c=.o}

SRC_fs = wmiifs.c
OBJ_fs = ${SRC_fs:.c=.o}

SRC_keys = wmiikeys.c
OBJ_keys = ${SRC_keys:.c=.o}

SRC_plumb = wmiiplumb.c
OBJ_plumb = ${SRC_plumb:.c=.o}

SRC_warp = wmiiwarp.c
OBJ_warp = ${SRC_warp:.c=.o}

all: wmiibar wmiibar2 wmiimenu wmiir wmiir2 wmiifs wmiikeys wmiiplumb wmiiwarp
	@echo built wmii commands

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

wmiibar: ${OBJ_bar}
	@echo LD $@
	@${CC} -o $@ ${OBJ_bar} ${LDFLAGS2}
# Solaris
#	@${CC} -o $@ ${OBJ_bar2} ${LDFLAGS2} -lsocket

wmiimenu: ${OBJ_menu}
	@echo LD $@
	@${CC} -o $@ ${OBJ_menu} ${LDFLAGS}
# Solaris
#	@${CC} -o $@ ${OBJ_menu} ${LDFLAGS} -lsocket

wmiir2: ${OBJ_r2}
	@echo LD $@
	@${CC} -o $@ ${OBJ_r2} -g -static -L${PREFIX}/lib -L/usr/lib -lc \
		-L../libixp2 -lixp -L../libcext -lcext
# Solaris
#	@${CC} -o $@ ${OBJ_r2} -g -static -L${PREFIX}/lib -L/usr/lib -lc \
#		-L../libixp2 -lixp -L../libcext -lcext -lsocket

wmiir: ${OBJ_r}
	@echo LD $@
	@${CC} -o $@ ${OBJ_r} -g -static -L${PREFIX}/lib -L/usr/lib -lc \
		-L../libixp -lixp -L../libcext -lcext
# Solaris
#	@${CC} -o $@ ${OBJ_r} -g -static -L${PREFIX}/lib -L/usr/lib -lc \
#		-L../libixp -lixp -L../libcext -lcext -lsocket

wmiifs: ${OBJ_fs}
	@echo LD $@
	@${CC} -o $@ ${OBJ_fs} ${LDFLAGS}
# Solaris
#	@${CC} -o $@ ${OBJ_fs} ${LDFLAGS} -lsocket

wmiikeys: ${OBJ_keys}
	@echo LD $@
	@${CC} -o $@ ${OBJ_keys} ${LDFLAGS}
# Solaris
#	@${CC} -o $@ ${OBJ_keys} ${LDFLAGS} -lsocket

wmiiplumb: ${OBJ_plumb}
	@echo LD $@
	@${CC} -o $@ ${OBJ_plumb} ${LDFLAGS}

wmiiwarp: ${OBJ_warp}
	@echo LD $@
	@${CC} -o $@ ${OBJ_warp} ${LDFLAGS}

clean:
	rm -f wmiibar wmiibar2 wmiimenu wmiir wmiir2 wmiikeys wmiiplumb wmiifs\
		wmiiwarp *.o
