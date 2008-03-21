/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <string.h>
#include "fns.h"

static const char version[] = "witray-"VERSION", ©2007 Kris Maglione\n";

static int (*xlib_errorhandler) (Display*, XErrorEvent*);

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

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotifies).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
struct {
	uchar rcode;
	uchar ecode;
} itab[] = {
	{ 0, BadWindow },
	{ X_SetInputFocus, BadMatch },
	{ X_PolyText8, BadDrawable },
	{ X_PolyFillRectangle, BadDrawable },
	{ X_PolySegment, BadDrawable },
	{ X_ConfigureWindow, BadMatch },
	{ X_GrabKey, BadAccess },
	{ X_GetAtomName, BadAtom },
};

static int
errorhandler(Display *dpy, XErrorEvent *error) {
	int i;

	USED(dpy);

	if(error->request_code == X_QueryTree
	&& error->error_code == BadWindow
	&& error->resourceid == win.w)
		fatal("%W: window does not exist", &win);

	for(i = 0; i < nelem(itab); i++)
		if((itab[i].rcode == 0 || itab[i].rcode == error->request_code)
		&& (itab[i].ecode == 0 || itab[i].ecode == error->error_code))
			return 0;

	fprint(2, "%s: fatal error: Xrequest code=%d, Xerror code=%d\n",
			argv0, error->request_code, error->error_code);

	return xlib_errorhandler(display, error); /* calls exit() */
}

static Window
findframe(Window *w) {
	XWindow *children;
	XWindow xw, par, root;
	Window ret = {0, };
	uint n;

	for(par=w->w; par != scr.root.w; ) {
		xw = par;
		XQueryTree(display, xw, &root, &par, &children, &n);
		XFree(children);
	}
	ret.w = xw;
	ret.parent = &scr.root;
	return ret;
}

static void
getwinsize(Window *win) {
	int x, y;
	uint w, h;
	/* ignored */
	XWindow root;
	uint border, depth;

	XGetGeometry(display, win->w, &root,
		     &x, &y, &w, &h,
		     &border, &depth);
	win->r = rectaddpt(Rect(0, 0, w, h),
			   Pt(x, y));
}

int
main(int argc, char *argv[]) {
	char *s;

	fmtinstall('r', errfmt);

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	s = EARGF(usage());
	if(!getulong(s, &win.w))
		usage();

	if(argc)
		usage();

	setlocale(LC_CTYPE, "");

	initdisplay();
	xlib_errorhandler = XSetErrorHandler(errorhandler);

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

