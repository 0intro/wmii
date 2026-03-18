
VERS = git$$(git rev-parse --short HEAD 2>/dev/null || echo unknown)

WMII_GITVERSION = $(VERS)
WMII_GITVERSION:= $(shell echo $(VERS))
WMII_GITVERSION!= echo $(VERS)

VERSION = $(WMII_GITVERSION)
COPYRIGHT = ©2010 Kris Maglione

CONFDIR = wmii
LOCALCONF = ~/.$(CONFDIR)
GLOBALCONF = $(ETC)/$(CONFDIR)

.MAKE.EXPORTED += WMII_GITVERSION
SUBMAKE_EXPORT = WMII_GITVERSION=$(WMII_GITVERSION)

LIBS9 = $(ROOT)/lib/libstuff.a $(ROOT)/lib/libregexp9.a $(ROOT)/lib/libbio.a $(ROOT)/lib/libfmt.a $(ROOT)/lib/libutf.a

CFLAGS += -include $(ROOT)/include/config.h \
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

