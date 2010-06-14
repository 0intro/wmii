/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
/*
 * Plan 9 port version must include libc.h in order to
 * get Plan 9 debugging malloc, which sometimes returns
 * different pointers than the standard malloc.
 */
#include <stdlib.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

static int
fmtStrFlush(Fmt *f)
{
	char *s;
	int n;

	if(f->start == nil)
		return 0;
	n = (uintptr_t)f->farg;
	n *= 2;
	s = (char*)f->start;
	f->start = realloc(s, n);
	if(f->start == nil){
		f->farg = nil;
		f->to = nil;
		f->stop = nil;
		free(s);
		return 0;
	}
	f->farg = (void*)(uintptr_t)n;
	f->to = (char*)f->start + ((char*)f->to - s);
	f->stop = (char*)f->start + n - 1;
	return 1;
}

int
fmtstrinit(Fmt *f)
{
	int n;

	memset(f, 0, sizeof *f);
	f->runes = 0;
	n = 32;
	f->start = malloc(n);
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (char*)f->start + n - 1;
	f->flush = fmtStrFlush;
	f->farg = (void*)(uintptr_t)n;
	f->nfmt = 0;
	return 0;
}

/*
 * print into an allocated string buffer
 */
char*
vsmprint(const char *fmt, va_list args)
{
	Fmt f;
	int n;

	if(fmtstrinit(&f) < 0)
		return nil;
	va_copy(f.args,args);
	n = dofmt(&f, fmt);
	va_end(f.args);
	if(n < 0){
		free(f.start);
		return nil;
	}
	return fmtstrflush(&f);
}
