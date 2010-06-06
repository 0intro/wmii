/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"

int
lprint(int fd, const char *fmt, ...) {
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = vlprint(fd, fmt, ap);
	va_end(ap);
	return res;
}

