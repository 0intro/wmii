/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <fmt.h>
#include "util.h"

char*
join(char **list, char *sep, Fmt *f) {
	Fmt fmt;
	char **p;

	if(f == nil) {
		f = &fmt;
		if(fmtstrinit(f) < 0)
			abort();
	}

	for(p=list; *p; p++) {
		if(p != list)
			fmtstrcpy(f, sep);
		fmtstrcpy(f, *p);
	}

	if(f != &fmt)
		return nil;
	return fmtstrflush(f);
}
