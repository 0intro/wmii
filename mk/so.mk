SOPTARG = $(ROOT)/lib/$(TARG)
SO = $(PTARG).so
SONAME = $(TARG).so
OFILES_PIC = ${OBJ:=.o_pic}

all: $(HFILES) $(SO) 

install: $(SOPTARG).install
uninstall: $(SOPTARG).uninstall
clean: soclean
depend: ${OBJ:=.depend}

soclean:
	for i in $(SO) $(OFILES_PIC); do \
		rm -f $$i; \
	done 2>/dev/null || true

printsoinstall:
	echo 'Install directories:'
	echo '	Lib: $(LIBDIR)'

printinstall: printsoinstall

$(SO): $(OFILES_PIC)
	mkdir $(ROOT)/lib 2>/dev/null || true
	$(LINKSO) $@ $(OFILES_PIC)

