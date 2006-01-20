/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>

#include "blitz.h"

long long blitz_strtonum(const char *numstr, long long minval, long long maxval)
{
	const char *errstr;
	long long ret = cext_strtonum(numstr, minval, maxval, &errstr);
	if (errstr)
		fprintf(stderr, "liblitz: cannot convert '%s' into integer: %s [%lld..%lld]\n", numstr, errstr, minval, maxval);
	return ret;
}
