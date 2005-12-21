# window manager improved 2 testcases
#   (C)opyright MMIV-MMV Anselm R. Garbe

include ../config.mk

CFLAGS += -I../liblitz -I../libixp -I../libwmii -I../libcext
LDFLAGS += -L../liblitz -llitz -L../libixp -lixp \
	-L../libwmii -lwmii -L../libcext -lcext

all: 
	@echo built test cases

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

clean:
	rm -f test_container *.o
