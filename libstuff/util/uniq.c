/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "util.h"

void
uniq(char **toks) {
	char **p, **q;

	q = toks;
	if(*q == nil)
		return;
	for(p=q+1; *p; p++)
		if(strcmp(*q, *p))
			*++q = *p;
	*++q = nil;
}
