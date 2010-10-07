ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

bindings.c: keys.txt Makefile
	( echo "char binding_spec[] ="; \
	  sed 's/.*/	"&\\n"/' keys.txt; \
	  echo "	;" ) >$@

TARG =	wimenu
HFILES=	dat.h fns.h
TAGFILES= dat.h $(ROOT)/include/*.h $(ROOT)/include/stuff/*.h

PACKAGES += $(X11PACKAGES)

LIB = $(LIBS9) $(LIBIXP)
LIBS += -lm
OBJ =	main	\
	caret	\
	history	\
	menu	\
	keys	\
	bindings

include $(ROOT)/mk/one.mk

