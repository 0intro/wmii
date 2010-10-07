ROOT=.
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

DIRS = \
	doc	     \
	examples     \
	man	     \
	lib	     \
	cmd	     \
	rc	     \
	alternative_wmiircs

DOCS = FAQ \
       LICENSE \
       README

deb-dep:
	IFS=', '; \
	apt-get -qq install build-essential $$(sed -n 's/([^)]*)//; s/^Build-Depends: \(.*\)/\1/p' debian/control)

DISTRO = unstable
deb:
	$(ROOT)/util/genchangelog wmii-hg $(VERSION) $(DISTRO)
	dpkg-buildpackage -rfakeroot -b -nc
	[ -d .hg ] && hg revert debian/changelog || true

include $(ROOT)/mk/dir.mk

