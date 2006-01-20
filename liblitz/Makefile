# liblitz - non wimp GUI toolkit
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../libixp -I../libcext
LDFLAGS += -L../libixp -lixp -L../libcext -lcext

SRC = draw.c geometry.c kb.c mouse.c util.c
OBJ = ${SRC:.c=.o}

all: liblitz
	@echo built liblitz

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

liblitz: ${OBJ}
	@echo AR $@.a
	@${AR} $@.a ${OBJ}
	@${RANLIB} $@.a

clean:
	rm -f liblitz.a *.o
