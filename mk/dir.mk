MKSUBDIR =  targ=$@; targ=$${targ\#d}; \
	for i in $$dirs; do \
		export BASE=$(BASE)$$i/; \
		if [ ! -d $$i ]; then \
			echo Skipping nonexistent directory: $$i 1>&2; \
		else \
			echo MAKE $$targ $$BASE; \
			(cd $$i && $(MAKE) $$targ) || exit $?; \
		fi; \
	done

dall:
	+dirs="$(DIRS)"; $(MKSUBDIR)
dclean:
	+dirs="$(DIRS)"; $(MKSUBDIR)
dinstall:
	+dirs="$(INSTDIRS)"; $(MKSUBDIR)
duninstall:
	+dirs="$(INSTDIRS)"; $(MKSUBDIR)
ddepend:
	+dirs="$(DIRS)"; $(MKSUBDIR)

all: dall
clean: dclean
install: dinstall
uninstall: duninstall
depend: ddepend

INSTDIRS = $(DIRS)

