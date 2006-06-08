/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "cext.h"

unsigned int
cext_tokenize(char **result, unsigned int reslen, char *str, char delim)
{
	char *p, *n;
	unsigned int i = 0;

	if(!str)
		return 0;
	for(n = str; *n == delim; n++);
	p = n;
	for(i = 0; *n != 0;) {
		if(i == reslen)
			return i;
		if(*n == delim) {
			*n = 0;
			if(strlen(p))
				result[i++] = p;
			p = ++n;
		} else
			n++;
	}
	if((i < reslen) && (p < n) && strlen(p))
		result[i++] = p;
	return i;	/* number of tokens */
}
