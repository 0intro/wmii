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
	wmii9rc	  \
	wmiir

OFILES = util.o

LDFLAGS += -lfmt -lutf
CFLAGS += $(INCX11)

include $(ROOT)/mk/many.mk
include $(ROOT)/mk/dir.mk

OWMIIR=wmiir.o $(OFILES) $(LIBIXP)
wmiir.out: $(OWMIIR)
	$(LINK) $@ $(STATIC) $(OWMIIR)

wmii/x11.o wmii/map.o: dall
	true

O9MENU=wmii9menu.o clientutil.o wmii/x11.o wmii/map.o $(OFILES) $(LIBIXP)
wmii9menu.out: $(O9MENU)
	$(LINK) $@ $(O9MENU) $$(pkg-config --libs $(X11PACKAGES)) -lXext -lbio

