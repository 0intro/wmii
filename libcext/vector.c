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
		fprintf(stderr, "%s\n", "Out of memory in cext_vattach\n");
		exit(1);
	}
	v->data[v->size - 1] = p;
}

void
cext_vattachat(Vector *v, void *p, unsigned int pos)
{
	cext_vattach(v, p);
	if(pos >= v->size)
		return;
	memmove(v->data + pos + 1, v->data + pos,
			(v->size - pos - 1) * sizeof(void *));
	v->data[pos] = p;
}

void
cext_vdetach(Vector *v, void *data)
{
	unsigned int i;
	for(i = 0; i < v->size; i++)
		if (v->data[i] == data) {
			memmove(v->data + i, v->data + i + 1,
					(v->size - i - 1) * sizeof(void *));
			v->data[--v->size] = nil;
			break;
		}
	if(v->size == 0) {
		free(v->data);
		v->data = nil;
	}
}
