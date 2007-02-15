/* Written by Kris Maglione <fbsdaemon at gmail dot com> */
/* Public domain */
#include "wmii.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
fatal(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}

void *
emalloc(uint size) {
	void *ret = malloc(size);
	if(!ret)
		fatal("fatal: could not malloc() %d bytes\n", size);
	return ret;
}

void *
emallocz(uint size) {
	void *ret = emalloc(size);
	memset(ret, 0, size);
	return ret;
}

void *
erealloc(void *ptr, uint size) {
	void *ret = realloc(ptr, size);
	if(!ret)
		fatal("fatal: could not realloc() %d bytes\n", size);
	return ret;
}

char *
estrdup(const char *str) {
	void *ret = strdup(str);
	if(!ret)
		fatal("fatal: could not strdup() %u bytes\n", strlen(str));
	return ret;
}

uint
tokenize(char *res[], uint reslen, char *str, char delim) {
	char *s;
	uint i;

	i = 0;
	s = str;
	while(i < reslen && *s) {
		while(*s == delim)
			*(s++) = '\0';
		if(*s)
			res[i++] = s;
		while(*s && *s != delim)
			s++;
	}
	return i;
}

int
max(int a, int b) {
	if(a > b)
		return a;
	return b;
}

char *
str_nil(char *s) {
	if(s)
		return s;
	return "<nil>";
}
