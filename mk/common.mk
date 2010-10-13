all:

install: all simpleinstall
uninstall: simpleuninstall

DOCDIR = $(DOC)
simpleinstall:
	for f in $(DOCS); do \
		$(INSTALL) 0644 $$f $(DOCDIR) $$f; \
	done
	for f in $(TEXT); do \
		$(INSTALL) 0644 $$f $(DIR) $$f; \
	done
	for f in $(BINARY); do \
		$(INSTALL) -b 0644 $$f $(DIR) $$f; \
	done
	for f in $(EXECS); do \
		$(INSTALL) 0755 $$f $(DIR) $$f; \
	done

simpleuninstall:
	for f in $(DOCS); do \
		$(UNINSTALL) $$f $(DOCDIR) $$f; \
	done
	for f in $(TEXT); do \
		$(UNINSTALL) $$f $(DIR) $$f; \
	done
	for f in $(BINARY); do \
		$(UNINSTALL) -b $$f $(DIR) $$f; \
	done
	for f in $(EXECS); do \
		$(UNINSTALL) -b $$f $(DIR) $$f; \
	done

cleandep:
	echo CLEANDEP
	rm .depend 2>/dev/null || true

tags:
	files=; \
	for f in $(OBJ); do \
		[ -f "$$f.c" ] && files="$$files $$f.c"; \
	done; \
	echo CTAGS $$files $(TAGFILES); \
	if [ -n "$$files" ]; then $(DEBUG) $(CTAGS) $$files $(TAGFILES); fi

.PHONY: all options clean dist install uninstall depend cleandep tags
.PHONY: simpleuninstall simpleinstall
