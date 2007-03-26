dall:
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then echo Skipping nonexistent directory: $$i 1>&2; continue; fi; \
		echo MAKE all ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" all) || exit $?; \
	done
dclean:
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then echo Skipping nonexistent directory: $$i 1>&2; continue; fi; \
		echo MAKE clean ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" clean) || exit $?; \
	done
dinstall:
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then echo Skipping nonexistent directory: $$i 1>&2; continue; fi; \
		echo MAKE install ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" install) || exit $?; \
	done
duninstall:
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then echo Skipping nonexistent directory: $$i 1>&2; continue; fi; \
		echo MAKE uninstall ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" uninstall) || exit $?; \
	done
ddepend:
	for i in ${DIRS}; do \
		if [ ! -d $$i ]; then echo Skipping nonexistent directory: $$i 1>&2; continue; fi; \
		echo MAKE depend ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" depend) || exit $?; \
	done

all: dall
clean: dclean
install: dinstall
uninstall: duninstall
depend: ddepend
