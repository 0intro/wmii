# liblitz - non wimp GUI toolkit
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../libixp -I../libcext
LDFLAGS += -L../libixp -lixp -L../libcext -lcext

SRC = brush.c color.c draw.c font.c
OBJ = ${SRC:.c=.o}

all: liblitz.a
	@echo built liblitz

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

liblitz.a: ${OBJ}
	@echo AR $@
	@${AR} $@ ${OBJ}
	@${RANLIB} $@

clean:
	rm -f liblitz.a *.o
