ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wistrut
HFILES=	dat.h fns.h

LIB =	${LIBIXP}
LDFLAGS += -lm ${LIBX11} -lXext -lXrandr -lregexp9 -lbio -lfmt -lutf
CFLAGS += ${INCX11} -DVERSION=\"${VERSION}\" \
	  -DIXP_NEEDAPI=86
OBJ =	main	\
	event	\
	ewmh	\
	win	\
	_util	\
	../wmii/map	\
	../wmii/printevent \
	printevent_kludge \
	../wmii/x11	\
	../util

include ${ROOT}/mk/one.mk

