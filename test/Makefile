ROOT=..
include $(ROOT)/mk/hdr.mk

TARG =	grav

OFILES = ../cmd/util.o     \
	 ../cmd/wmii/map.o \
	 ../cmd/wmii/x11.o 

LDFLAGS += $(OFILES) -lfmt -lutf -lbio $(LIBX11) -lXext
CFLAGS += $(INCX11)

include $(ROOT)/mk/many.mk

