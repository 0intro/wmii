/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "cext.h"

void **
cext_array_attach(void **array, void *p, unsigned int psize, unsigned int *size)
{
	unsigned int i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(psize * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		void **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(psize * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = p;
	return array;
}

void
cext_array_detach(void **array, void *p, unsigned int *size)
{
	unsigned int i;
	if(!array)
		return;
	for(i = 0; (i < (*size)) && array[i] && array[i] != p; i++);
	if((i >= (*size)) || !array[i])
		return; /* not found */
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}
