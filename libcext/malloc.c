/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cext.h"

static void
bad_malloc(unsigned int size)
{
	fprintf(stderr, "fatal: could not malloc() %d bytes\n",
			(int) size);
	exit(1);
}

void *
cext_emallocz(unsigned int size)
{
	void *res = calloc(1, size);
	if(!res)
		bad_malloc(size);
	return res;
}

void *
cext_emalloc(unsigned int size)
{
	void *res = malloc(size);
	if(!res)
		bad_malloc(size);
	return res;
}

void *
cext_erealloc(void *ptr, unsigned int size)
{
	void *res = realloc(ptr, size);
	if(!res)
		bad_malloc(size);
	return res;
}

char *
cext_estrdup(const char *str)
{
	void *res = strdup(str);
	if(!res)
		bad_malloc(strlen(str));
	return res;
}
