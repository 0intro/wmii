# window manager improved 2 - window manager improved 2
#   (C)opyright MMVI Anselm R. Garbe

include config.mk

SRC = area.c bar.c brush.c client.c color.c column.c draw.c event.c \
	font.c frame.c fs.c geom.c key.c mouse.c rule.c view.c wm.c
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

${OBJ}: wm.h config.mk

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
	@cp -R LICENSE Makefile README wmii config.mk rc \
		wmii.1 wmiiwm.1 wm.h ${SRC} wmii-${VERSION}
	@tar -cf wmii-${VERSION}.tar wmii-${VERSION}
	@gzip wmii-${VERSION}.tar
	@rm -rf wmii-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@sed 's|CONFPREFIX|${CONFPREFIX}|g; s|VERSION|${VERSION}|g' <wmii >${DESTDIR}${PREFIX}/bin/wmii
	@cp -f wmiiwm ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wmii
	@chmod 755 ${DESTDIR}${PREFIX}/bin/wmiiwm
	@echo installing scripts to ${DESTDIR}${CONFPREFIX}/wmii-${VERSION}
	@mkdir -p ${DESTDIR}${CONFPREFIX}/wmii-${VERSION}
	@cd rc; for i in *; do \
		sed 's|CONFPREFIX|${CONFPREFIX}|g' <$$i >${DESTDIR}${CONFPREFIX}/wmii-${VERSION}/$$i; \
		chmod 755 ${DESTDIR}${CONFPREFIX}/wmii-${VERSION}/$$i; \
	done
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed 's/VERSION/${VERSION}/g' < wmii.1 > ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@sed 's/VERSION/${VERSION}/g' < wmiiwm.1 > ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/wmii
	@rm -f ${DESTDIR}${PREFIX}/bin/wmiiwm
	@echo removing scripts from ${DESTDIR}${CONFPREFIX}/wmii-${VERSION}
	@rm -rf ${DESTDIR}${CONFPREFIX}/wmii-${VERSION}
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/wmii.1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/wmiiwm.1

.PHONY: all options clean dist install uninstall
