ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

TARG =	wikeyname \
	wmii9menu

PACKAGES += $(X11PACKAGES)

LIB = $(LIBS9) $(LIBIXP)
LIBS += $(LIB)
CFLAGS += $(INCX11)

wiclick.out: wiclick.o
	$(LINK) $@ $< -lXtst

include $(ROOT)/mk/many.mk

