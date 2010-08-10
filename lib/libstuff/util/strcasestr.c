/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "util.h"

/* TODO: Make this UTF-8 compliant. */
char*
strcasestr(const char *dst, const char *src) {
	int len, dc, sc;

	if(src[0] == '\0')
		return (char*)(uintptr_t)dst;

	len = strlen(src) - 1;
	sc  = tolower(src[0]);
	for(; (dc = *dst); dst++) {
		dc = tolower(dc);
		if(sc == dc && (len == 0 || !strncasecmp(dst+1, src+1, len)))
			return (char*)(uintptr_t)dst;
	}
	return nil;
}
