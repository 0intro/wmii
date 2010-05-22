ROOT= ../..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

main.c: ${ROOT}/mk/wmii.mk

TARG =	click
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES) xext xrandr xrender xinerama

LIB =	$(LIBIXP)
LIBS += -lm -lXtst $(LIBS9)
CFLAGS += -DIXP_NEEDAPI=86
OBJ =	main

include ${ROOT}/mk/one.mk

