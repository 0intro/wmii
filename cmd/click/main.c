/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <X11/extensions/XTest.h>
#include <locale.h>
#include <string.h>
#include "fns.h"

static const char version[] = "click-"VERSION", "COPYRIGHT"\n";

static void
usage(void) {
	fatal("usage: %s [window]\n", argv0);
}

static void
click(Window *w, Point p) {
	Rectangle r;
	Point rp;

	r = getwinrect(w);
	rp = subpt(r.max, p);

	XTestFakeMotionEvent(display, 0, rp.x, rp.y, 0);

	XTestFakeButtonEvent(display, 1, true, 0);
	XTestFakeButtonEvent(display, 1, false, 0);

	XTestFakeMotionEvent(display, 0, r.max.x, r.max.y, 0);
}

int
main(int argc, char *argv[]) {
	char *s;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	setlocale(LC_CTYPE, "");

	initdisplay();

	s = ARGF();
	if(s && !getulong(s, &win.xid))
		usage();
	if (!s)
		win.xid = getfocus();

	if(argc)
		usage();

	click(&win, Pt(1, 1));

	XCloseDisplay(display);
	return 0;
}

