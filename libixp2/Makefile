# libixp - lib ixp protocol
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

CFLAGS += -I ../libcext

SRC = client.c convert.c message.c server.c socket.c transport.c

OBJ = ${SRC:.c=.o}

all: libixp
	@echo built libixp2

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

libixp: ${OBJ}
	@echo AR $@.a
	@${AR} $@.a ${OBJ}
	@${RANLIB} $@.a

clean:
	rm -f libixp.a *.o
