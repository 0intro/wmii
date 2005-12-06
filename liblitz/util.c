/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>

#include "blitz.h"

long long _strtonum(const char *numstr, long long minval, long long maxval)
{
	const char *errstr;
	long long ret = cext_strtonum(numstr, minval, maxval, &errstr);
	if (errstr)
		fprintf(stderr, "liblitz: cannot convert '%s' into integer: %s\n", numstr, errstr);
	return ret;
}
