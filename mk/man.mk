
targ = for k in $(MANPAGES); do echo $$k | sed 's/ .*//'; done
TARG := $(shell $(targ))
TARG != $(targ)

all: $(TARG)
install: $(TARG:.1=.install) $(TARG:.3=.install) maninstall
uninstall: $(TARG:.1=.uninstall) $(TARG:.3=.uninstall) manuninstall

.PHONY: maninstall manuninstall

MANLOOP = \
	set -ef; \
	for k in $(MANPAGES); do \
		set -- $$k; \
		real=$$1; shift; \
		for targ; do \
			_ $$real $(MAN)/man$${real\#\#*.}/$$targ; \
		done; \
	done
maninstall:
	_() { echo LN $$1 $${2##*/}; ln -sf $$1 $(DESTDIR)$$2; }; $(MANLOOP)
manuninstall:
	_() { echo RM $${2##*/}; rm -f $(DESTDIR)$$2; }; $(MANLOOP)

printinstall:
	echo 'Install directories:'
	echo '	Man: $(MAN)'

