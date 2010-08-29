DIR =
DIRS =
DOC =
DOCDIR =
DOCS =
EXECS =
HFILES =
INCLUDES =
LIB =
LIBS =
OBJ =
OFILES =
OFILES_PIC =
PACKAGES =
PROG =
SO =
TAGFILES =
TARG =
TEXT =

FILTER = cat

EXCFLAGS = $(INCLUDES) -D_XOPEN_SOURCE=600

COMPILE_FLAGS = $(EXCFLAGS) $(CFLAGS)
COMPILE    = $(SHELL) $(ROOT)/util/compile "$(CC)" "$(PACKAGES)" "$(COMPILE_FLAGS)"
COMPILEPIC = $(SHELL) $(ROOT)/util/compile "$(CC)" "$(PACKAGES)" "$(COMPILE_FLAGS) $(SOCFLAGS)"

LINK   = $(SHELL) $(ROOT)/util/link "$(LD)" "$(PACKAGES)" "$(LDFLAGS) $(LIBS)"
LINKSO = $(SHELL) $(ROOT)/util/link "$(LD)" "$(PACKAGES)" "$(SOLDFLAGS) $(LIBS) $(SHARED)"

CLEANNAME=$(SHELL) $(ROOT)/util/cleanname

SOEXT=so
TAGFILES=

CTAGS=ctags

PACKAGES = 

# Try to find a sane shell. /bin/sh is a last resort, because it's
# usually bash on Linux, which means it's painfully slow.
SHELLSEARCH = for sh in /bin/dash /bin/ksh /bin/sh; do \
	      if test -x $$sh; then echo $$sh; exit; fi; done

BINSH:= $(shell $(SHELLSEARCH))
BINSH!= $(SHELLSEARCH)
SHELL := $(BINSH)
.SHELL: name=sh path=$(SHELL)

include $(ROOT)/config.mk
sinclude $(ROOT)/config.local.mk
sinclude $(shell echo .)depend

.SILENT:
.SUFFIXES: .$(SOEXT) .1 .3 .awk .build .c .clean .depend .install .man1 .man3 .o .o_pic .out .pdf .py .rc .sh .uninstall
all:

.c.depend:
	echo MKDEP $<
	$(DEBUG) eval "$(MKDEP) $(COMPILE_FLAGS)" $< | sed '1s|.*:|$(<:%.c=%.o):|' >>.depend

.sh.depend .rc.depend .1.depend .3.depend .awk.depend:
	:

.c.o:
	$(COMPILE) $@ $<
.c.o_pic:
	$(COMPILEPIC) $@ $<

.o.out:
	$(LINK) $@ $<
.c.out:
	$(COMPILE) $(<:.c=.o) $<
	$(LINK) $@ $(<:.c=.o)

.rc.out .awk.out .sh.out:
	echo FILTER $(BASE)$<
	[ -n "$(<:%.sh=)" ] || $(BINSH) -n $<
	set -e; \
	$(DEBUG) $(FILTER) $< >$@; \
	$(DEBUG) chmod 0755 $@

.man1.1 .man3.3:
	echo TXT2TAGS $(BASE)$<
	$(DEBUG) txt2tags -o- $< >$@

DEBUG = _debug() { [ -n "$$noisycc" ] && echo >&2 $$@ || true; "$$@"; }; _debug

INSTALL= _install() { set -e; \
		 dashb=$$1; [ $$1 = -b ] && shift; \
		 d=$(DESTDIR)$$3; f=$$d/$$(basename $$4); \
		 if [ ! -d $$d ]; then echo MKDIR $$3; mkdir -p $$d; fi; \
		 echo INSTALL $$($(CLEANNAME) $(BASE)$$2); \
		 $(DEBUG) rm -f $$f; \
		 if [ "$$dashb" = -b ]; \
		 then $(DEBUG) cp -f $$2 $$f; \
		 else $(DEBUG) $(FILTER) <$$2 >$$f; \
		 fi; \
		 $(DEBUG) chmod $$1 $$f; \
	 }; _install
UNINSTALL= _uninstall() { set -e; \
	           echo UNINSTALL $$($(CLEANNAME) $(BASE)$$1); \
		   $(DEBUG) rm -f $(DESTDIR)$$2/$$(basename $$3); \
	   }; _uninstall

.out.install:
	$(INSTALL) -b 0755 $< $(BIN) $*
.out.uninstall:
	$(UNINSTALL) $< $(BIN) $*

.a.install .$(SOEXT).install:
	$(INSTALL) -b 0644 $< $(LIBDIR) $<
.a.uninstall .$(SOEXT).uninstall:
	$(UNINSTALL) $< $(LIBDIR) $<

.h.install:
	$(INSTALL) 0644 $< $(INCLUDE) $<
.h.uninstall:
	$(UNINSTALL) $< $(INCLUDE) $<

.pdf.install:
	$(INSTALL) -b 0644 $< $(DOC) $<
.pdf.uninstall:
	$(UNINSTALL) $< $(DOC) $<

INSTALMAN=   _installman()   { man=$${1\#\#*.}; $(INSTALL) 0644 $$1 $(MAN)/man$$man $$1; }; _installman
UNINSTALLMAN=_uninstallman() { man=$${1\#\#*.}; $(UNINSTALL) $$1 $(MAN)/man$$man $$1; }; _uninstallman
MANSECTIONS=1 2 3 4 5 6 7 8 9
$(MANSECTIONS:%=.%.install):
	$(INSTALMAN) $<
$(MANSECTIONS:%=.%.uninstall):
	$(UNINSTALLMAN) $<

.out.clean:
	echo CLEAN $$($(CLEANNAME) $(BASE)$<)
	rm -f $< || true 2>/dev/null
	rm -f $*.o || true 2>/dev/null
.o.clean .o_pic.clean:
	echo CLEAN $$($(CLEANNAME) $(BASE)$<)
	rm -f $< || true 2>/dev/null

printinstall:
clean:
install: printinstall
depend: cleandep

include $(ROOT)/mk/common.mk

