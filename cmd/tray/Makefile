ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	witray
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES)

LIB = $(LIBS9) $(LIBIXP)
LIBS += -lm
OBJ = \
	client	\
	ewmh	\
	main	\
	selection	\
	tray	\
	xembed

include $(ROOT)/mk/one.mk

