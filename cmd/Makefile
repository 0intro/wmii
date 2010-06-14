ROOT=..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

wmiir.c: $(ROOT)/mk/wmii.mk

DIRS =	menu \
	strut \
	tray \
	wmii \
	x11
TARG =	wihack    \
	wmii.rc   \
	wmii.sh   \
	wmiir

LIBS += $(LIBS9) $(LIBIXP)

include $(ROOT)/mk/many.mk
include $(ROOT)/mk/dir.mk

