ROOT=..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

wmiir.c: ${ROOT}/mk/wmii.mk

DIRS =	wmii
TARG =	wmii9menu \
	wmii9rc	  \
	wmiiloop  \
	wmiir	  \
	wmiistartrc

OFILES = util.o
FILTER = sed "s|CONFPREFIX|${ETC}|g; \
	      s|CONFVERSION|${CONFVERSION}|g; \
	      s|P9PATHS|${P9PATHS}|g; \
	      s|AWKPATH|${AWKPATH}|g"

LDFLAGS += -lfmt -lutf
CFLAGS += ${INCX11} -DVERSION=\"${VERSION}\"

include ${ROOT}/mk/many.mk
include ${ROOT}/mk/dir.mk

OWMIIR=wmiir.o ${OFILES} ${LIBIXP}
wmiir.O: ${OWMIIR}
	${LINK} $@ ${STATIC} ${OWMIIR}

wmii9menu.O: wmii9menu.o ${OFILES}
	${LINK} $@ $*.o ${OFILES} ${LIBX11}
