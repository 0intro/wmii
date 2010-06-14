/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <string.h>
#include "util.h"

char*
estrndup(const char *str, uint len) {
	char *ret;

	len = min(len, strlen(str));
	ret = emalloc(len + 1);
	memcpy(ret, str, len);
	ret[len] = '\0';
	return ret;
}
