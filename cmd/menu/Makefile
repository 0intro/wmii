ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	wimenu
HFILES=	dat.h fns.h

LIB =	$(LIBIXP)
LDFLAGS += -lm $(LIBX11) -lXext -lXrandr -lXinerama \
	   -lregexp9 -lbio -lfmt -lutf
CFLAGS += $(INCX11) -DVERSION=\"$(VERSION)\" \
	  -DIXP_NEEDAPI=86
OBJ =	main	\
	event	\
	menu	\
	../wmii/geom	\
	../wmii/map	\
	../wmii/printevent	\
	../wmii/x11	\
	../wmii/xext	\
	../util

include $(ROOT)/mk/one.mk

