ROOT=.
include ${ROOT}/mk/hdr.mk

DIRS =	libixp	\
	cmd	\
	rc	\
	man

config:
	ROOT="${ROOT}" ${ROOT}/util/genconfig

include ${ROOT}/mk/dir.mk
