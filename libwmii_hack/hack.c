/* Copyright ©2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "hack.h"
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "util.c"
#include "x11.c"

enum {
	Timeout = 10,
};

static void*	xlib;

static long	lastwin;
static long	transient;
static Atom	types[32];
static long	ntypes;
static char**	tags;
static long	pid;
static long	stime;
static char	hostname[256];
static long	nsec;

typedef Window (*mapfn)(Display*, Window);

static Window (*mapwindow)(Display*, Window);
static Window (*mapraised)(Display*, Window);

static void
init(Display *d) { /* Hrm... assumes one display... */
	char *toks[nelem(types)];
	char *s, *p;
	long n;
	int i;

	xlib = dlopen("libX11.so", RTLD_GLOBAL | RTLD_LAZY);
	if(xlib == nil)
		return;
	mapwindow = (mapfn)(uintptr_t)dlsym(xlib, "XMapWindow");
	mapraised = (mapfn)(uintptr_t)dlsym(xlib, "XMapRaised");

	unsetenv("LD_PRELOAD");

	if((s = getenv("WMII_HACK_TRANSIENT"))) {
		if(getlong(s, &n))
			transient = n;
		unsetenv("WMII_HACK_TRANSIENT");
	}
	if((s = getenv("WMII_HACK_TYPE"))) {
		s = strdup(s);
		unsetenv("WMII_HACK_TYPE");

		n = tokenize(toks, nelem(toks), s, ',');
		for(i=0; i < n; i++) {
			for(p=toks[i]; *p; p++)
				if(*p >= 'a' && *p <= 'z')
					*p += 'A' - 'a';
			toks[i] = smprint("_NET_WM_WINDOW_TYPE_%s", toks[i]);
		}
		XInternAtoms(d, toks, n, false, types);
		ntypes = n;
		for(i=0; i < n; i++)
			free(toks[i]);
		free(s);
	}
	if((s = getenv("WMII_HACK_TAGS"))) {
		s = strdup(s);
		unsetenv("WMII_HACK_TAGS");

		n = tokenize(toks, nelem(toks)-1, s, '+');
		tags = strlistdup(toks, n);
		free(s);
	}
	if((s = getenv("WMII_HACK_TIME"))) {
		getlong(s, &stime);
		unsetenv("WMII_HACK_TIME");
	}

	pid = getpid();
	gethostname(hostname, sizeof hostname);
}

static int /* Temporary Xlib error handler */
ignoreerrors(Display *d, XErrorEvent *e) {
	//USED(d, e);
	return 0;
}

static void
setprops(Display *d, Window w) {
	long *l;

	if(!xlib)
		init(d);

	if(getprop_long(d, w, "_NET_WM_PID", "CARDINAL", 0L, &l, 1L))
		free(l);
	else {
		changeprop_long(d, w, "_NET_WM_PID", "CARDINAL", &pid, 1);
		changeprop_string(d, w, "WM_CLIENT_MACHINE", hostname);
	}

	/* Kludge. */
	if(nsec == 0)
		nsec = time(0);
	else if(time(0) > nsec + Timeout)
		return;

	if(transient)
		changeprop_long(d, w, "WM_TRANSIENT_FOR", "WINDOW", &transient, 1);
	if(ntypes)
		changeprop_long(d, w, "_NET_WM_WINDOW_TYPE", "ATOM", (long*)types, ntypes);
	if(tags)
		changeprop_textlist(d, w, "_WMII_TAGS", "UTF8_STRING", tags);
	if(stime)
		changeprop_long(d, w, "_WMII_LAUNCH_TIME", "CARDINAL", &stime, 1);
}

int
XMapWindow(Display *d, Window w) {

	setprops(d, w);
	return mapwindow(d, w);
}

int
XMapRaised(Display *d, Window w) {

	setprops(d, w);
	return mapraised(d, w);
}

