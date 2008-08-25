FILTER = cat
EXCFLAGS = -I$$(echo $(INCPATH)|sed 's/:/ -I/g') -D_XOPEN_SOURCE=600
COMPILE= CC="$(CC)" CFLAGS="$(EXCFLAGS) $(CFLAGS)" $(ROOT)/util/compile
COMPILEPIC= CC="$(CC)" CFLAGS="$(EXCFLAGS) $(CFLAGS) $(SOCFLAGS)" $(ROOT)/util/compile
LINK= LD="$(LD)" LDFLAGS="$(LDFLAGS)" $(ROOT)/util/link
LINKSO= LD="$(LD)" LDFLAGS="$(SOLDFLAGS) $(SHARED)" $(ROOT)/util/link
CLEANNAME=$(ROOT)/util/cleanname
SOEXT=so

include $(ROOT)/config.mk

# I hate this.
MKCFGSH=if test -f $(ROOT)/config.local.mk; then echo $(ROOT)/config.local.mk; else echo /dev/null; fi
MKCFG:=${shell $(MKCFGSH)}
MKCFG!=${MKCFGSH}
include $(MKCFG)
# and this:
# Try to find a sane shell. /bin/sh is a last resort, because it's
# usually bash on Linux, which means, it's painfully slow.
BINSH := $(shell \
	   if [ -x /bin/dash ]; then echo /bin/dash; \
	   elif [ -x /bin/ksh ]; then echo /bin/ksh; \
	   else echo /bin/sh; fi)
BINSH != echo /bin/sh

.SILENT:
.SUFFIXES: .O .o .o_pic .c .sh .rc .$(SOEXT) .awk .1 .man1 .depend .install .uninstall .clean
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

.o.O:
	$(LINK) $@ $<
.c.O:
	$(COMPILE) ${<:.c=.o} $<
	$(LINK) $@ ${<:.c=.o}

.sh.O:
	echo FILTER $(BASE)$<
	$(FILTER) $< >$@
	sh -n $@
	chmod 0755 $@
.rc.O .awk.O:
	echo FILTER $(BASE)$<
	$(FILTER) $< >$@
	chmod 0755 $@
.1.man1:
	echo FILTER $(BASE)$<
	$(FILTER) $< >$@

.O.install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$*)
	cp -f $< $(BIN)/$*
	chmod 0755 $(BIN)/$* 
.O.uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$*)
	rm -f $(BIN)/$* 

.a.install .$(SOEXT).install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$<)
	cp -f $< $(LIBDIR)/$<
	chmod 0644 $(LIBDIR)/$<
.a.uninstall .$(SOEXT).uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$<)
	rm -f $(LIBDIR)/$<

.h.install:
	echo INSTALL $$($(CLEANNAME) $(BASE)$<)
	cp -f $< $(INCLUDE)/$<
	chmod 0644 $(INCLUDE)/$<
.h.uninstall:
	echo UNINSTALL $$($(CLEANNAME) $(BASE)$<)
	rm -f $(INCLUDE)/$<

.man1.install:
	set -e; \
	man=1; \
	path="$(MAN)/man$$man/$*.$$man"; \
	echo INSTALL man $$($(CLEANNAME) "$(BASE)/$*($$man)"); \
	cp "$<" "$$path"; \
	chmod 0644 "$$path"
.man1.uninstall:
	echo UNINSTALL man $$($(CLEANNAME) $*'(1)')
	rm -f $(MAN)/man1/$<

.O.clean:
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

