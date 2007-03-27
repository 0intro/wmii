ROOT=..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

BIN = ${ETC}/wmii-${CONFVERSION}
TARG =	rc.wmii   \
	wmiirc    \
	welcome

FILTER = sed "s|CONFPREFIX|${ETC}|g; \
	      s|CONFVERSION|${CONFVERSION}|g; \
	      s|P9PATHS|${P9PATHS}|g; \
	      s|AWKPATH|${AWKPATH}|g"

include ${ROOT}/mk/many.mk
