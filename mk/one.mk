PROG = $(TARG).out
OFILES = $(OBJ:=.o)

all: $(PROG)

install: $(TARG).install
uninstall: $(TARG).uninstall
clean: oneclean
depend: $(OBJ:=.depend)

printinstall:
	echo 'Install directories:'
	echo '	Bin: $(BIN)'

oneclean:
	for i in $(PROG) $(OFILES); do \
		[ -e $$i ] && \
		echo CLEAN $$($(CLEANNAME) $(BASE)$$i); \
		rm -f $$i; \
	done 2>/dev/null || true

$(OFILES): $(HFILES)

$(PROG): $(OFILES) $(LIB)
	$(LINK) $@ $(OFILES) $(LIB)

