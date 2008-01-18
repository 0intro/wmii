#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))	
static int
getbase(const char **s) {
	const char *p;

	p = *s;
	if(!strbcmp(p, "0x")) {
		*s += 2;
		return 16;
	}
	if(isdigit(p[0])) {
		if(p[1] == 'r') {
			*s += 2;
			return p[0] - '0';
		}
		if(isdigit(p[1]) && p[2] == 'r') {
			*s += 3;
			return 10*(p[0]-'0') + (p[1]-'0');
		}
	}
	if(p[0] == '0') {
		*s += 1;
		return 8;
	}
	return 10;
}

static int
getlong(const char *s, long *ret) {
	const char *end;
	char *rend;
	int base;

	end = s+strlen(s);
	base = getbase(&s);

	*ret = strtol(s, &rend, base);
	return (end == rend);
}

static uint
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

