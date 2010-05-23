/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "util.h"


void
grep(char **list, Reprog *re, int flags) {
	char **p, **q;
	int res;

	q = list;
	for(p=q; *p; p++) {
		res = 0;
		if(re)
			res = regexec(re, *p, nil, 0);
		if(res && !(flags & GInvert)
		|| !res && (flags & GInvert))
			*q++ = *p;
	}
	*q = nil;
}
