/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cext.h"

char           *
estrdup(const char *s)
{
	char           *tmp;

	tmp = (char *) emalloc(strlen(s) + 1);
	strcpy(tmp, (char *) s);

	return tmp;
}
