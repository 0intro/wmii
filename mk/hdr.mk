FILTER = cat
EXCFLAGS = -I$$(echo $(INCPATH)|sed 's/:/ -I/g') -D_XOPEN_SOURCE=600
COMPILE= CC="$(CC)" CFLAGS="$(EXCFLAGS) $(CFLAGS) $$(pkg-config --cflags $(PACKAGES))" $(ROOT)/util/compile
COMPILEPIC= CC="$(CC)" CFLAGS="$(EXCFLAGS) $(CFLAGS) $$(pkg-config --cflags $(PACKAGES)) $(SOCFLAGS)" $(ROOT)/util/compile
LINK= LD="$(LD)" LDFLAGS="$(LDFLAGS) $$(pkg-config --libs $(PACKAGES))" $(ROOT)/util/link
LINKSO= LD="$(LD)" LDFLAGS="$(SOLDFLAGS) $(SHARED) $$(pkg-config --libs $(PACKAGES))" $(ROOT)/util/link
CLEANNAME=$(ROOT)/util/cleanname
SOEXT=so
TAGFILES=
CTAGS=ctags

PACKAGES = 2>/dev/null

include $(ROOT)/config.mk

# I hate this.
MKCFGSH=if test -f $(ROOT)/config.local.mk; then echo $(ROOT)/config.local.mk; else echo /dev/null; fi
MKCFG:=${shell $(MKCFGSH)}
MKCFG!=${MKCFGSH}
include $(MKCFG)

# and this:
# Try to find a sane shell. /bin/sh is a last resort, because it's
# usually bash on Linux, which means it's painfully slow.
BINSH := $(shell \
	   if [ -x /bin/dash ]; then echo /bin/dash; \
	   elif [ -x /bin/ksh ]; then echo /bin/ksh; \
	   else echo /bin/sh; fi)
BINSH != echo /bin/sh

.SILENT:
.SUFFIXES: .out .o .o_pic .c .pdf .sh .rc .$(SOEXT) .awk .1 .man1 .depend .install .uninstall .clean
all:

.c.depend:
	echo MKDEP $<
	$(MKDEP) $(EXCFLAGS) $(CFLAGS) $< >>.depend

.sh.depend .rc.depend .1.depend .awk.depend:
	:

.c.o:
	$(COMPILE) $@ $<
.c.o_pic:
	$(COMPILEPIC) $@ $<

.o.out:
	$(LINK) $@ $<
.c.out:
	$(COMPILE) ${<:.c=.o} $<
	$(LINK) $@ ${<:.c=.o}

.sh.out:
	echo FILTER $(BASE)$<
	$(FILTER) $< >$@
	sh -n $@
	chmod 0755 $@
.rc.out .awk.out:
	echo FILTER $(BASE)$<
	$(FILTER) $< >$@
	chmod 0755 $@
.man1.1:
	echo TXT2TAGS $(BASE)$<
	txt2tags -o- $< | $(FILTER) >$@

.out.install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$*)
	cp -f $< $(DESTDIR)$(BIN)/$*
	chmod 0755 $(DESTDIR)$(BIN)/$* 
.out.uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$*)
	rm -f $(DESTDIR)$(BIN)/$* 

.a.install .$(SOEXT).install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$<)
	cp -f $< $(DESTDIR)$(LIBDIR)/$<
	chmod 0644 $(DESTDIR)$(LIBDIR)/$<
.a.uninstall .$(SOEXT).uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$<)
	rm -f $(DESTDIR)$(LIBDIR)/$<

.h.install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$<)
	cp -f $< $(DESTDIR)$(INCLUDE)/$<
	chmod 0644 $(DESTDIR)$(INCLUDE)/$<
.h.uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$<)
	rm -f $(DESTDIR)$(INCLUDE)/$<

.pdf.install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$<)
	cp -f $< $(DESTDIR)$(DOC)/$<
	chmod 0644 $(DESTDIR)$(DOC)/$<
.pdf.uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$<)
	rm -f $(DESTDIR)$(DOC)/$<

.1.install:
	set -e; \
	man=1; \
	path="$(MAN)/man$$man/$*.$$man"; \
	echo INSTALL man $$($(CLEANNAME) "$(BASE)/$*($$man)"); \
	cp "$<" $(DESTDIR)"$$path"; \
	chmod 0644 $(DESTDIR)"$$path"
.1.uninstall:
	echo UNINSTALL man $$($(CLEANNAME) $*'(1)')
	rm -f $(DESTDIR)$(MAN)/man1/$<

.out.clean:
	echo CLEAN $$($(CLEANNAME) $(BASE)$<)
	rm -f $< || true 2>/dev/null
	rm -f $*.o || true 2>/dev/null
.o.clean .o_pic.clean:
	echo CLEAN $$($(CLEANNAME) $(BASE)$<)
	rm -f $< || true 2>/dev/null

printinstall:
mkdirs:
clean:
install: printinstall mkdirs
depend: cleandep

include $(ROOT)/mk/common.mk

