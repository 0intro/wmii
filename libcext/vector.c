/* extendible vectors. Icarus Sparry 2006.  Public domain. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cext.h"

void
cext_vattach(Vector *v, void *p)
{
	++v->size;
	if (!(v->data = realloc(v->data, v->size * sizeof(void *)))) {
		fprintf(stderr, "%s\n", "Out of memory in cext_evector_attach\n");
		exit(1);
	}
	v->data[v->size - 1]=p;
}

void
cext_vdetach(Vector *v, void *data)
{
	unsigned int i;
	for(i = 0; i < v->size; i++)
		if (v->data[i] == data) {
			memmove(v->data + i, v->data + i + 1,
					(v->size - i - 1) * sizeof(void *));
			--v->size;
			return;
		}
	if(v->size == 0)
		free(v->data);
}
