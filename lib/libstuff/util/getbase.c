/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <ctype.h>
#include <string.h>
#include <stuff/util.h>

#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))
int
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
