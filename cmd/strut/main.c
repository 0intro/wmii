/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <string.h>
#include <time.h>
#include "fns.h"

static Window*	testwin;
static ulong	testtime[2];

static const char version[] = "wistrut-"VERSION", "COPYRIGHT"\n";

static void manage(ulong);

static void
usage(void) {
	fatal("usage: %s [-HV] <window|class>...\n", argv0);
}

static void
search_wins(char *pattern) {
	ulong *wins;
	ulong n, num;
	int i;
	char **class;
	Reprog *regexp;
	Window *win;

	regexp = regcomp(pattern);

	num = getprop_ulong(&scr.root, "_NET_CLIENT_LIST", "WINDOW", 0L, &wins, 1024L);
	for(i = 0; i < num; i++) {
		win = window(wins[i]);

		n = getprop_textlist(win, "WM_CLASS", &class);
		bufclear();
		bufprint("%s:%s:%s",
			 (n > 0 ? class[0] : "<nil>"),
			 (n > 1 ? class[1] : "<nil>"),
			 freelater(windowname(win)));
		freestringlist(class);
		if(regexec(regexp, buffer, nil, 0))
			manage(wins[i]);
	}
	free(wins);
}

static Window
findframe(Window *w) {
	XWindow *children;
	XWindow xw, par, root;
	Window ret = {0, };
	uint n;

	xw = w->xid;
	for(par=w->xid; par != scr.root.xid; ) {
		xw = par;
		XQueryTree(display, xw, &root, &par, &children, &n);
		XFree(children);
	}
	ret.type = WWindow;
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

static bool
managable(ulong xid) {
	ulong *ret;
	ulong n;
	bool retval;

	n = getprop_ulong(window(xid), "_WMII_STRUT", "WINDOW", 0L, &ret, 1L);
	if(n < 0)
		retval = true;
	else {
		if(ret[0] == xid)
			retval = ret[0] != testtime[0] || ret[1] != testtime[1];
		else
			retval = managable(ret[0]);
	}
	free(ret);
	return retval;
}

static void
manage(ulong xid) {
	Window *frame;
	Window *win;

	if(!managable(xid))
		return;

	win = emallocz(sizeof *win);
	frame = emalloc(sizeof *frame);

	win->type = WWindow;
	win->xid = xid;
	*frame = findframe(win);
	frame->aux = win;

	getwinsize(frame);
	restrut(frame);
	sethandler(frame, &handlers);
	selectinput(frame, StructureNotifyMask);

	changeprop_ulong(frame, "_WMII_STRUT", "WINDOW", testtime, nelem(testtime));
}

int
main(int argc, char *argv[]) {
	ulong win;
	char *s;

	setlocale(LC_CTYPE, "");
	fmtinstall('E', fmtevent);

	ARGBEGIN{
	case 'H':
		direction = DHorizontal;
		break;
	case 'V':
		direction = DVertical;
		break;
	case 'v':
		lprint(1, "%s", version);
		return 0;
	default:
		usage();
	}ARGEND;

	initdisplay();

	testwin = createwindow(&scr.root, Rect(0, 0, 1, 1), 0,
			    InputOnly, nil, 0);
	testtime[0] = testwin->xid;
	testtime[1] = time(nil);

	while(argc) {
		s = ARGF();
		if(getulong(s, &win))
			manage(win);
		else
			search_wins(s);
	}

	changeprop_ulong(testwin, "_WMII_STRUT", "WINDOW", testtime, nelem(testtime));

	event_looprunning = windowmap.nmemb > 0;
	event_loop();

	XCloseDisplay(display);
	return 0;
}

