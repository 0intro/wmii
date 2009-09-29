VERS = hg$$(hg identify -n)
VERS = hg$$(hg log -r $$(hg id 2>/dev/null | awk -F'[+ ]' '{print $$1}') --template '{rev}' 2>/dev/null)
VERSION = $(VERS)
VERSION := $(shell echo $(VERS))
VERSION != echo $(VERS)
CONFVERSION = -hg
COPYRIGHT = ©2009 Kris Maglione

CFLAGS += '-DVERSION=\"$(VERSION)\"' '-DCOPYRIGHT=\"$(COPYRIGHT)\"' \
	  '-DCONFVERSION=\"$(CONFVERSION)\"' '-DCONFPREFIX=\"$(ETC)\"'
FILTER = sed "s|@CONFPREFIX@|$(ETC)|g; \
	      s|@CONFVERSION@|$(CONFVERSION)|g; \
	      s|@P9PATHS@|$(P9PATHS)|g; \
	      s|@DOCDIR@|$(DOC)|g; \
	      s|@LIBDIR@|$(LIBDIR)|g; \
	      s|@BINSH@|$(BINSH)|g; \
	      s|@AWKPATH@|$(AWKPATH)|g"

