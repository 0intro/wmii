/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>

/* Misc */
static Atom
xatom(Display *display, char *name) {
	/* Blech. I don't trust Xlib's cacheing.
	MapEnt *e;
	
	e = hash_get(&amap, name, 1);
	if(e->val == nil)
		e->val = (void*)XInternAtom(display, name, False);
	return (Atom)e->val;
	*/
	return XInternAtom(display, name, False);
}
/* Properties */
#if 0
static void
delproperty(Display *display, Window w, char *prop) {
	XDeleteProperty(display, w, xatom(display, prop));
}
#endif

static void
changeproperty(Display *display, Window w, char *prop, char *type, int width, uchar data[], int n) {
	XChangeProperty(display, w, xatom(display, prop), xatom(display, type), width, PropModeReplace, data, n);
}

static void
changeprop_string(Display *display, Window w, char *prop, char *string) {
	changeprop_char(display, w, prop, "UTF8_STRING", string, strlen(string));
}

static void
changeprop_char(Display *display, Window w, char *prop, char *type, char data[], int len) {
	changeproperty(display, w, prop, type, 8, (uchar*)data, len);
}

#if 0
static void
changeprop_short(Display *display, Window w, char *prop, char *type, short data[], int len) {
	changeproperty(display, w, prop, type, 16, (uchar*)data, len);
}
#endif

static void
changeprop_long(Display *display, Window w, char *prop, char *type, long data[], int len) {
	changeproperty(display, w, prop, type, 32, (uchar*)data, len);
}

static void
changeprop_textlist(Display *display, Window w, char *prop, char *type, char *data[]) {
	char **p, *s, *t;
	int len, n;

	len = 0;
	for(p=data; *p; p++)
		len += strlen(*p) + 1;
	s = malloc(len);
	if(s == nil)
		return;
	t = s;
	for(p=data; *p; p++) {
		n = strlen(*p) + 1;
		memcpy(t, *p, n);
		t += n;
	}
	changeprop_char(display, w, prop, type, s, len);
	free(s);
}

#if 0
static void
freestringlist(char *list[]) {
	XFreeStringList(list);
}
#endif

static ulong
getprop(Display *display, Window w, char *prop, char *type, Atom *actual, int *format, ulong offset, uchar **ret, ulong length) {
	Atom typea;
	ulong n, extra;
	int status;

	typea = (type ? xatom(display, type) : 0L);

	status = XGetWindowProperty(display, w,
		xatom(display, prop), offset, length, False /* delete */,
		typea, actual, format, &n, &extra, ret);

	if(status != Success) {
		*ret = nil;
		return 0;
	}
	if(n == 0) {
		free(*ret);
		*ret = nil;
	}
	return n;
}

#if 0
static ulong
getproperty(Display *display, Window w, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length) {
	int format;

	return getprop(display, w, prop, type, actual, &format, offset, ret, length);
}
#endif

static ulong
getprop_long(Display *display, Window w, char *prop, char *type, ulong offset, long **ret, ulong length) {
	Atom actual;
	ulong n;
	int format;

	n = getprop(display, w, prop, type, &actual, &format, offset, (uchar**)ret, length);
	if(n == 0 || format == 32 && xatom(display, type) == actual)
		return n;
	free(*ret);
	*ret = 0;
	return 0;
}

#ifdef notdef
static char**
strlistdup(char *list[], int n) {
	char **p, *q;
	int i, m;

	for(i=0, m=0; i < n; i++)
		m += strlen(list[i])+1;

	p = malloc((n+1)*sizeof(char*) + m);
	if(p == nil)
		return nil;
	q = (char*)&p[n+1];

	for(i=0; i < n; i++) {
		p[i] = q;
		m = strlen(list[i])+1;
		memcpy(q, list[i], m);
		q += m;
	}
	p[n] = nil;
	return p;
}
#endif

static char**
strlistdup(char *list[], int n) {
	char **p, *q;
	int i, m;

	m = 0;
	for(i=0; i < n; i++)
		m += strlen(list[i]) + 1;

	p = malloc((n+1) * sizeof(*p) + m);
	q = (char*)&p[n+1];

	for(i=0; i < n; i++) {
		p[i] = q;
		m = strlen(list[i]) + 1;
		memcpy(q, list[i], m);
		q += m;
	}
	p[n] = nil;
	return p;
}

#if 0
static int
getprop_textlist(Display *display, Window w, char *name, char **ret[]) {
	XTextProperty prop;
	char **list;
	int n;

	n = 0;

	XGetTextProperty(display, w, &prop, xatom(display, name));
	if(prop.nitems > 0) {
		if(Xutf8TextPropertyToTextList(display, &prop, &list, &n) == Success) {
			*ret = strlistdup(list, n);
			XFreeStringList(list);
		}
		XFree(prop.value);
	}
	return n;
}
#endif

#if 0
static char*
getprop_string(Display *display, Window w, char *name) {
	char **list, *str;
	int n;

	str = nil;

	n = getprop_textlist(display, w, name, &list);
	if(n > 0)
		str = strdup(*list);
	freestringlist(list);

	return str;
}
#endif

