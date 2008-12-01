/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <X11/extensions/XTest.h>
#include <locale.h>
#include <string.h>
#include "fns.h"

static const char version[] = "click-"VERSION", ©2008 Kris Maglione\n";

static void
usage(void) {
	fatal("usage: %s [window]\n", argv0);
}

static void
click(Window *w, Point p) {
	Point rp, restore;

	restore = querypointer(&scr.root);

	rp = translate(w, &scr.root, p);

	XTestFakeMotionEvent(display, 0, rp.x, rp.y, 0);

	XTestFakeButtonEvent(display, 1, true, 0);
	XTestFakeButtonEvent(display, 1, false, 0);

	rp = getwinrect(w).max;
	XTestFakeMotionEvent(display, 0, rp.x - 1, rp.y - 1, 0);

	return;
	XButtonEvent e = { 0, };

	e.root = scr.root.w;
	e.window = w->w;
	e.same_screen = true;

	e.x = p.x;
	e.y = p.y;
	e.x_root = rp.x;
	e.y_root = rp.y;
	e.button = 1; /* Hopefully ignored, except enough to do the trick. */

	e.type = ButtonPress;
	sendevent(w, true, ButtonPressMask, (XEvent*)&e);
	e.type = ButtonRelease;
	e.state = Button1Mask;
	sendevent(w, true, ButtonReleaseMask, (XEvent*)&e);
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
	if(s && !getulong(s, &win.w))
		usage();
	if (!s)
		win.w = getfocus();

	if(argc)
		usage();

	click(&win, Pt(0, 0));

	XCloseDisplay(display);
	return 0;
}

