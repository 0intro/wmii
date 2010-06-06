/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"
#include <bio.h>

int
Blprint(Biobuf *bp, const char *fmt, ...)
{
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = Bvlprint(bp, fmt, arg);
	va_end(arg);
	return n;
}

