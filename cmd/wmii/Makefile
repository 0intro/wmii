ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h x11.h

LIB =	${LIBIXP}
LDFLAGS += -lm ${LIBX11} -lXext ${LIBICONV} -lregexp9 -lbio -lfmt -lutf
CFLAGS += ${INCX11} ${INCICONV}
OBJ =	area	\
	bar	\
	client	\
	column	\
	event	\
	frame	\
	fs		\
	geom	\
	key		\
	main		\
	map		\
	message	\
	mouse	\
	rule		\
	printevent\
	utf		\
	view		\
	x11		\
	../util

include ${ROOT}/mk/one.mk

