ROOT= ../..
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/wmii.mk

main.c: $(ROOT)/mk/wmii.mk

bindings.c: keys.txt Makefile
	( echo "char binding_spec[] = "; \
	  sed 's/.*/	"&\\n"/' keys.txt; \
	  echo "	;" ) >bindings.c

TARG =	wimenu
HFILES=	dat.h fns.h

PACKAGES += $(X11PACKAGES) xext xrandr xrender xinerama

LIB =	$(LIBIXP)
LDFLAGS += -lm $(LIBS9)
CFLAGS += -DIXP_NEEDAPI=86
OBJ =	main	\
	caret	\
	history	\
	event	\
	menu	\
	keys	\
	bindings \
	../wmii/geom	\
	../wmii/map	\
	../wmii/printevent	\
	../wmii/x11	\
	../wmii/xext	\
	../clientutil	\
	../util

include $(ROOT)/mk/one.mk

