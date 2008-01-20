ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h

LIB =	${LIBIXP}
LDFLAGS += -lm ${LIBX11} -lXext -lXrandr ${LIBICONV} -lregexp9 -lbio -lfmt -lutf
CFLAGS += ${INCX11} ${INCICONV} -DVERSION=\"${VERSION}\" \
	  -DIXP_NEEDAPI=86
OBJ =	area	\
	bar	\
	client	\
	column	\
	div	\
	event	\
	ewmh	\
	float	\
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
	xext	\
	../util

include ${ROOT}/mk/one.mk

