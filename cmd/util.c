/* Written by Kris Maglione <fbsdaemon at gmail dot com> */
/* Public domain */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

void
fatal(const char *fmt, ...) {
	va_list ap;
	int err;

	err = errno;
	fprintf(stderr, "%s: fatal: ", argv0);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s\n", strerror(err));
	else
		fprintf(stderr, "\n");

	exit(1);
}

/* Can't malloc */
static void
mfatal(char *name, uint size) {
	const char
		couldnot[] = ": fatal: Could not ",
		paren[] = "() ",
		bytes[] = " bytes\n";
	char sizestr[8];
	int i;
	
	i = sizeof(sizestr);
	do {
		sizestr[--i] = '0' + (size%10);
		size /= 10;
	} while(size > 0);

	write(1, argv0, strlen(argv0)-1);
	write(1, couldnot, sizeof(couldnot)-1);
	write(1, name, strlen(name));
	write(1, paren, sizeof(paren)-1);
	write(1, sizestr+i, sizeof(sizestr)-i);
	write(1, bytes, sizeof(bytes)-1);

	exit(1);
}

void *
emalloc(uint size) {
	void *ret = malloc(size);
	if(!ret)
		mfatal("malloc", size);
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
		mfatal("realloc", size);
	return ret;
}

char *
estrdup(const char *str) {
	void *ret = strdup(str);
	if(!ret)
		mfatal("strdup", strlen(str));
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

int
min(int a, int b) {
	if(a < b)
		return a;
	return b;
}

char *
str_nil(char *s) {
	if(s)
		return s;
	return "<nil>";
}

uint
strlcat(char *dst, const char *src, uint size) {
	const char *s;
	char *d;
	int n, len;

	d = dst;
	s = src;
	n = size;
	while(n-- > 0 && *d != '\0')
		d++;
	len = n;

	while(*s != '\0') {
		if(n-- > 0)
			*d++ = *s;
		s++;
	}
	if(len > 0)
		*d = '\0';
	return size - n - 1;
}
