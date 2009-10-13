all:

install: all simpleinstall
uninstall: simpleuninstall

DOCDIR = $(DOC)
simpleinstall:
	for f in $(DOCS); do \
		$(INSTALL) 0644 $$f $(DOCDIR)/$$f; \
	done
	for f in $(TEXT); do \
		$(INSTALL) 0644 $$f $(DIR)/$$f; \
	done
	for f in $(BINARY); do \
		$(INSTALL) -b 0644 $$f $(DIR)/$$f; \
	done
	for f in $(EXECS); do \
		$(INSTALL) -b 0755 $$f $(DIR)/$$f; \
	done

cleandep:
	echo CLEANDEP
	rm .depend 2>/dev/null || true

tags:
	files=; \
	for f in $(OBJ); do \
		[ -f "$$f.c" ] && files="$$files $$f.c"; \
	done; \
	echo CTAGS $$files $(TAGFILES) || \
	ctags $$files $(TAGFILES)

DEP:=${shell if test -f .depend;then echo .depend;else echo /dev/null; fi}
DEP!=echo /dev/null
include $(DEP)

.PHONY: all options clean dist install uninstall depend cleandep tags
.PHONY: simpleuninstall simpleinstall
