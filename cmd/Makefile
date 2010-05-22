ROOT=..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

wmiir.c: $(ROOT)/mk/wmii.mk

DIRS =	wmii \
	menu
TARG =	wihack    \
	wmii.rc   \
	wmii.sh   \
	wmii9menu \
	wmiir

LIBS += $(LIBS9)
CFLAGS += $(INCX11)

include $(ROOT)/mk/many.mk
include $(ROOT)/mk/dir.mk

OWMIIR=wmiir.o $(OFILES) $(LIBIXP)
wmiir.out: $(OWMIIR)
	$(LINK) $@ $(STATIC) $(OWMIIR)

O9MENU=wmii9menu.o $(ROOT)/lib/libstuff.a $(LIBIXP)
wmii9menu.out: $(O9MENU)
	$(LINK) $@ $(O9MENU) $$(pkg-config --libs $(X11PACKAGES) xrandr xinerama) -lXext

