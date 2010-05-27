ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	wistrut
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES)

LIB = $(LIBS9)
LIBS += -lm
CFLAGS += -DIXP_NEEDAPI=86
OBJ =	main	\
	ewmh	\
	win

include $(ROOT)/mk/one.mk

