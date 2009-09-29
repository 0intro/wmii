ROOT=..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

BIN = $(ETC)/wmii$(CONFVERSION)
TARG =	python	\
	plan9port \
	ruby

$(TARG:%=%.install):
	echo INSTALL $$($(CLEANNAME) $(BASE)${@:.install=})
	cp -r ${@:.install=} $(DESTDIR)$(BIN)
$(TARG:%=%.uninstall):
	echo UNINSTALL $$($(CLEANNAME) $(BASE)${@:.uninstall=})
	rm -rf $(DESTDIR)$(BIN)/${@:.uninstall=}

.PHONY: $(TARG:%=%.install) $(TARG:%=%.uninstall)

install: $(TARG:%=%.install)
uninstall: $(TARG:%=%.uninstall)

