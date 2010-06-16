ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	wistrut
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES)

LIB = $(LIBS9)
LIBS += -lm
OBJ =	main	\
	ewmh	\
	win

include $(ROOT)/mk/one.mk

