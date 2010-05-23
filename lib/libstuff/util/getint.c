/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/util.h>

bool
getint(const char *s, int *ret) {
	long l;
	bool res;

	res = getlong(s, &l);
	*ret = l;
	return res;
}
