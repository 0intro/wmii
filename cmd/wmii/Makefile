ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wmii

LIB =	${LIBIXP}
EXLDFLAGS = -lm ${LIBX11} -lXext
EXCFLAGS = ${INCX11}
OBJ =	area	\
	bar	\
	client	\
	column	\
	draw	\
	event	\
	frame	\
	fs	\
	geom	\
	key	\
	main	\
	mouse	\
	rule	\
	printevent\
	view	\
	../util

include ${ROOT}/mk/one.mk
