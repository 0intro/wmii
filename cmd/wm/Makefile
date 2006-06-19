# wm - window manager improved 2
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../../config.mk

CFLAGS   += -I../../liblitz -I../../libixp -I../../libcext
LDFLAGS  += -L../../liblitz -llitz -L../../libixp -lixp -L../../libcext -lcext -lm

# Solaris
# LDFLAGS += -lsocket

SRC      = area.c bar.c client.c column.c event.c frame.c fs.c geom.c \
			key.c mouse.c rule.c view.c wm.c
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
