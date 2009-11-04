ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES) xext xrandr xrender xinerama

LIB =	$(LIBIXP)
LIBS += -lm $(LIBS9)

CFLAGS += $(INCICONV) -DIXP_NEEDAPI=97
OBJ =	area	\
	bar	\
	client	\
	column	\
	div	\
	event	\
	ewmh	\
	float	\
	frame	\
	fs	\
	geom	\
	key	\
	layout	\
	main	\
	map	\
	message	\
	mouse	\
	print   \
	root	\
	rule	\
	printevent\
	screen	\
	utf	\
	_util	\
	view	\
	xdnd	\
	x11	\
	xext	\
	../util

include $(ROOT)/mk/one.mk

