SOPTARG = $(ROOT)/lib/$(TARG)
SO = $(SOPTARG).$(SOEXT)
SONAME = $(TARG).$(SOEXT)
OFILES_PIC = $(OBJ:=.o_pic)

all: $(HFILES) $(SO) 

install: $(SOPTARG).install
uninstall: $(SOPTARG).uninstall
clean: soclean
depend: $(OBJ:=.depend)

soclean:
	for i in $(SO) $(OFILES_PIC); do \
		[ -e $$i ] && \
		echo CLEAN $$($(CLEANNAME) $(BASE)$$i); \
		rm -f $$i; \
	done 2>/dev/null || true

printsoinstall:
	echo 'Install directories:'
	echo '	Lib: $(LIBDIR)'

printinstall: printsoinstall

$(SO): $(OFILES_PIC)
	mkdir $(ROOT)/lib 2>/dev/null || true
	$(LINKSO) $@ $(OFILES_PIC)

