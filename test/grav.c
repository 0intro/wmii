#if 0
	set -e
	name=grav
	root=..
	obj=$root/cmd
	lib=$root/lib
	inc=$root/include
	cc -I$inc -I/usr/local/include \
		-o o.$name \
		-Wall \
		$name.c \
		$obj/util.o \
		$obj/wmii/map.o \
		$obj/wmii/x11.o \
		-L$lib -lfmt -lutf -lbio \
		-L/usr/local/lib -lX11 -lXext \

	exec o.$name
#endif
#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <util.h>
#include <x11.h>

char buffer[8196];
void debug() {}

static Window*	win;

static char*	gravity[] = {
	[NorthEastGravity] = "NorthEastGravity",
	[NorthWestGravity] = "NorthWestGravity",
	[SouthEastGravity] = "SouthEastGravity",
	[SouthWestGravity] = "SouthWestGravity",
	[StaticGravity]    = "StaticGravity",
};

static void
draw(Window *w) {
	Rectangle r;

	r = w->r;
	r = rectsubpt(r, r.min);

	fill(w, r, 0UL);
	border(w, Rect(3, 3, 97, 97), 2, ~0UL);
	border(w, Rect(8, 8, 92, 92), 2, ~0UL);
	sync();
}

static void
setgravity(Window *w, long gravity) {
	XSizeHints wmh;

	wmh.flags = PWinGravity;
	wmh.win_gravity = gravity;
	XSetWMNormalHints(display, w->w, &wmh);
}

static void
config(Window *w, long grav, Point p) {
	Rectangle r;

	r = rectsetorigin(Rect(0, 0, 100, 100), p);

	print("%s: %R\n", gravity[grav], r);

	setgravity(w, grav);
	w->r = ZR; /* Kludge. */
	reshapewin(w, r);
	draw(w);
	sleep(1);
}

int
main(void) {
	XSizeHints wmh;
	WinAttr wa;
	XEvent ev;

	initdisplay();

	/* Kludge the bar height. */
	scr.rect.max.y -= 14;

	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask|StructureNotifyMask;
	win = createwindow(&scr.root,
				Rect(0, 0, 100, 100), scr.depth, InputOutput,
				&wa, CWEventMask | CWBackPixmap);
	XSelectInput(display, win->w, ExposureMask);

	wmh.flags = PMinSize|PMaxSize|USPosition;
	wmh.min_width = wmh.max_width = 100;
	wmh.min_height = wmh.max_height = 100;
	XSetWMNormalHints(display, win->w, &wmh);

	mapwin(win);
	raisewin(win);
	XMaskEvent(display, ExposureMask, &ev);

	draw(win);
	sleep(2);

	config(win, StaticGravity, Pt(0, 0));

	config(win, NorthWestGravity,
		    Pt(0,
		       0));

	config(win, NorthEastGravity,
		    Pt(Dx(scr.rect) - 100,
		       0));

	config(win, SouthEastGravity,
		    Pt(Dx(scr.rect) - 100,
		       Dy(scr.rect) - 100));

	config(win, SouthWestGravity,
		    Pt(0,
		       Dy(scr.rect) - 100));

	sleep(1);
	return 0;
}

