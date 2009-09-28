ROOT=..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

TARG =	wmii.1	   \
	wmiir.1    \
	wimenu.1

wmiir.1:
	true

$(TARG): Makefile $(ROOT)/mk/wmii.mk header.t2t

FILTER = sed "s|CONFPREFIX|$(ETC)|g; \
	      s|CONFVERSION|$(CONFVERSION)|g; \
	      s|P9PATHS|$(P9PATHS)|g; \
	      s|AWKPATH|$(AWKPATH)|g; \
	      s|VERSION|$(VERSION)|g"

include ${ROOT}/mk/man.mk

