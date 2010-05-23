/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <string.h>
#include "util.h"

char*
estrdup(const char *str) {
	void *ret = strdup(str);
	if(!ret)
		mfatal("strdup", strlen(str));
	return ret;
}
