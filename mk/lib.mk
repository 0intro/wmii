PTARG = $(ROOT)/lib/$(TARG)
LIB = $(PTARG).a
OFILES = ${OBJ:=.o}

all: $(HFILES) $(LIB) 

install: $(PTARG).install
uninstall: $(PTARG).uninstall
clean: libclean
depend: ${OBJ:=.depend}

libclean:
	for i in $(LIB) $(OFILES); do \
		[ -e $$i ] && \
		echo CLEAN $$($(CLEANNAME) $(BASE)$$i); \
		rm -f $$i; \
	done 2>/dev/null || true

printinstall:
	echo 'Install directories:'
	echo '	Lib: $(LIBDIR)'

$(LIB): $(OFILES)
	echo AR $$($(CLEANNAME) $(BASE)/$@)
	mkdir $(ROOT)/lib 2>/dev/null || true
	$(AR) $@ $(OFILES)

SOMKSH=case "$(MAKESO)" in 1|[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]) echo $(ROOT)/mk/so.mk;; *) echo /dev/null;; esac
SOMK:=${shell $(SOMKSH)}
SOMK!=$(SOMKSH)
include $(SOMK)

