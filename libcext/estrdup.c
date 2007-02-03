/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cext.h"

char *
cext_estrdup(const char *s)
{
	char *tmp;

	tmp = (char *) cext_emallocz(strlen(s) + 1);
	strcpy(tmp, (char *) s);

	return tmp;
}
