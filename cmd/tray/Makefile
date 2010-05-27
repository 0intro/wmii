ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	witray
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES)

LIBS += -lm $(LIBS9) $(LIBIXP)
OBJ = \
	client	\
	ewmh	\
	main	\
	selection	\
	tray	\
	xembed

include $(ROOT)/mk/one.mk

