ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

TARG =	wmii
HFILES=	dat.h fns.h

LIB =	$(LIBIXP)
LDFLAGS += -lm $(LIBX11) -lXext -lXrandr -lXrender -lXinerama \
	   $(LIBICONV) -lregexp9 -lbio -lfmt -lutf

CFLAGS += $(INCX11) $(INCICONV) -DVERSION=\"$(VERSION)\" \
	  -DIXP_NEEDAPI=97
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

