LIB = ${TARG}.a
OFILES = ${OBJ:=.o}

all: ${HFILES} ${LIB} 

install: ${TARG}.install
uninstall: ${TARG}.uninstall
clean: libclean
depend: ${OBJ:=.depend}

libclean:
	for i in ${LIB} ${OFILES}; do \
		rm $$i; \
	done 2>/dev/null || true

printinstall:
	echo 'Install directories:'
	echo '	Lib: ${LIBDIR}'

${LIB}: ${OFILES}
	@echo AR $@
	@${AR} $@ ${OFILES}

include ${ROOT}/mk/common.mk
