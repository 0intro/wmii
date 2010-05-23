/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

void*
freelater(void *p) {
	static char*	obj[16];
	static long	nobj;
	int id;

	id = nobj++ % nelem(obj);
	free(obj[id]);
	obj[id] = p;
	return p;
}
