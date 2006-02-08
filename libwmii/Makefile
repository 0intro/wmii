# libwmii - mediator lib for wmii
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp -L../libcext -lcext

SRC = ixp.c signal.c spawn.c wm.c

OBJ = ${SRC:.c=.o}

all: libwmii
	@echo built libwmii

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

libwmii: ${OBJ}
	@echo AR $@.a
	@${AR} $@.a ${OBJ}
	@${RANLIB} $@.a

clean:
	rm -f libwmii.a *.o
