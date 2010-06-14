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
	int dc, sc;
        int len;

	len = strlen(src) - 1;
	for(; (sc = *src) && *dst; src++) {
		sc = tolower(dc);
		for(; (dc = *dst); dst++) {
			dc = tolower(dc);
			if(sc == dc && !strncasecmp(dst+1, src+1, len))
				return (char*)(uintptr_t)dst;
		}
	}
	return nil;
}
