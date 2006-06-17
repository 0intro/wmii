# libixp - lib ixp protocol
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I ../libcext

SRC = client.c convert.c message.c server.c socket.c transport.c intmap.c request.c

OBJ = ${SRC:.c=.o}

all: libixp.a
	@echo built libixp

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

libixp.a: ${OBJ}
	@echo AR $@
	@${AR} $@ ${OBJ}
	@${RANLIB} $@

clean:
	rm -f libixp.a *.o
