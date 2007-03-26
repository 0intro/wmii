MKSUBDIR =  targ=$@; \
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then \
			echo Skipping nonexistent directory: $$i 1>&2; \
		else \
			echo MAKE $${targ\#d} ${BASE}$$i/; \
			(cd $$i && ${MAKE} BASE="${BASE}$$i/" $${targ\#d}) || exit $?; \
		fi; \
	done
dall:
	${MKSUBDIR}
dclean:
	${MKSUBDIR}
dinstall:
	${MKSUBDIR}
duninstall:
	${MKSUBDIR}
ddepend:
	${MKSUBDIR}

all: dall
clean: dclean
install: dinstall
uninstall: duninstall
depend: ddepend
