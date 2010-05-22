/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

void *
emalloc(uint size) {
	void *ret = malloc(size);
	if(!ret)
		mfatal("malloc", size);
	return ret;
}
