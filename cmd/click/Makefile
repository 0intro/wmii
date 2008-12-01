ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	click
HFILES=	dat.h fns.h

LIB =	${LIBIXP}
LDFLAGS += -lm ${LIBX11} -lXext -lXtst -lregexp9 -lbio -lfmt -lutf
CFLAGS += ${INCX11} -DVERSION=\"${VERSION}\" \
	  -DIXP_NEEDAPI=86
OBJ =	main	\
	_util	\
	../wmii/map	\
	../wmii/x11	\
	../util

include ${ROOT}/mk/one.mk

