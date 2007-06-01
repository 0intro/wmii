ROOT=.
include ${ROOT}/mk/hdr.mk

PDIRS = \
	cmd	\
	rc	\
	man

DIRS =	\
	${PDIRS}

config:
	ROOT="${ROOT}" ${ROOT}/util/genconfig

include ${ROOT}/mk/dir.mk
INSTDIRS = ${PDIRS}

