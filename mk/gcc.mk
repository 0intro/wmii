CFLAGS += \
	-std=c99 \
	-pedantic \
	-pipe \
	-Wall \
	-Wimplicit \
	-Wmissing-prototypes \
	-Wno-comment \
	-Wno-missing-braces \
	-Wno-parentheses \
	-Wno-sign-compare \
	-Wno-switch \
	-Wpointer-arith \
	-Wreturn-type \
	-Wstrict-prototypes \
	-Wtrigraphs
MKDEP = cpp -M
SOCFLAGS += -fPIC
SOLDFLAGS += -shared -soname $(SONAME)

