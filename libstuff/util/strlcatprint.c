/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include <fmt.h>
#include "util.h"

int
strlcatprint(char *buf, int len, const char *fmt, ...) {
	va_list ap;
	int buflen;
	int ret;

	va_start(ap, fmt);
	buflen = strlen(buf);
	ret = vsnprint(buf+buflen, len-buflen, fmt, ap);
	va_end(ap);
	return ret;
}
