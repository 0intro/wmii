/* Copyright ©2006-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <string.h>
#include "fns.h"

static const char version[] = "witray-"VERSION", ©2007 Kris Maglione\n";

static void
usage(void) {
	fatal("usage: %s <window>\n", argv0);
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

void
debug(int flag, const char *fmt, ...) {
	va_list ap;

	USED(flag);
	va_start(ap, fmt);
	vfprint(2, fmt, ap);
	va_end(ap);
}

ErrorCode ignored_xerrors[] = { {0,} };

static Window
findframe(Window *w) {
	XWindow *children;
	XWindow xw, par, root;
	Window ret = {0, };
	uint n;

	for(par=w->xid; par != scr.root.xid; ) {
		xw = par;
		XQueryTree(display, xw, &root, &par, &children, &n);
		XFree(children);
	}
	ret.xid = xw;
	ret.parent = &scr.root;
	return ret;
}

static void
getwinsize(Window *win) {
	int x, y;
	uint w, h;
	XWindow root;
	uint border, depth;

	XGetGeometry(display, win->xid, &root,
		     &x, &y, &w, &h,
		     &border, &depth);
	win->r = rectaddpt(Rect(0, 0, w, h),
			   Pt(x+border, y+border));
}

int
main(int argc, char *argv[]) {
	char *s;

	fmtinstall('r', errfmt);
extern int fmtevent(Fmt*);
	fmtinstall('E', fmtevent);

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	s = EARGF(usage());
	if(!getulong(s, &win.xid))
		usage();

	if(argc)
		usage();

	setlocale(LC_CTYPE, "");

	initdisplay();

	frame = findframe(&win);
	getwinsize(&frame);
	restrut();
	sethandler(&frame, &handlers);
	selectinput(&frame, StructureNotifyMask);

	running = true;
	xevent_loop();

	XCloseDisplay(display);
	return 0;
}

