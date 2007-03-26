ROOT=..
include ${ROOT}/mk/hdr.mk

DIRS =	wmii
TARG =	wmii9menu \
	wmii9rc	  \
	wmiiloop  \
	wmiir	  \
	wmiistartrc

OFILES = util.o
FILTER = sed "s|CONFPREFIX|${CONFPREFIX}|g; \
	      s|CONFVERSION|${CONFVERSION}|g; \
	      s|P9PATHS|${P9PATHS}|g; \
	      s|AWKPATH|${AWKPATH}|g"

include ${ROOT}/mk/many.mk
include ${ROOT}/mk/dir.mk

OWMIIR=${OFILES} ${LIBIXP}
wmiir.O: ${OWMIIR}
	${LINK} $@ $*.o ${OWMIIR}

EXCFLAGS=${INCX11}
wmii9menu.O:
	${LINK} $@ $*.o ${LIBX11}
