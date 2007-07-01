/* Written by Kris Maglione <fbsdaemon at gmail dot com> */
/* Public domain */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fmt.h>
#include <util.h>

static int
Vfmt(Fmt *f) {
	char *fmt;
	va_list ap;
	int i;

	fmt = va_arg(f->args, char*);
	va_copy(ap, va_arg(f->args, va_list));

	i = fmtvprint(f, fmt, ap);
	va_end(ap);
	return i;
}

void
fatal(const char *fmt, ...) {
	va_list ap;

	fmtinstall('V', Vfmt);

	va_start(ap, fmt);
	fprint(2, "%s: fatal: %V\n", argv0, fmt, ap);
	va_end(ap);

	exit(1);
}

/* Can't malloc */
static void
mfatal(char *name, uint size) {
	const char
		couldnot[] = ": fatal: Could not ",
		paren[] = "() ",
		bytes[] = " bytes\n";
	char buf[1024];
	char sizestr[8];
	int i;
	
	i = sizeof(sizestr);
	do {
		sizestr[--i] = '0' + (size%10);
		size /= 10;
	} while(size > 0);

	strlcat(buf, argv0, sizeof(buf));
	strlcat(buf, couldnot, sizeof(buf));
	strlcat(buf, name, sizeof(buf));
	strlcat(buf, paren, sizeof(buf));
	strlcat(buf, sizestr+i, sizeof(buf));
	strlcat(buf, bytes, sizeof(buf));
	write(2, buf, strlen(buf));

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
