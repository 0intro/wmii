PROGS = ${TARG:=.O}

all: ${OFILES} ${PROGS}

install: ${TARG:=.install}
uninstall: ${TARG:=.uninstall}
clean: manyclean

printinstall:
	echo 'Install directories:'
	echo '	Bin: ${BIN}'

manyclean:
	for i in ${TARG:=.o} ${TARG:=.O} ${OFILES}; do \
		rm $$i; \
	done 2>/dev/null || true

include ${ROOT}/mk/common.mk

