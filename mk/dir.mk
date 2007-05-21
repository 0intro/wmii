MKSUBDIR =  targ=$@; \
	for i in $$dirs; do \
		if [ ! -d $$i ]; then \
			echo Skipping nonexistent directory: $$i 1>&2; \
		else \
			echo MAKE $${targ\#d} ${BASE}$$i/; \
			(cd $$i && ${MAKE} BASE="${BASE}$$i/" $${targ\#d}) || exit $?; \
		fi; \
	done

dall:
	dirs="${DIRS}"; ${MKSUBDIR}
dclean:
	dirs="${DIRS}"; ${MKSUBDIR}
dinstall:
	dirs="${INSTDIRS}"; ${MKSUBDIR}
duninstall:
	dirs="${INSTDIRS}"; ${MKSUBDIR}
ddepend:
	dirs="${DIRS}"; ${MKSUBDIR}

all: dall
clean: dclean
install: dinstall
uninstall: duninstall
depend: ddepend

INSTDIRS = ${DIRS}

