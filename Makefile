# window manager improved 2 - window manager improved 2
#   (C)opyright MMVI Anselm R. Garbe

include config.mk

SRC = area.c bar.c client.c column.c draw.c event.c \
	frame.c fs.c geom.c key.c main.c mouse.c rule.c view.c
OBJ = ${SRC:.c=.o}

all: options wmiiwm

options:
	@echo wmii build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo "LD       = ${LD}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: wmii.h config.mk

wmiiwm: ${OBJ}
	@echo LD $@
	@${LD} -o $@ ${OBJ} ${LDFLAGS}
	@strip $@

clean:
	@echo cleaning
	@rm -f wmiiwm ${OBJ} wmii-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p wmii-${VERSION}
	@cp -R LICENSE Makefile README wmii wmiir config.mk rc \
		wmii.1 wmiir.1 wmiiwm.1 wmii.h ${SRC} wmii-${VERSION}
	@tar -cf wmii-${VERSION}.tar wmii-${VERSION}
	@gzip wmii-${VERSION}.tar
	@rm -rf wmii-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@sed 's|CONFPREFIX|${CONFPREFIX}|g; s|CONFVERSION|${CONFVERSION}|g' <wmii >${DESTDIR}${PREFIX}/bin/wmii
	@cp -f wmiir ${DESTDIR}${PREFIX}/bin
	@cp -f wmiiwm ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wmii
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wmiir
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wmiiwm
	@echo installing scripts to ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@mkdir -p ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@cd rc; for i in *; do \
		sed 's|CONFPREFIX|${CONFPREFIX}|g' <$$i >${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}/$$i; \
		chmod 755 ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}/$$i; \
	done
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed 's/VERSION/${VERSION}/g ; s|CONFPREFIX|${CONFPREFIX}|g' < wmii.1 > ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@sed 's/VERSION/${VERSION}/g' < wmiir.1 > ${DESTDIR}${MANPREFIX}/man1/wmiir.1
	@sed 's/VERSION/${VERSION}/g' < wmiiwm.1 > ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/wmiir.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/wmii
	@rm -f ${DESTDIR}${PREFIX}/bin/wmiir
	@rm -f ${DESTDIR}${PREFIX}/bin/wmiiwm
	@echo removing scripts from ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@rm -rf ${DESTDIR}${CONFPREFIX}/wmii-${CONFVERSION}
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/wmiir.1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1

.PHONY: all options clean dist install uninstall
