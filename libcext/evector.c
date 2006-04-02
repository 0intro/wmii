/* extendible vectors. Icarus Sparry 2006.  Public domain. */
#include <stdlib.h>
#include <unistd.h>

#include "cext.h"

#define ERRMSG "Out of memory in cext_evector_attach\n"
void
cext_evector_attach(evector_t *v, void *p)
{
	++v->size;
	if (!(v->data = realloc(v->data, v->size * sizeof(void *)))) {
		write(2,ERRMSG,sizeof(ERRMSG)-1);
		exit(99);
	}
	v->data[v->size-1]=p;
}

void
cext_evector_detach(evector_t *v, void *data)
{
	void **p = v->data, **end;
	if (!p) return;
	for(end=p+v->size-1; p<=end; p++)
		if (*p == data) {
			for(; p<end; p++)
				p[0] = p[1]; /* Could use memmove rather than this loop */
			*end = nil;
			--v->size;
			return;
		}
}

