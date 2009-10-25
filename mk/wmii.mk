VERS = hg$$(hg identify -n)
VERS = hg$$(hg log -r $$(hg id 2>/dev/null | awk -F'[+ ]' '{print $$1}') --template '{rev}' 2>/dev/null)
VERSION = $(VERS)
VERSION := $(shell echo $(VERS))
VERSION != echo $(VERS)
CONFVERSION = -hg
COPYRIGHT = ©2009 Kris Maglione

LIBS9 = $(ROOT)/lib/libregexp9.a $(ROOT)/lib/libbio.a $(ROOT)/lib/libfmt.a $(ROOT)/lib/libutf.a

CFLAGS += '-DVERSION=\"$(VERSION)\"' '-DCOPYRIGHT=\"$(COPYRIGHT)\"' \
	  '-DCONFVERSION=\"$(CONFVERSION)\"' '-DCONFPREFIX=\"$(ETC)\"'
FILTER = sed "s|@CONFPREFIX@|$(ETC)|g; \
	      s|@CONFVERSION@|$(CONFVERSION)|g; \
	      s|@DOCDIR@|$(DOC)|g; \
	      s|@VERSION@|$(VERSION)|g; \
	      s|@LIBDIR@|$(LIBDIR)|g; \
	      s|@BINSH@|$(BINSH)|g; \
	      s|@TERMINAL@|$(TERMINAL)|g;"

