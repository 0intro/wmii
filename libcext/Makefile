# libcext - c extensions for wmii project
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../config.mk

SRC = assert.c emallocz.c estrdup.c strlcat.c strlcpy.c tokenize.c trim.c vector.c

OBJ = ${SRC:.c=.o}

.PREFIXES = .c .o

all: libcext.a
	@echo built libcext

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

$(OBJ): cext.h

libcext.a: ${OBJ}
	@echo AR $@
	@${AR} $@ ${OBJ}
	@${RANLIB} $@

clean:
	rm -f libcext.a *.o
