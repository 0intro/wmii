/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>

#include "cext.h"

void *
cext_emallocz(unsigned int size)
{
	void *res = calloc(1, size);

	if(!res) {
		fprintf(stderr, "fatal: could not malloc() %d bytes\n",
				(int) size);
		exit(1);
	}
	return res;
}
