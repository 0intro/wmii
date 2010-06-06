/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"
#include <bio.h>

static int
fmtBlflush(Fmt *f)
{
	mbstate_t state;
	Biobuf *bp;
	Rune *rp, *rend;
	int res;

	bp = f->farg;
	rend = f->to;
	state = (mbstate_t){0};
	for(rp=(Rune*)f->start; rp < rend; rp++) {
		if(MB_LEN_MAX + bp->ocount > 0 && Bflush(bp) < 0)
			return 0;

		res = wcrtomb((char*)bp->ebuf + bp->ocount, *rp, &state);
		if(res == -1)
			Bputc(bp, '?');
		else
			bp->ocount += res;
	}
	f->to = f->start;
	return 1;
}

int
Bvlprint(Biobuf *bp, const char *fmt, va_list args)
{
	Fmt f;
	Rune buf[256];
	int res;

	if(utf8locale())
		return Bvprint(bp, fmt, args);

	f.runes = 1;
	f.start = (char*)buf;
	f.to = (char*)buf;
	f.stop = (char*)(buf + nelem(buf) - 1);
	f.flush = fmtBlflush;
	f.farg = bp;
	f.nfmt = 0;

	va_copy(f.args, args);
	res = dofmt(&f, fmt);
	va_end(f.args);
	if(res > 0 && fmtBlflush(&f) == 0)
		return -1;
	return res;
}

