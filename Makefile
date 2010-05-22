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
	libstuff  \
	libbio    \
	libfmt	  \
	libregexp \
	libutf	  \
	$(PDIRS)

DOCS = README \
       LICENSE

deb-dep:
	IFS=', '; \
	apt-get -qq install build-essential $$(sed -n 's/([^)]*)//; s/^Build-Depends: \(.*\)/\1/p' debian/control)

DISTRO = unstable
deb:
	if [ -d .hg ]; \
	then hg tip --template 'wmii-hg ($(VERSION)) $(DISTRO); urgency=low\n\n  * {desc}\n\n -- {author}  {date|rfc822date}\n'; \
	else awk 'BEGIN{"date"|getline; print "wmii-hg ($(VERSION)) $(DISTRO); urgency=low\n\n  * Upstream build\n\n -- Kris Maglione <jg@suckless.org>  "$$0"\n"}'; \
	fi >debian/changelog
	dpkg-buildpackage -rfakeroot -b -nc
	[ -d .hg ] && hg revert debian/changelog

include ${ROOT}/mk/dir.mk
INSTDIRS = $(PDIRS)

