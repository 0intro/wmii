PROG = ${TARG}.O
OFILES = ${OBJ:=.o}

all: ${PROG}

install: ${TARG}.install
uninstall: ${TARG}.uninstall
clean: oneclean
depend: ${OBJ:=.depend}

printinstall:
	echo 'Install directories:'
	echo '	Bin: ${BIN}'

oneclean:
	for i in ${PROG} ${OFILES}; do \
		rm $$i; \
	done 2>/dev/null || true

${OFILES}: ${HFILES}

${PROG}: ${OFILES} ${LIB}
	${LINK} $@ ${OFILES} ${LIB}

include ${ROOT}/mk/common.mk
