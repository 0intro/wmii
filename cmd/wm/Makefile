# wm - window manager improved 2
#   (C)opyright MMIV-MMVI Anselm R. Garbe

include ../../config.mk

all: layoutdef.c
	@${MAKE} -f Makefile.wm
	@echo built core wm

layoutdef.c:
	@echo SH mklayout.sh
	@sh mklayout.sh

clean:
	rm -f wmiiwm *.o layoutdef.*
