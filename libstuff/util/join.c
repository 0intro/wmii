/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <fmt.h>
#include "util.h"

char*
join(char **list, char *sep) {
	Fmt f;
	char **p;

	if(fmtstrinit(&f) < 0)
		abort();

	for(p=list; *p; p++) {
		if(p != list)
			fmtstrcpy(&f, sep);
		fmtstrcpy(&f, *p);
	}

	return fmtstrflush(&f);
}
