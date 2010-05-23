#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char*
vsmprint(const char *fmt, va_list ap) {
	va_list al;
	char *buf = "";
	int n;

	va_copy(al, ap);
	n = vsnprintf(buf, 0, fmt, al);
	va_end(al);

	buf = malloc(++n);
	if(buf)
		vsnprintf(buf, n, fmt, ap);
	return buf;
}

static char*
smprint(const char *fmt, ...) {
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = vsmprint(fmt, ap);
	va_end(ap);
	return ret;
}

static char*
strdup(const char *s) {
	char *ret;
	int len;

	len = strlen(s)+1;
	ret = malloc(len);
	if(ret)
		memcpy(ret, s, len);
	return ret;
}

