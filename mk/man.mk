
all: $(TARG)
install: ${TARG:.1=.install}
uninstall: ${TARG:.1=.uninstall}

printinstall:
	echo 'Install directories:'
	echo '	Man: $(MAN)'

