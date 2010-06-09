ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG     = wmii
HFILES   = dat.h fns.h
TAGFILES = dat.h

PACKAGES += $(X11PACKAGES) xext xrandr xrender xinerama

LIB = $(LIBIXP) $(LIBS9)
LIBS += -lm

CFLAGS += $(INCICONV) -DIXP_NEEDAPI=97
OBJ =	area	\
	bar	\
	backtrace \
	client	\
	column	\
	div	\
	error	\
	event	\
	ewmh	\
	float	\
	frame	\
	fs	\
	key	\
	layout	\
	main	\
	message	\
	mouse	\
	print   \
	root	\
	rule	\
	screen	\
	stack	\
	utf	\
	view	\
	xdnd

include $(ROOT)/mk/one.mk

