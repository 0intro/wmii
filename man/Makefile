ROOT=..
include ${ROOT}/mk/hdr.mk

TARG =	wmii.1	   \
	wmiiloop.1 \
	wmiir.1

FILTER = sed "s|CONFPREFIX|${ETC}|g; \
	      s|CONFVERSION|${CONFVERSION}|g; \
	      s|P9PATHS|${P9PATHS}|g; \
	      s|AWKPATH|${AWKPATH}|g"

include ${ROOT}/mk/man.mk

