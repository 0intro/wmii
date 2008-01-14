ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h

LIB =	${LIBIXP}
LDFLAGS += -lm ${LIBX11} -lXext ${LIBICONV} -lregexp9 -lbio -lfmt -lutf
CFLAGS += ${INCX11} ${INCICONV} -DVERSION=\"${VERSION}\"
OBJ =	area	\
	bar	\
	client	\
	column	\
	event	\
	ewmh	\
	frame	\
	fs	\
	geom	\
	key	\
	main	\
	map	\
	message	\
	mouse	\
	rule	\
	printevent\
	utf	\
	view	\
	x11	\
	../util

include ${ROOT}/mk/one.mk

