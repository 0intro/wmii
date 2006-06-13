# liblitz - non wimp GUI toolkit
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I../libixp -I../libcext
LDFLAGS += -L../libixp -lixp -L../libcext -lcext

SRC = blitz.c color.c font.c label.c layout.c window.c
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
