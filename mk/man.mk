install: ${TARG:.1=.install}

printinstall:
	echo 'Install directories:'
	echo '	Man: ${MAN}'

include ${ROOT}/mk/common.mk
