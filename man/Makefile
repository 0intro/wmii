ROOT=..
include ${ROOT}/mk/hdr.mk

TARG =	wmii.1	   \
	wmiiloop.1 \
	wmiir.1	   \
	wmiiwm.1

FILTER = sed "s|CONFPREFIX|${CONFPREFIX}|g; \
	      s|CONFVERSION|${CONFVERSION}|g; \
	      s|P9PATHS|${P9PATHS}|g; \
	      s|AWKPATH|${AWKPATH}|g"

include ${ROOT}/mk/man.mk
