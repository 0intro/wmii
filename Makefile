ROOT=.
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

DIRS = \
	doc	     \
	man	     \
	lib	     \
	cmd	     \
	rc	     \
	alternative_wmiircs

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
	fi >debian/changelog || true
	dpkg-buildpackage -rfakeroot -b -nc
	[ -d .hg ] && hg revert debian/changelog || true

include $(ROOT)/mk/dir.mk

