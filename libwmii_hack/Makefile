ROOT= ..
include ${ROOT}/mk/hdr.mk

hack.o hack.o_pic: util.c x11.c hack.h x11.h

CFLAGS += $(INCX11)

TARG =	libwmii_hack
OBJ =	hack
#	util \
#	x11

include ${ROOT}/mk/so.mk

