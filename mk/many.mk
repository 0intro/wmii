PROGS = ${TARG:=.O}

all: ${PROGS}

install: ${TARG:=.install}
clean: manyclean

printinstall:
	echo 'Install directories:'
	echo '	Bin: ${BIN}'

manyclean:
	for i in ${TARG}; do \
		rm $$i.o; rm $$i.O; \
	done 2>/dev/null || true

include ${ROOT}/mk/common.mk

