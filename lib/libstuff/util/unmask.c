/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <fmt.h>
#include "util.h"

int
unmask(Fmt *f, long mask, char **table, long sep) {
	int i, nfmt;

	nfmt = f->nfmt;
	for(i=0; table[i]; i++)
		if(*table[i] && (mask & (1<<i))) {
			if(f->nfmt > nfmt)
				fmtrune(f, sep);
			if(fmtstrcpy(f, table[i]))
				return -1;
		}
	return 0;
}

