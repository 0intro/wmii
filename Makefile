ROOT=.
include ${ROOT}/mk/hdr.mk

PDIRS = \
	cmd	\
	rc	\
	man

DIRS =	libixp	\
	${PDIRS}

include ${ROOT}/mk/dir.mk
INSTDIRS = ${PDIRS}

