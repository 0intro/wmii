.SILENT:
.SUFFIXES: .O .o .c .sh .rc .awk .1 .depend .install .clean
all:

.c.o:
	${COMPILE} $@ $<

.c.depend:
	${DEPEND} $< >>.depend

.o.O:
	${LINK} $@ $<

.awk.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@

.rc.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@

.sh.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@

.O.install:
	echo INSTALL $*
	cp -f $< ${BIN}/$*
	chmod 0755 ${BIN}/$* 

.a.install:
	echo INSTALL $<
	cp -f $< ${LIBDIR}/$<
	chmod 0644 ${LIBDIR}/$<

.h.install:
	echo INSTALL $<
	cp -f $< ${INCLUDE}/$<
	chmod 0644 ${INCLUDE}/$<

.1.install:
	echo INSTALL man $*'(1)'
	${FILTER} $< >${MAN}/man1/$<
	chmod 0644 ${MAN}/man1/$<

.O.clean:
	rm $< || true 2>/dev/null
	rm $*.o || true 2>/dev/null
.o.clean:
	rm $< || true 2>/dev/null

printinstall:
mkdirs:
clean:
install: printinstall mkdirs

COMPILE= CC="${CC}" CFLAGS="${CFLAGS} ${EXCFLAGS}" ${ROOT}/util/compile
LINK= LD="${LD}" LDFLAGS="${LDFLAGS} ${EXLDFLAGS}" ${ROOT}/util/link

include ${ROOT}/config.mk
