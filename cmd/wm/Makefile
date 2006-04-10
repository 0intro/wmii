# wm - window manager improved 2
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../../config.mk

CFLAGS   += -I../../liblitz -I../../libixp -I../../libcext
LDFLAGS  += -L../../liblitz -llitz -L../../libixp -lixp -L../../libcext -lcext -lm

# Solaris
# LDFLAGS += -lsocket

SRC      = area.c bar.c fs.c frame.c wm.c kb.c client.c event.c mouse.c rule.c tag.c view.c
OBJ      = ${SRC:.c=.o}

all: wmiiwm
	@echo built core window manager

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: wm.h

wmiiwm: ${OBJ}
	@echo LD $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f wmiiwm *.o
