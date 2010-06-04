/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"
#include <sys/time.h>

uvlong
nsec(void) {
	struct timeval tv;

	gettimeofday(&tv, nil);
	return (uvlong)tv.tv_sec * 1000000000 + (uvlong)tv.tv_usec * 1000;
}

