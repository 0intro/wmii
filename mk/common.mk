all:

install: all
depend: cleandep

MANDIRS=${MAN}/man1
mkdirs:
	for i in ${BIN} ${ETC} ${LIBDIR} ${MANDIRS} ${INCLUDE} ${DIRS}; do \
		test -d $$i || echo MKDIR $$i; \
		mkdir -pm 0755 $$i; \
	done

install: ${HFILES:.h=.install}
uninstall: ${HFILES:.h=.uninstall}

cleandep:
	rm .depend 2>/dev/null || true

.PHONY: all options clean dist install uninstall depend cleandep
