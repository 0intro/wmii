# window manager improved 2 testcases
#   (C)opyright MMIV-MMV Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libwmii -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp \
	-L../libwmii -lwmii -L../libcext -lcext

SRC_container = test_container.c
OBJ_container = ${SRC_container:.c=.o}

all: test_container
	@echo built test cases

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

test_container: ${OBJ_container}
	@echo LD $@
	@${CC} -o $@ ${OBJ_container} ${LDFLAGS}

clean:
	rm -f test_container *.o
