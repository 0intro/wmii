ROOT=..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

wmiir.c: $(ROOT)/mk/wmii.mk

DIRS =	wmii
TARG =	wihack    \
	wmii.rc   \
	wmii.sh   \
	wmii9menu \
	wmii9rc	  \
	wmiir	  \
	wmiistartrc

OFILES = util.o
FILTER = sed "s|CONFPREFIX|$(ETC)|g; \
	      s|CONFVERSION|$(CONFVERSION)|g; \
	      s|P9PATHS|$(P9PATHS)|g; \
	      s|LIBDIR|$(LIBDIR)|g; \
	      s|BINSH|$(BINSH)|g; \
	      s|AWKPATH|$(AWKPATH)|g"

LDFLAGS += -lfmt -lutf
CFLAGS += $(INCX11) -DVERSION=\"$(VERSION)\"

include $(ROOT)/mk/many.mk
include $(ROOT)/mk/dir.mk

OWMIIR=wmiir.o $(OFILES) $(LIBIXP)
wmiir.O: $(OWMIIR)
	$(LINK) $@ $(STATIC) $(OWMIIR)

OMENU=menu.o wmii/x11.o $(OFILES) $(LIBIXP)
menu.O: $(OMENU) dall
	$(LINK) $@ $(OMENU)

wmii9menu.O: wmii9menu.o $(OFILES)
	$(LINK) $@ $*.o $(OFILES) $(LIBX11)

