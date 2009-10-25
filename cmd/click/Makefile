ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	click
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES) xext xrandr xrender xinerama

LIB =	$(LIBIXP)
LDFLAGS += -lm -lXtst $(LIBS9)
CFLAGS += -DVERSION=\"$(VERSION)\" -DIXP_NEEDAPI=86
OBJ =	main	\
	_util	\
	../wmii/map	\
	../wmii/x11	\
	../util

include ${ROOT}/mk/one.mk

