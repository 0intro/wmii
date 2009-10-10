ROOT=.
include ${ROOT}/mk/hdr.mk

PDIRS = \
	cmd	     \
	libwmii_hack \
	rc	     \
	alternative_wmiircs \
	doc	     \
	man

DIRS =	\
	libbio    \
	libfmt	  \
	libregexp \
	libutf	  \
	${PDIRS}

config:
	ROOT="${ROOT}" ${ROOT}/util/genconfig

deb-dep:
	IFS=', '; \
	apt-get -qq install build-essential $$(sed -n 's/([^)]*)//; s/^Build-Depends: \(.*\)/\1/p' debian/control)

deb:
	dpkg-buildpackage -rfakeroot

include ${ROOT}/mk/dir.mk
INSTDIRS = ${PDIRS}
.PHONY: config

