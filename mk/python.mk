
# all: pybuild
install: $(PYMODULES:%=%.install)
clean: pyclean

.py.install:
	echo PYTHON install $*
	DESTDIR=$(DESTDIR); \
	cp -f $*.py setup.py && \
	$(DEBUG) $(PYTHON) -m pip install --root=$${DESTDIR:-/} $(PYPREFIX) --no-deps --no-build-isolation --ignore-installed . ; \
	rm -f setup.py
	rm -rf build $*.egg-info *.dist-info
pyclean:
	echo CLEAN build/
	rm -rf build *.egg-info

.PHONY: pybuild pyclean
