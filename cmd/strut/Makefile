ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	wistrut
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES) xext xrandr xinerama

LIB =	$(LIBIXP)
LDFLAGS += -lm $(LIBS9)
CFLAGS += -DIXP_NEEDAPI=86
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

