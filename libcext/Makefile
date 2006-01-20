# libcext - c extensions for wmii project
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

SRC = emallocz.c estrdup.c strlcat.c strlcpy.c strtonum.c tokenize.c

OBJ = ${SRC:.c=.o}

.PREFIXES = .c .o

all: libcext
	@echo built libcext

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

libcext: ${OBJ}
	@echo AR $@.a
	@${AR} $@.a ${OBJ}
	@${RANLIB} $@.a

clean:
	rm -f libcext.a *.o
