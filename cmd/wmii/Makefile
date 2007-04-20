ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h

LIB =	${LIBIXP}
EXLDFLAGS = -lm ${LIBX11} -lXext ${LIBICONV}
EXCFLAGS = ${INCX11} ${INCICONV}
OBJ =	area	\
	bar	\
	client	\
	column	\
	event	\
	frame	\
	fs	\
	geom	\
	key	\
	main	\
	mouse	\
	rule	\
	printevent\
	utf	\
	view	\
	x11	\
	../util

include ${ROOT}/mk/one.mk
