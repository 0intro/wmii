ROOT=.
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/wmii.mk

PDIRS = \
	doc	     \
	man	     \
	cmd	     \
	libwmii_hack \
	rc	     \
	alternative_wmiircs

DIRS =	\
	libbio    \
	libfmt	  \
	libregexp \
	libutf	  \
	$(PDIRS)

DOCS = README

deb-dep:
	apt-get -qq install build-essential debhelper libxext-dev x11proto-xext-dev libx11-dev libxrandr-dev

deb:
	dpkg-buildpackage -rfakeroot

include ${ROOT}/mk/dir.mk
INSTDIRS = $(PDIRS)

