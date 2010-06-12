
VERS = hg$$(hg identify -n)
VERS = $$(test -n "$$WMII_HGVERSION" && echo $$WMII_HGVERSION || \
          hg log -r $$(hg id 2>/dev/null | awk -F'[+ ]' '{print $$1}') --template 'hg{rev}' 2>/dev/null)

WMII_HGVERSION = $(VERS)
WMII_HGVERSION := $(shell echo $(VERS))
WMII_HGVERSION != echo $(VERS)
VERSION = $(WMII_HGVERSION)
CONFVERSION = -hg
COPYRIGHT = ©2010 Kris Maglione

.MAKE.EXPORTED += WMII_HGVERSION
SUBMAKE_EXPORT = WMII_HGVERSION=$(WMII_HGVERSION)

LIBS9 = $(ROOT)/lib/libstuff.a $(ROOT)/lib/libregexp9.a $(ROOT)/lib/libbio.a $(ROOT)/lib/libfmt.a $(ROOT)/lib/libutf.a

CFLAGS += '-DVERSION=\"$(VERSION)\"' '-DCOPYRIGHT=\"$(COPYRIGHT)\"' \
	  '-DCONFVERSION=\"$(CONFVERSION)\"' '-DCONFPREFIX=\"$(ETC)\"'
FILTER = sed "s|@CONFPREFIX@|$(ETC)|g; \
	      s|@GLOBALCONF@|$(ETC)/wmii$(CONFVERSION)|g; \
	      s|@LOCALCONF@|~/.wmii$(CONFVERSION)|g; \
	      s|@CONFVERSION@|$(CONFVERSION)|g; \
	      s|@DOCDIR@|$(DOC)|g; \
	      s|@ALTDOC@|$(DOC)/alternative_wmiircs|g; \
	      s|@EXAMPLES@|$(DOC)/examples|g; \
	      s|@VERSION@|$(VERSION)|g; \
	      s|@LIBDIR@|$(LIBDIR)|g; \
	      s|@BINSH@|$(BINSH)|g; \
	      s|@TERMINAL@|$(TERMINAL)|g; \
	      /^@@/d;"

