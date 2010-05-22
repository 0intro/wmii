ROOT= ..
include ${ROOT}/mk/hdr.mk

hack.o hack.o_pic: util.c hack.h x11.h

CFLAGS += $(INCX11)
SOLDFLAGS += $(LIBX11)

TARG =	libwmii_hack
# Can't just link libstuff here. We need PIC objects.
OBJ =	hack	\
	../libstuff/util/getbase		\
	../libstuff/util/getlong		\
	../libstuff/util/tokenize

include ${ROOT}/mk/so.mk

