all:

install: all

MANDIRS=${MAN}/man1
mkdirs:
	for i in ${BIN} ${ETC} ${LIBDIR} ${MANDIRS} ${INCLUDE} ${DIRS}; do \
		test -d $$i || echo MKDIR $$i; \
		mkdir -pm 0755 $$i; \
	done

cleandep:
	echo CLEANDEP
	rm .depend 2>/dev/null || true

DEP:=${shell if test -f .depend;then echo .depend;else echo /dev/null; fi}
DEP!=echo /dev/null
include ${DEP}

.PHONY: all options clean dist install uninstall depend cleandep
