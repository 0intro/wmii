#include "fmtdef.h"

void
vseprint(Fmt *f, char *buf, char *e) {
	Fmt f;

	if(e <= buf)
		return nil;
	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = e - 1;
	f.flush = 0;
	f.farg = nil;
	f.nfmt = 0;
	va_copy(f.args,args);
	dofmt(&f, fmt);
	va_end(f.args);
	*(char*)f.to = '\0';
	return (char*)f.to;
}
