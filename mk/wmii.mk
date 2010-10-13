
VERS = hg$$(hg identify -n)
VERS = $$(test -n "$$WMII_HGVERSION" && echo $$WMII_HGVERSION || \
          echo -n "hg$$(hg id -n 2>/dev/null)")

WMII_HGVERSION = $(VERS)
WMII_HGVERSION:= $(shell echo $(VERS))
WMII_HGVERSION!= echo $(VERS)

VERSION = $(WMII_HGVERSION)
COPYRIGHT = ©2010 Kris Maglione

CONFDIR = wmii-hg
LOCALCONF = ~/.$(CONFDIR)
GLOBALCONF = $(ETC)/$(CONFDIR)

.MAKE.EXPORTED += WMII_HGVERSION
SUBMAKE_EXPORT = WMII_HGVERSION=$(WMII_HGVERSION)

LIBS9 = $(ROOT)/lib/libstuff.a $(ROOT)/lib/libregexp9.a $(ROOT)/lib/libbio.a $(ROOT)/lib/libfmt.a $(ROOT)/lib/libutf.a

CFLAGS += '-DVERSION=\"$(VERSION)\"' '-DCOPYRIGHT=\"$(COPYRIGHT)\"' \
	  '-DCONFDIR=\"$(CONFDIR)\"' '-DCONFPREFIX=\"$(ETC)\"' \
	  '-DLOCALCONF=\"$(LOCALCONF)\"' '-DGLOBALCONF=\"$(GLOBALCONF)\"' \
	  -DIXP_NEEDAPI=129

FILTER = sed "s|@ALTDOC@|$(DOC)/alternative_wmiircs|g; \
	      s|@BINSH@|$(BINSH)|g; \
	      s|@CONFDIR@|$(CONFDIR)|g; \
	      s|@CONFPREFIX@|$(ETC)|g; \
	      s|@DOCDIR@|$(DOC)|g; \
	      s|@EXAMPLES@|$(DOC)/examples|g; \
	      s|@GLOBALCONF@|$(GLOBALCONF)|g; \
	      s|@LIBDIR@|$(LIBDIR)|g; \
	      s|@LOCALCONF@|$(LOCALCONF)|g; \
	      s|@PYTHON@|$(PYTHON)|g; \
	      s|@TERMINAL@|$(TERMINAL)|g; \
	      s|@VERSION@|$(VERSION)|g; \
	      /^@@/d;"

