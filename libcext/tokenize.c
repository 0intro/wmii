/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "cext.h"

size_t tokenize(char **result, size_t reslen, char *str, char delim)
{
	char *p, *n;
	size_t i = 0;

	if (!str)
		return 0;
	for (n = str; *n == ' '; n++);
	p = n;
	for (i = 0; *n != '\0';) {
		if (i == reslen)
			return i;
		if (*n == delim) {
			*n = '\0';
			if (strlen(p))
				result[i++] = p;
			p = ++n;
		} else
			n++;
	}
	if (strlen(p))
		result[i++] = p;
	return i;					/* number of tokens */
}
