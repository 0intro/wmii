# wmii - window manager improved 2
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include config.mk

SUBDIRS = libcext liblitz libixp cmd

BIN = cmd/wm/wmii cmd/wm/wmiiwm cmd/wmiipsel \
	cmd/wmiir cmd/wmiisetsid cmd/wmiiwarp

MAN1 = cmd/wm/wmii.1 cmd/wm/wmiiwm.1 cmd/wmiir.1

all:
	@echo wmii build options:
	@echo "LIBS     = ${LIBS}"
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@for i in ${SUBDIRS} cmd/wm; do \
		(cd $$i; make) \
	done

dist: clean
	mkdir -p wmii-${VERSION}
	cp -R Makefile README LICENSE config.mk rc ${SUBDIRS} wmii-${VERSION}
	tar -cf wmii-${VERSION}.tar wmii-${VERSION}
	gzip wmii-${VERSION}.tar
	rm -rf wmii-${VERSION}

clean:
	rm -f *.o
	for i in ${SUBDIRS} cmd/wm; do \
		(cd $$i; make clean); \
	done
	rm -rf wmii-${VERSION}*

install: all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ${BIN} ${DESTDIR}${PREFIX}/bin
	@sed 's|CONFPREFIX|${CONFPREFIX}|g' <cmd/wm/wmii >${DESTDIR}${PREFIX}/bin/wmii
	@for i in ${BIN}; do \
		chmod 755 ${DESTDIR}${PREFIX}/bin/`basename $$i`; \
	done
	@echo installed executable files to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${CONFPREFIX}/wmii-4
	@cd rc; for i in *; do \
		sed 's|CONFPREFIX|${CONFPREFIX}|g' <$$i >${DESTDIR}${CONFPREFIX}/wmii-4/$$i; \
		chmod 755 ${DESTDIR}${CONFPREFIX}/wmii-4/$$i; \
	done
	@echo installed rc scripts to ${DESTDIR}${CONFPREFIX}/wmii-4
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f ${MAN1} ${DESTDIR}${MANPREFIX}/man1
	@sed 's|CONFPREFIX|${CONFPREFIX}|g' <cmd/wm/wmii.1 >${DESTDIR}${MANPREFIX}/man1/wmii.1
	@for i in ${MAN1}; do \
		chmod 444 ${DESTDIR}${MANPREFIX}/man1/`basename $$i`; \
	done
	@echo installed manual pages to ${DESTDIR}${MANPREFIX}/man1

uninstall:
	for i in ${BIN}; do \
		rm -f ${DESTDIR}${PREFIX}/bin/`basename $$i`; \
	done
	for i in ${MAN1}; do \
		rm -f ${DESTDIR}${MANPREFIX}/man1/`basename $$i`; \
	done
	rm -rf ${DESTDIR}${CONFPREFIX}/wmii-4
