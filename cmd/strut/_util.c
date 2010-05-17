/* Copyright ©2008-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include <string.h>
#include "fns.h"

#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))	
static int
getbase(const char **s, long *sign) {
	const char *p;
	int ret;

	ret = 10;
	*sign = 1;
	if(**s == '-') {
		*sign = -1;
		*s += 1;
	}else if(**s == '+')
		*s += 1;

	p = *s;
	if(!strbcmp(p, "0x")) {
		*s += 2;
		ret = 16;
	}
	else if(isdigit(p[0])) {
		if(p[1] == 'r') {
			*s += 2;
			ret = p[0] - '0';
		}
		else if(isdigit(p[1]) && p[2] == 'r') {
			*s += 3;
			ret = 10*(p[0]-'0') + (p[1]-'0');
		}
	}
	else if(p[0] == '0') {
		ret = 8;
	}
	if(ret != 10 && (**s == '-' || **s == '+'))
		*sign = 0;
	return ret;
}

bool
getlong(const char *s, long *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign == 0)
		return false;

	*ret = sign * strtol(s, &rend, base);
	return (end == rend);
}

bool
getulong(const char *s, ulong *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign < 1)
		return false;

	*ret = strtoul(s, &rend, base);
	return (end == rend);
}

