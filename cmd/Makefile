# window manager improved 2 utilities
#   (C)opyright MMIV-MMV Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libwmii -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp \
	-L../libwmii -lwmii -L../libcext -lcext
LDFLAGS2 += ${LIBS} -L../liblitz -llitz -L../libixp2 -lixp \
	-L../libwmii -lwmii -L../libcext -lcext

SRC_bar = wmibar.c
OBJ_bar = ${SRC_bar:.c=.o}

SRC_bar2 = wmibar2.c
OBJ_bar2 = ${SRC_bar2:.c=.o}

SRC_menu = wmimenu.c
OBJ_menu = ${SRC_menu:.c=.o}

SRC_r2 = wmir2.c
OBJ_r2 = ${SRC_r2:.c=.o}

SRC_r = wmir.c
OBJ_r = ${SRC_r:.c=.o}

SRC_fs = wmifs.c
OBJ_fs = ${SRC_fs:.c=.o}

SRC_keys = wmikeys.c
OBJ_keys = ${SRC_keys:.c=.o}

SRC_plumb = wmiplumb.c
OBJ_plumb = ${SRC_plumb:.c=.o}

SRC_warp = wmiwarp.c
OBJ_warp = ${SRC_warp:.c=.o}

all: wmibar wmibar2 wmimenu wmir wmir2 wmifs wmikeys wmiplumb wmiwarp
	@echo built wmi commands

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

wmibar: ${OBJ_bar}
	@echo LD $@
	@${CC} -o $@ ${OBJ_bar} ${LDFLAGS}

wmibar2: ${OBJ_bar2}
	@echo LD $@
	@${CC} -o $@ ${OBJ_bar2} ${LDFLAGS2}

wmimenu: ${OBJ_menu}
	@echo LD $@
	@${CC} -o $@ ${OBJ_menu} ${LDFLAGS}

wmir2: ${OBJ_r2}
	@echo LD $@
	@${CC} -o $@ ${OBJ_r2} -g -static -L${LIBDIR} -L/usr/lib -lc \
		-L../libixp2 -lixp -L../libcext -lcext

wmir: ${OBJ_r}
	@echo LD $@
	@${CC} -o $@ ${OBJ_r} -g -static -L${LIBDIR} -L/usr/lib -lc \
		-L../libixp -lixp -L../libcext -lcext

wmifs: ${OBJ_fs}
	@echo LD $@
	@${CC} -o $@ ${OBJ_fs} ${LDFLAGS}

wmikeys: ${OBJ_keys}
	@echo LD $@
	@${CC} -o $@ ${OBJ_keys} ${LDFLAGS}

wmiplumb: ${OBJ_plumb}
	@echo LD $@
	@${CC} -o $@ ${OBJ_plumb} ${LDFLAGS}

wmiwarp: ${OBJ_warp}
	@echo LD $@
	@${CC} -o $@ ${OBJ_warp} ${LDFLAGS}

clean:
	rm -f wmibar wmibar2 wmimenu wmir wmir2 wmikeys wmiplumb wmifs\
		wmiwarp *.o
