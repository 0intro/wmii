# wm - window manager improved 2
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../../config.mk

CFLAGS   += -I../../liblitz -I../../libixp -I../../libcext
LDFLAGS  += -L../../liblitz -llitz -L../../libixp -lixp -L../../libcext -lcext

# Solaris
# LDFLAGS += -lsocket

SRC      = area.c bar.c fs.c wm.c kb.c client.c event.c mouse.c page.c column.c 
OBJ      = ${SRC:.c=.o}

all: wmiiwm
	@echo built core window manager

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

wmiiwm: ${OBJ}
	@echo LD $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f wmiiwm *.o
