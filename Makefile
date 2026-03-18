ROOT=.
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

include/config.h: mk/wmii.mk config.mk Makefile
	@echo GENERATE include/config.h
	@printf '#define VERSION "%s"\n' '$(VERSION)' >$@
	@printf '#define COPYRIGHT "%s"\n' '$(COPYRIGHT)' >>$@
	@printf '#define CONFDIR "%s"\n' '$(CONFDIR)' >>$@
	@printf '#define CONFPREFIX "%s"\n' '$(ETC)' >>$@
	@printf '#define LOCALCONF "%s"\n' '$(LOCALCONF)' >>$@
	@printf '#define GLOBALCONF "%s"\n' '$(GLOBALCONF)' >>$@

dall: include/config.h

clean: config-clean
config-clean:
	rm -f include/config.h

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
       README.md

deb-dep:
	IFS=', '; \
	apt-get -qq install build-essential $$(sed -n 's/([^)]*)//; s/^Build-Depends: \(.*\)/\1/p' debian/control)

DISTRO = unstable
deb:
	$(ROOT)/util/genchangelog wmii $(VERSION) $(DISTRO)
	dpkg-buildpackage -rfakeroot -b -nc
	[ -d .git ] && git checkout debian/changelog || true

include $(ROOT)/mk/dir.mk

