#include "fmtdef.h"

Fmt
fmtbuf(char *buf, int len) {
	Fmt f;

	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = buf + len - 1;
	f.flush = 0;
	f.farg = nil;
	f.nfmt = 0;
	return f;
}
