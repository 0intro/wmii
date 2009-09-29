all:

install: all

MANDIRS=$(MAN)/man1
mkdirs:
	for i in $(BIN) $(ETC) $(DOC) $(LIBDIR) $(MANDIRS) $(INCLUDE); do \
		test -d $(DESTDIR)$$i || echo MKDIR $$i; \
		mkdir -pm 0755 $(DESTDIR)$$i; \
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
