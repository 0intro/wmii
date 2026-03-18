
# all: pybuild
install: $(PYMODULES:%=%.install)
clean: pyclean

.py.install:
	echo PYTHON install $* $(PYPREFIX)
	DESTDIR=$(DESTDIR); \
	$(DEBUG) $(PYTHON) $< install -c --root=$${DESTDIR:-/} $(PYPREFIX)
	rm -rf build $*.egg-info
pyclean:
	echo CLEAN build/
	rm -rf build *.egg-info

.PHONY: pybuild pyclean
