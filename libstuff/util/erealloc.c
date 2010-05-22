/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <stdlib.h>
#include "util.h"

void *
erealloc(void *ptr, uint size) {
	void *ret = realloc(ptr, size);
	if(!ret)
		mfatal("realloc", size);
	return ret;
}
