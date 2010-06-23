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

COMPILE_FLAGS = $(EXCFLAGS) $(CFLAGS) $$(pkg-config --cflags $(PACKAGES))
COMPILE    = $(SHELL) $(ROOT)/util/compile "$(CC)" "$(COMPILE_FLAGS)"
COMPILEPIC = $(SHELL) $(ROOT)/util/compile "$(CC)" "$(COMPILE_FLAGS) $(SOCFLAGS)"

LINK   = $(SHELL) $(ROOT)/util/link "$(LD)" "$$(pkg-config --libs $(PACKAGES)) $(LDFLAGS) $(LIBS)"
LINKSO = $(SHELL) $(ROOT)/util/link "$(LD)" "$$(pkg-config --libs $(PACKAGES)) $(SOLDFLAGS) $(LIBS) $(SHARED)"

CLEANNAME=$(SHELL) $(ROOT)/util/cleanname

SOEXT=so
TAGFILES=

CTAGS=ctags

PACKAGES = 2>/dev/null

# and this:
# Try to find a sane shell. /bin/sh is a last resort, because it's
# usually bash on Linux, which means it's painfully slow.
BINSH := $(shell \
	   if [ -x /bin/dash ]; then echo /bin/dash; \
	   elif [ -x /bin/ksh ]; then echo /bin/ksh; \
	   else echo /bin/sh; fi)
BINSH != echo /bin/sh

include $(ROOT)/config.mk

# I hate this.
MKCFGSH=if test -f $(ROOT)/config.local.mk; then echo $(ROOT)/config.local.mk; else echo /dev/null; fi
MKCFG:=$(shell $(MKCFGSH))
MKCFG!=$(MKCFGSH)
include $(MKCFG)

.SILENT:
.SUFFIXES: .out .o .o_pic .c .pdf .sh .rc .$(SOEXT) .awk .1 .3 .man1 .man3 .depend .install .uninstall .clean
all:

MAKEFILES=.depend
.c.depend:
	echo MKDEP $<
	[ -n "$(noisycc)" ] && echo $(MKDEP) $(COMPILE_FLAGS) $< || true
	eval "$(MKDEP) $(COMPILE_FLAGS)" $< | sed '1s|.*:|$(<:%.c=%.o):|' >>.depend

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
	[ -n "$(noisycc)" ] && set -x; \
	$(FILTER) $< >$@; \
	chmod 0755 $@

.man1.1 .man3.3:
	echo TXT2TAGS $(BASE)$<
	[ -n "$(noisycc)" ] && set -x; \
	txt2tags -o- $< >$@

INSTALL= _install() { set -e; \
		 dashb=$$1; [ $$1 = -b ] && shift; \
		 d=$(DESTDIR)$$3; f=$$d/$$(basename $$4); \
		 if [ ! -d $$d ]; then echo MKDIR $$3; mkdir -p $$d; fi; \
		 echo INSTALL $$($(CLEANNAME) $(BASE)$$2); \
		 [ -n "$(noisycc)" ] && set -x; \
		 rm -f $$f; \
		 if [ "$$dashb" = -b ]; \
		 then cp -f $$2 $$f; \
		 else $(FILTER) <$$2 >$$f; \
		 fi; \
		 chmod $$1 $$f; \
		 set +x; \
	 }; _install
UNINSTALL= _uninstall() { set -e; \
	           echo UNINSTALL $$($(CLEANNAME) $(BASE)$$1); \
		   [ -n "$(noisycc)" ] && set -x; \
		   rm -f $(DESTDIR)$$2/$$(basename $$3); \
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

