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

deb-dep:
	apt-get install build-essential debhelper libxext-dev x11proto-xext-dev libx11-dev

deb:
	dpkg-buildpackage -rfakeroot

include ${ROOT}/mk/dir.mk
INSTDIRS = ${PDIRS}

