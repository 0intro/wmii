/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"
#include <unistd.h>

static int
fmtlfdflush(Fmt *f) {
	mbstate_t state;
	char buf[256];
	Rune *rp, *rend;
	char *sp, *send;
	int res;

	sp = buf;
	send = buf + sizeof buf - UTFmax;
	rend = f->to;
	state = (mbstate_t){0};
	for(rp=(Rune*)f->start; rp < rend; rp++) {
		res = wcrtomb(sp, *rp, &state);
		if(res == -1)
			*sp++ = '?'; /* Fixme? */
		else
			sp += res;
		if(sp >= send || rp == rend - 1) {
			if(write((uintptr_t)f->farg, buf, sp - buf) != sp - buf)
				return 0;
			sp = buf;
		}
	}
	f->to = f->start;
	return 1;
}

int
vlprint(int fd, const char *fmt, va_list args) {
	Fmt f;
	Rune buf[256];
	int res;

	if(utf8locale())
		return vfprint(fd, fmt, args);

	f.runes = 1;
	f.start = (char*)buf;
	f.to = (char*)buf;
	f.stop = (char*)(buf + nelem(buf) - 1);
	f.flush = fmtlfdflush;
	f.farg = (void*)(uintptr_t)fd;
	f.nfmt = 0;

	va_copy(f.args, args);
	res = dofmt(&f, fmt);
	va_end(f.args);
	if(res > 0 && fmtlfdflush(&f) == 0)
		return -1;
	return res;
}

