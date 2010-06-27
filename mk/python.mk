
# all: pybuild
install: $(PYMODULES:%=%.install)
clean: pyclean

.py.install:
	echo PYTHON install $* $(PYPREFIX)
	DESTDIR=$(DESTDIR); \
	$(DEBUG) $(PYTHON) $< install -cO1 --root=$${DESTDIR:-/} $(PYPREFIX)
pyclean:
	echo CLEAN build/
	rm -rf build

.PHONY: pybuild pyclean
