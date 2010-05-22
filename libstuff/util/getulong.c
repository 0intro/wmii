/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <string.h>
#include <stuff/util.h>

bool
getulong(const char *s, ulong *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	if(s == nil)
		return false;
	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign < 1)
		return false;

	*ret = strtoul(s, &rend, base);
	return (end == rend);
}
