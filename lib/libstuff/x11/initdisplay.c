/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

int	(*xlib_errorhandler) (Display*, XErrorEvent*);

Map	windowmap;
Map	atommap;
Map	atomnamemap;
static MapEnt*	wbucket[137];
static MapEnt*	abucket[137];
static MapEnt*	anamebucket[137];

static int
Afmt(Fmt *f) {

	return fmtstrcpy(f, atomname(va_arg(f->args, Atom)));
}

static int
Pfmt(Fmt *f) {
	Point p;

	p = va_arg(f->args, Point);
	return fmtprint(f, "(%d,%d)", p.x, p.y);
}

static int
Rfmt(Fmt *f) {
	Rectangle r;

	r = va_arg(f->args, Rectangle);
	return fmtprint(f, "%P+%dx%d", r.min, Dx(r), Dy(r));
}

static int
Wfmt(Fmt *f) {
	Window *w;

	w = va_arg(f->args, Window*);
	if(w == nil)
		return fmtstrcpy(f, "<nil>");
	return fmtprint(f, "0x%ulx", w->xid);
}

void
initdisplay(void) {
	display = XOpenDisplay(nil);
	if(display == nil)
		fatal("Can't open display");
	scr.screen = DefaultScreen(display);
	scr.colormap = DefaultColormap(display, scr.screen);
	scr.visual = DefaultVisual(display, scr.screen);
	scr.visual32 = DefaultVisual(display, scr.screen);
	scr.gc = DefaultGC(display, scr.screen);
	scr.depth = DefaultDepth(display, scr.screen);
	
	scr.white = WhitePixel(display, scr.screen);
	scr.black = BlackPixel(display, scr.screen);
	
	scr.root.xid = RootWindow(display, scr.screen);
	scr.root.r = Rect(0, 0,
			  DisplayWidth(display, scr.screen),
			  DisplayHeight(display, scr.screen));
	scr.rect = scr.root.r;
	
	scr.root.parent = &scr.root;

	windowmap.bucket = wbucket;
	windowmap.nhash = nelem(wbucket);
	atommap.bucket = abucket;
	atommap.nhash = nelem(abucket);
	atomnamemap.bucket = anamebucket;
	atomnamemap.nhash = nelem(anamebucket);

	fmtinstall('A', Afmt);
	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	fmtinstall('W', Wfmt);

	xlib_errorhandler = XSetErrorHandler(errorhandler);
}
