/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include <stuff/util.h>

bool
getlong(const char *s, long *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	if(s == nil)
		return false;
	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign == 0)
		return false;

	*ret = sign * strtol(s, &rend, base);
	return (end == rend);
}
