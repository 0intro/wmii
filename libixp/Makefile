# libixp - lib ixp protocol
#   (C)opyright MMIV-MMV Anselm R. Garbe

include ../config.mk

CFLAGS += -I ../libcext
LDFLAGS += -L../libcext -lcext

SRC = client.c message.c ramfs.c server.c

OBJ = ${SRC:.c=.o}

all: libixp
	@echo built libixp

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

libixp: ${OBJ}
	@echo AR $@.a
	@${AR} $@.a ${OBJ}
	@${RANLIB} $@.a

clean:
	rm -f libixp.a *.o
