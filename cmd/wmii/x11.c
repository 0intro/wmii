/* Copyright ©2007-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define _X11_VISIBLE
#define pointerwin __pointerwin
#include "dat.h"
#include <limits.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>
#include <bio.h>
#include "fns.h"
#undef  pointerwin

const Point	ZP = {0, 0};
const Rectangle	ZR = {{0, 0}, {0, 0}};

const Window	_pointerwin = { .w = PointerRoot };
Window*		const pointerwin = (Window*)&_pointerwin;

static Map	windowmap;
static Map	atommap;
static MapEnt*	wbucket[137];
static MapEnt*	abucket[137];

static int	errorhandler(Display*, XErrorEvent*);
static int	(*xlib_errorhandler) (Display*, XErrorEvent*);

static XftColor*	xftcolor(ulong);


/* Rectangles/Points */
XRectangle
XRect(Rectangle r) {
	XRectangle xr;

	xr.x = r.min.x;
	xr.y = r.min.y;
	xr.width = Dx(r);
	xr.height = Dy(r);
	return xr;
}

int
eqrect(Rectangle a, Rectangle b) {
	return a.min.x==b.min.x && a.max.x==b.max.x
	    && a.min.y==b.min.y && a.max.y==b.max.y;
}

int
eqpt(Point p, Point q) {
	return p.x==q.x && p.y==q.y;
}

Point
addpt(Point p, Point q) {
	p.x += q.x;
	p.y += q.y;
	return p;
}

Point
subpt(Point p, Point q) {
	p.x -= q.x;
	p.y -= q.y;
	return p;
}

Point
mulpt(Point p, Point q) {
	p.x *= q.x;
	p.y *= q.y;
	return p;
}

Point
divpt(Point p, Point q) {
	p.x /= q.x;
	p.y /= q.y;
	return p;
}

Rectangle
insetrect(Rectangle r, int n) {
	r.min.x += n;
	r.min.y += n;
	r.max.x -= n;
	r.max.y -= n;
	return r;
}

Rectangle
rectaddpt(Rectangle r, Point p) {
	r.min.x += p.x;
	r.max.x += p.x;
	r.min.y += p.y;
	r.max.y += p.y;
	return r;
}

Rectangle
rectsubpt(Rectangle r, Point p) {
	r.min.x -= p.x;
	r.max.x -= p.x;
	r.min.y -= p.y;
	r.max.y -= p.y;
	return r;
}

Rectangle
rectsetorigin(Rectangle r, Point p) {
	Rectangle ret;

	ret.min.x = p.x;
	ret.min.y = p.y;
	ret.max.x = p.x + Dx(r);
	ret.max.y = p.y + Dy(r);
	return ret;
}

/* Formatters */
static int
Afmt(Fmt *f) {
	Atom a;
	char *s;
	int i;

	a = va_arg(f->args, Atom);
	s = XGetAtomName(display, a);
	i = fmtprint(f, "%s", s);
	free(s);
	return i;
}

static int
Rfmt(Fmt *f) {
	Rectangle r;

	r = va_arg(f->args, Rectangle);
	return fmtprint(f, "%P+%dx%d", r.min, Dx(r), Dy(r));
}

static int
Pfmt(Fmt *f) {
	Point p;

	p = va_arg(f->args, Point);
	return fmtprint(f, "(%d,%d)", p.x, p.y);
}

static int
Wfmt(Fmt *f) {
	Window *w;

	w = va_arg(f->args, Window*);
	return fmtprint(f, "0x%ulx", w->w);
}

/* Init */
void
initdisplay(void) {
	display = XOpenDisplay(nil);
	if(display == nil)
		fatal("Can't open display");
	scr.screen = DefaultScreen(display);
	scr.colormap = DefaultColormap(display, scr.screen);
	scr.visual = DefaultVisual(display, scr.screen);
	scr.gc = DefaultGC(display, scr.screen);
	scr.depth = DefaultDepth(display, scr.screen);
	
	scr.white = WhitePixel(display, scr.screen);
	scr.black = BlackPixel(display, scr.screen);
	
	scr.root.w = RootWindow(display, scr.screen);
	scr.root.r = Rect(0, 0,
			  DisplayWidth(display, scr.screen),
			  DisplayHeight(display, scr.screen));
	scr.rect = scr.root.r;
	
	scr.root.parent = &scr.root;

	windowmap.bucket = wbucket;
	windowmap.nhash = nelem(wbucket);
	atommap.bucket = abucket;
	atommap.nhash = nelem(abucket);

	fmtinstall('A', Afmt);
	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	fmtinstall('W', Wfmt);

	xlib_errorhandler = XSetErrorHandler(errorhandler);
}

/* Error handling */

ErrorCode ignored_xerrors[];
static bool	_trap_errors;
static long	nerrors;

static int
errorhandler(Display *dpy, XErrorEvent *error) {
	ErrorCode *e;

	USED(dpy);

	if(_trap_errors)
		nerrors++;

	e = ignored_xerrors;
	if(e)
	for(; e->rcode || e->ecode; e++)
		if((e->rcode == 0 || e->rcode == error->request_code)
		&& (e->ecode == 0 || e->ecode == error->error_code))
			return 0;

	fprint(2, "%s: fatal error: Xrequest code=%d, Xerror code=%d\n",
			argv0, error->request_code, error->error_code);
	return xlib_errorhandler(display, error); /* calls exit() */
}

int
traperrors(bool enable) {
	
	sync();
	_trap_errors = enable;
	if (enable)
		nerrors = 0;
	return nerrors;
	
}

/* Images */
Image*
allocimage(int w, int h, int depth) {
	Image *img;

	img = emallocz(sizeof *img);
	img->type = WImage;
	img->w = XCreatePixmap(display, scr.root.w, w, h, depth);
	img->gc = XCreateGC(display, img->w, 0, nil);
	img->depth = depth;
	img->r = Rect(0, 0, w, h);
	return img;
}

void
freeimage(Image *img) {
	if(img == nil)
		return;

	assert(img->type == WImage);

	if(img->xft)
		XftDrawDestroy(img->xft);
	XFreePixmap(display, img->w);
	XFreeGC(display, img->gc);
	free(img);
}

static XftDraw*
xftdrawable(Image *img) {
	if(img->xft == nil)
		img->xft = XftDrawCreate(display, img->w, scr.visual, scr.colormap);
	return img->xft;
}

/* Windows */
Window*
createwindow_visual(Window *parent, Rectangle r,
		    int depth, Visual *vis, uint class,
		    WinAttr *wa, int valmask) {
	Window *w;

	assert(parent->type == WWindow);

	w = emallocz(sizeof *w);
	w->type = WWindow;
	w->parent = parent;

	w->w = XCreateWindow(display, parent->w, r.min.x, r.min.y, Dx(r), Dy(r),
				0 /* border */, depth, class, vis, valmask, wa);
#if 0
	print("createwindow_visual(%W, %R, %d, %p, %ud, %p, %x) = %W\n",
			parent, r, depth, vis, class, wa, valmask, w);
#endif
	if(class != InputOnly)
		w->gc = XCreateGC(display, w->w, 0, nil);

	w->r = r;
	w->depth = depth;
	return w;
}

Window*
createwindow(Window *parent, Rectangle r, int depth, uint class, WinAttr *wa, int valmask) {
	return createwindow_visual(parent, r, depth, scr.visual, class, wa, valmask);
}

Window*
window(XWindow xw) {
	Window *w;

	w = malloc(sizeof *w);
	w->type = WWindow;
	w->w = xw;
	return freelater(w);
}

void
reparentwindow(Window *w, Window *par, Point p) {
	assert(w->type == WWindow);
	XReparentWindow(display, w->w, par->w, p.x, p.y);
	w->parent = par;
	w->r = rectsubpt(w->r, w->r.min);
	w->r = rectaddpt(w->r, p);
}

void
destroywindow(Window *w) {
	assert(w->type == WWindow);
	sethandler(w, nil);
	if(w->xft)
		XftDrawDestroy(w->xft);
	if(w->gc)
		XFreeGC(display, w->gc);
	XDestroyWindow(display, w->w);
	free(w);
}

void
setwinattr(Window *w, WinAttr *wa, int valmask) {
	assert(w->type == WWindow);
	XChangeWindowAttributes(display, w->w, valmask, wa);
}

void
selectinput(Window *w, long mask) {
	XSelectInput(display, w->w, mask);
}

static void
configwin(Window *w, Rectangle r, int border) {
	XWindowChanges wc;

	if(eqrect(r, w->r) && border == w->border)
		return;

	wc.x = r.min.x - border;
	wc.y = r.min.y - border;
	wc.width = Dx(r);
	wc.height = Dy(r);
	wc.border_width = border;
	XConfigureWindow(display, w->w, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);

	w->r = r;
	w->border = border;
}

void
setborder(Window *w, int width, long pixel) {

	assert(w->type == WWindow);
	if(width)
		XSetWindowBorder(display, w->w, pixel);
	if(width != w->border)
		configwin(w, w->r, width);
}

void
reshapewin(Window *w, Rectangle r) {
	assert(w->type == WWindow);
	assert(Dx(r) > 0 && Dy(r) > 0); /* Rather than an X error. */

	configwin(w, r, w->border);
}

void
movewin(Window *w, Point pt) {
	Rectangle r;

	assert(w->type == WWindow);
	r = rectsetorigin(w->r, pt);
	reshapewin(w, r);
}

int
mapwin(Window *w) {
	assert(w->type == WWindow);
	if(!w->mapped) {
		XMapWindow(display, w->w);
		w->mapped = 1;
		return 1;
	}
	return 0;
}

int
unmapwin(Window *w) {
	assert(w->type == WWindow);
	if(w->mapped) {
		XUnmapWindow(display, w->w);
		w->mapped = 0;
		w->unmapped++;
		return 1;
	}
	return 0;
}

void
raisewin(Window *w) {
	assert(w->type == WWindow);
	XRaiseWindow(display, w->w);
}

void
lowerwin(Window *w) {
	assert(w->type == WWindow);
	XLowerWindow(display, w->w);
}

Handlers*
sethandler(Window *w, Handlers *new) {
	Handlers *old;
	void **e;

	assert(w->type == WWindow);
	assert((w->prev != nil && w->next != nil) || w->next == w->prev);

	if(new == nil)
		map_rm(&windowmap, (ulong)w->w);
	else {
		e = map_get(&windowmap, (ulong)w->w, true);
		*e = w;
	}
	old = w->handler;
	w->handler = new;
	return old;
}

Window*
findwin(XWindow w) {
	void **e;
	
	e = map_get(&windowmap, (ulong)w, false);
	if(e)
		return *e;
	return nil;
}

/* Shape */
void
setshapemask(Window *dst, Image *src, Point pt) {
	/* Assumes that we have the shape extension... */
	XShapeCombineMask (display, dst->w,
		ShapeBounding, pt.x, pt.y, src->w, ShapeSet);
}

static void
setgccol(Image *dst, ulong col) {
	XSetForeground(display, dst->gc, col);
}

/* Drawing */
void
border(Image *dst, Rectangle r, int w, ulong col) {
	if(w == 0)
		return;

	r = insetrect(r, w/2);
	r.max.x -= w%2;
	r.max.y -= w%2;

	XSetLineAttributes(display, dst->gc, w, LineSolid, CapButt, JoinMiter);
	setgccol(dst, col);
	XDrawRectangle(display, dst->w, dst->gc,
			r.min.x, r.min.y, Dx(r), Dy(r));
}

void
fill(Image *dst, Rectangle r, ulong col) {
	setgccol(dst, col);
	XFillRectangle(display, dst->w, dst->gc,
		r.min.x, r.min.y, Dx(r), Dy(r));
}

static XPoint*
convpts(Point *pt, int np) {
	XPoint *rp;
	int i;
	
	rp = emalloc(np * sizeof *rp);
	for(i = 0; i < np; i++) {
		rp[i].x = pt[i].x;
		rp[i].y = pt[i].y;
	}
	return rp;
}

void
drawpoly(Image *dst, Point *pt, int np, int cap, int w, ulong col) {
	XPoint *xp;
	
	xp = convpts(pt, np);
	XSetLineAttributes(display, dst->gc, w, LineSolid, cap, JoinMiter);
	setgccol(dst, col);
	XDrawLines(display, dst->w, dst->gc, xp, np, CoordModeOrigin);
	free(xp);
}

void
fillpoly(Image *dst, Point *pt, int np, ulong col) {
	XPoint *xp;

	xp = convpts(pt, np);
	setgccol(dst, col);
	XFillPolygon(display, dst->w, dst->gc, xp, np, Complex, CoordModeOrigin);
	free(xp);
}

void
drawline(Image *dst, Point p1, Point p2, int cap, int w, ulong col) {
	XSetLineAttributes(display, dst->gc, w, LineSolid, cap, JoinMiter);
	setgccol(dst, col);
	XDrawLine(display, dst->w, dst->gc, p1.x, p1.y, p2.x, p2.y);
}

uint
drawstring(Image *dst, Font *font,
	   Rectangle r, Align align,
	   char *text, ulong col) {
	char *buf;
	uint x, y, w, h, len;
	int shortened;

	shortened = 0;

	len = strlen(text);
	buf = emalloc(len+1);
	memcpy(buf, text, len+1);

	h = font->ascent + font->descent;
	y = r.min.y + Dy(r) / 2 - h / 2 + font->ascent;

	/* shorten text if necessary */
	w = 0;
	while(len > 0) {
		w = textwidth_l(font, buf, len + min(shortened, 3));
		if(w <= Dx(r) - (font->height & ~1))
			break;
		while(len > 0 && (buf[--len]&0xC0) == 0x80)
			buf[len] = '.';
		buf[len] = '.';
		shortened++;
	}

	if(len == 0 || w > Dx(r))
		goto done;

	/* mark shortened info in the string */
	if(shortened)
		len += min(shortened, 3);

	switch (align) {
	case East:
		x = r.max.x - (w + (font->height / 2));
		break;
	case Center:
		x = r.min.x + (Dx(r) - w) / 2;
		break;
	default:
		x = r.min.x + (font->height / 2);
		break;
	}

	setgccol(dst, col);
	switch(font->type) {
	case FFontSet:
		Xutf8DrawString(display, dst->w,
				font->font.set, dst->gc,
				x, y,
				buf, len);
		break;
	case FXft:
		XftDrawStringUtf8(xftdrawable(dst), xftcolor(col),
				  font->font.xft,
				  x, y, (uchar*)buf, len);
		break;
	case FX11:
		XSetFont(display, dst->gc, font->font.x11->fid);
		XDrawString(display, dst->w, dst->gc,
			    x, y, buf, len);
		break;
	default:
		die("Invalid font type.");
	}

done:
	free(buf);
	return w;
}

void
copyimage(Image *dst, Rectangle r, Image *src, Point p) {
	XCopyArea(display,
		  src->w, dst->w,
		  dst->gc,
		  r.min.x, r.min.y, Dx(r), Dy(r),
		  p.x, p.y);
}

/* Colors */
bool
namedcolor(char *name, ulong *ret) {
	XColor c, c2;

	if(XAllocNamedColor(display, scr.colormap, name, &c, &c2)) {
		/* FIXME: Kludge. */
		/* This isn't garunteed to work. In the case of RGBA
		 * visuals, we need the Alpha set to 1.0. This
		 * could, in theory, break plain RGB, or even RGBA.
		 */
		*ret = c.pixel | 0xff000000;
		return true;
	}
	return false;
}

bool
loadcolor(CTuple *c, char *str) {
	char buf[24];

	utflcpy(buf, str, sizeof buf);
	memcpy(c->colstr, str, sizeof c->colstr);

	buf[7] = buf[15] = buf[23] = '\0';
	return namedcolor(buf, &c->fg)
	    && namedcolor(buf+8, &c->bg)
	    && namedcolor(buf+16, &c->border);
}

static XftColor*
xftcolor(ulong col) {
	XftColor *c;

	c = emallocz(sizeof *c);
	*c = (XftColor) {
		col, {
			(col>>8)  & 0xff00,
			(col>>0)  & 0xff00,
			(col<<8)  & 0xff00,
			(col>>16) & 0xff00,
		}
	};
	return freelater(c);
}

/* Fonts */
Font*
loadfont(char *name) {
	XFontStruct **xfonts;
	char **missing, **font_names;
	Biobuf *b;
	Font *f;
	int n, i;

	missing = nil;
	f = emallocz(sizeof *f);
	f->name = estrdup(name);
	if(!strncmp(f->name, "xft:", 4)) {
		f->type = FXft;

		f->font.xft = XftFontOpenXlfd(display, scr.screen, f->name + 4);
		if(!f->font.xft)
			f->font.xft = XftFontOpenName(display, scr.screen, f->name + 4);
		if(!f->font.xft)
			goto error;

		f->ascent = f->font.xft->ascent;
		f->descent = f->font.xft->descent;
	}else {
		f->font.set = XCreateFontSet(display, name, &missing, &n, nil);
		if(missing) {
			b = Bfdopen(dup(2), O_WRONLY);
			Bprint(b, "%s: note: missing fontset%s for '%s':", argv0,
					(n > 1 ? "s" : ""), name);
			for(i = 0; i < n; i++)
				Bprint(b, "%s %s", (i ? "," : ""), missing[i]);
			Bprint(b, "\n");
			Bterm(b);
			freestringlist(missing);
		}

		if(f->font.set) {
			f->type = FFontSet;
			XFontsOfFontSet(f->font.set, &xfonts, &font_names);
			f->ascent = xfonts[0]->ascent;
			f->descent = xfonts[0]->descent;
		}else {
			f->type = FX11;
			f->font.x11 = XLoadQueryFont(display, name);
			if(!f->font.x11)
				goto error;

			f->ascent = f->font.x11->ascent;
			f->descent = f->font.x11->descent;
		}
	}
	f->height = f->ascent + f->descent;
	return f;

error:
	fprint(2, "%s: cannot load font: %s\n", argv0, name);
	f->type = 0;
	freefont(f);
	return nil;
}

void
freefont(Font *f) {
	switch(f->type) {
	case FFontSet:
		XFreeFontSet(display, f->font.set);
		break;
	case FXft:
		XftFontClose(display, f->font.xft);
		break;
	case FX11:
		XFreeFont(display, f->font.x11);
		break;
	default:
		break;
	}
	free(f->name);
	free(f);
}

uint
textwidth_l(Font *font, char *text, uint len) {
	XRectangle r;
	XGlyphInfo i;

	switch(font->type) {
	case FFontSet:
		Xutf8TextExtents(font->font.set, text, len, nil, &r);
		return r.width;
	case FXft:
		XftTextExtentsUtf8(display, font->font.xft, (uchar*)text, len, &i);
		return i.width;
	case FX11:
		return XTextWidth(font->font.x11, text, len);
	default:
		die("Invalid font type");
		return 0; /* shut up ken */
	}
}

uint
textwidth(Font *font, char *text) {
	return textwidth_l(font, text, strlen(text));
}

uint
labelh(Font *font) {
	return font->height + 2;
}

/* Misc */
Atom
xatom(char *name) {
	void **e;
	
	e = hash_get(&atommap, name, true);
	if(*e == nil)
		*e = (void*)XInternAtom(display, name, false);
	return (Atom)*e;
}

void
sendmessage(Window *w, char *name, long l0, long l1, long l2, long l3, long l4) {
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w->w;
	e.message_type = xatom(name);
	e.format = 32;
	e.data.l[0] = l0;
	e.data.l[1] = l1;
	e.data.l[2] = l2;
	e.data.l[3] = l3;
	e.data.l[4] = l4;
	sendevent(w, false, NoEventMask, (XEvent*)&e);
}

void
sendevent(Window *w, bool propegate, long mask, XEvent *e) {
	XSendEvent(display, w->w, propegate, mask, e);
}

KeyCode
keycode(char *name) {
	return XKeysymToKeycode(display, XStringToKeysym(name));
}

typedef struct KMask KMask;

static struct KMask {
	int		mask;
	const char*	name;
} masks[] = {
	{ShiftMask,   "Shift"},
	{ControlMask, "Control"},
	{Mod1Mask,    "Mod1"},
	{Mod2Mask,    "Mod2"},
	{Mod3Mask,    "Mod3"},
	{Mod4Mask,    "Mod4"},
	{Mod5Mask,    "Mod5"},
	{0,}
};

bool
parsekey(char *str, int *mask, char **key) {
	static char *keys[16];
	KMask *m;
	int i, nkeys;

	*mask = 0;
	nkeys = tokenize(keys, nelem(keys), str, '-');
	for(i=0; i < nkeys; i++) {
		for(m=masks; m->mask; m++)
			if(!strcasecmp(m->name, keys[i])) {
				*mask |= m->mask;
				goto next;
			}
		break;
	next: continue;
	}
	if(key) {
		if(nkeys)
			*key = keys[i];
		return i == nkeys - 1;
	}
	else
		return i == nkeys;
}

void
sync(void) {
	XSync(display, false);
}

/* Properties */
void
delproperty(Window *w, char *prop) {
	XDeleteProperty(display, w->w, xatom(prop));
}

void
changeproperty(Window *w, char *prop, char *type,
	       int width, uchar data[], int n) {
	XChangeProperty(display, w->w, xatom(prop), xatom(type), width,
			PropModeReplace, data, n);
}

void
changeprop_string(Window *w, char *prop, char *string) {
	changeprop_char(w, prop, "UTF8_STRING", string, strlen(string));
}

void
changeprop_char(Window *w, char *prop, char *type, char data[], int len) {
	changeproperty(w, prop, type, 8, (uchar*)data, len);
}

void
changeprop_short(Window *w, char *prop, char *type, short data[], int len) {
	changeproperty(w, prop, type, 16, (uchar*)data, len);
}

void
changeprop_long(Window *w, char *prop, char *type, long data[], int len) {
	changeproperty(w, prop, type, 32, (uchar*)data, len);
}

void
changeprop_ulong(Window *w, char *prop, char *type, ulong data[], int len) {
	changeproperty(w, prop, type, 32, (uchar*)data, len);
}

void
changeprop_textlist(Window *w, char *prop, char *type, char *data[]) {
	char **p, *s, *t;
	int len, n;

	len = 0;
	for(p=data; *p; p++)
		len += strlen(*p) + 1;
	s = emalloc(len);
	t = s;
	for(p=data; *p; p++) {
		n = strlen(*p) + 1;
		memcpy(t, *p, n);
		t += n;
	}
	changeprop_char(w, prop, type, s, len);
	free(s);
}

void
freestringlist(char *list[]) {
	XFreeStringList(list);
}

static ulong
getprop(Window *w, char *prop, char *type, Atom *actual, int *format,
	ulong offset, uchar **ret, ulong length) {
	Atom typea;
	ulong n, extra;
	int status;

	typea = (type ? xatom(type) : 0L);

	status = XGetWindowProperty(display, w->w,
		xatom(prop), offset, length, false /* delete */,
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

ulong
getproperty(Window *w, char *prop, char *type, Atom *actual,
	    ulong offset, uchar **ret, ulong length) {
	int format;

	return getprop(w, prop, type, actual, &format, offset, ret, length);
}

ulong
getprop_long(Window *w, char *prop, char *type,
	     ulong offset, long **ret, ulong length) {
	Atom actual;
	ulong n;
	int format;

	n = getprop(w, prop, type, &actual, &format, offset, (uchar**)ret, length);
	if(n == 0 || format == 32 && xatom(type) == actual)
		return n;
	free(*ret);
	*ret = 0;
	return 0;
}

ulong
getprop_ulong(Window *w, char *prop, char *type,
	      ulong offset, ulong **ret, ulong length) {
	return getprop_long(w, prop, type, offset, (long**)ret, length);
}

char**
strlistdup(char *list[]) {
	char **p;
	char *q;
	int i, m, n;

	n = 0;
	m = 0;
	for(p=list; *p; p++, n++)
		m += strlen(*p) + 1;

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

int
getprop_textlist(Window *w, char *name, char **ret[]) {
	XTextProperty prop;
	char **list;
	int n;

	*ret = nil;
	n = 0;

	XGetTextProperty(display, w->w, &prop, xatom(name));
	if(prop.nitems > 0) {
		if(Xutf8TextPropertyToTextList(display, &prop, &list, &n) == Success)
			*ret = list;
		XFree(prop.value);
	}
	return n;
}

char*
getprop_string(Window *w, char *name) {
	char **list, *str;
	int n;

	str = nil;

	n = getprop_textlist(w, name, &list);
	if(n > 0)
		str = estrdup(*list);
	freestringlist(list);

	return str;
}

Rectangle
getwinrect(Window *w) {
	XWindowAttributes wa;
	Point p;

	if(!XGetWindowAttributes(display, w->w, &wa))
		return ZR;
	p = translate(w, &scr.root, ZP);
	return rectaddpt(Rect(0, 0, wa.width, wa.height), p);
}

void
setfocus(Window *w, int mode) {
	XSetInputFocus(display, w->w, mode, CurrentTime);
}

XWindow
getfocus(void) {
	XWindow ret, revert;

	XGetInputFocus(display, &ret, &revert);
	return ret;
}

/* Mouse */
Point
querypointer(Window *w) {
	XWindow win;
	Point pt;
	uint ui;
	int i;
	
	XQueryPointer(display, w->w, &win, &win, &i, &i, &pt.x, &pt.y, &ui);
	return pt;
}

int
pointerscreen(void) {
	XWindow win;
	Point pt;
	uint ui;
	int i;
	
	return XQueryPointer(display, scr.root.w, &win, &win, &i, &i,
			     &pt.x, &pt.y, &ui);
}

void
warppointer(Point pt) {
	/* Nasty kludge for xephyr, xnest. */
	static int havereal = -1;
	static char* real;

	if(havereal == -1) {
		real = getenv("REALDISPLAY");
		havereal = real != nil;
	}
	if(havereal)
		system(sxprint("DISPLAY=%s wiwarp %d %d", real, pt.x, pt.y));

	XWarpPointer(display,
		/* src, dest w */ None, scr.root.w,
		/* src_rect */	0, 0, 0, 0,
		/* target */	pt.x, pt.y);
}

Point
translate(Window *src, Window *dst, Point sp) {
	Point pt;
	XWindow w;

	XTranslateCoordinates(display, src->w, dst->w, sp.x, sp.y,
			      &pt.x, &pt.y, &w);
	return pt;
}

int
grabpointer(Window *w, Window *confine, Cursor cur, int mask) {
	XWindow cw;
	
	cw = None;
	if(confine)
		cw = confine->w;
	return XGrabPointer(display, w->w, false /* owner events */, mask,
		GrabModeAsync, GrabModeAsync, cw, cur, CurrentTime
		) == GrabSuccess;
}

void
ungrabpointer(void) {
	XUngrabPointer(display, CurrentTime);
}

int
grabkeyboard(Window *w) {

	return XGrabKeyboard(display, w->w, true /* owner events */,
		GrabModeAsync, GrabModeAsync, CurrentTime
		) == GrabSuccess;
}

void
ungrabkeyboard(void) {
	XUngrabKeyboard(display, CurrentTime);
}

/* Insanity */
void
sethints(Window *w) {
	XSizeHints xs;
	XWMHints *wmh;
	WinHints *h;
	Point p;
	long size;

	if(w->hints == nil)
		w->hints = emalloc(sizeof *h);

	h = w->hints;
	memset(h, 0, sizeof *h);

	h->max = Pt(INT_MAX, INT_MAX);
	h->inc = Pt(1,1);

	wmh = XGetWMHints(display, w->w);
	if(wmh) {
		if(wmh->flags & WindowGroupHint)
			h->group = wmh->window_group;
		free(wmh);
	}

	if(!XGetWMNormalHints(display, w->w, &xs, &size))
		return;

	if(xs.flags & PMinSize) {
		h->min.x = xs.min_width;
		h->min.y = xs.min_height;
	}
	if(xs.flags & PMaxSize) {
		h->max.x = xs.max_width;
		h->max.y = xs.max_height;
	}

	/* Goddamn buggy clients. */
	if(h->max.x < h->min.x)
		h->max.x = h->min.x;
	if(h->max.y < h->min.y)
		h->max.y = h->min.y;

	h->base = h->min;
	if(xs.flags & PBaseSize) {
		p.x = xs.base_width;
		p.y = xs.base_height;
		h->base = p;
		h->baspect = p;
	}

	if(xs.flags & PResizeInc) {
		h->inc.x = max(xs.width_inc, 1);
		h->inc.y = max(xs.height_inc, 1);
	}

	if(xs.flags & PAspect) {
		h->aspect.min.x = xs.min_aspect.x;
		h->aspect.min.y = xs.min_aspect.y;
		h->aspect.max.x = xs.max_aspect.x;
		h->aspect.max.y = xs.max_aspect.y;
	}

	h->position = (xs.flags & (USPosition|PPosition)) != 0;

	if(!(xs.flags & PWinGravity))
		xs.win_gravity = NorthWestGravity;
	p = ZP;
	switch (xs.win_gravity) {
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		p.y = 1;
		break;
	case SouthEastGravity:
	case SouthGravity:
	case SouthWestGravity:
		p.y = 2;
		break;
	}
	switch (xs.win_gravity) {
	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
		p.x = 1;
		break;
	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
		p.x = 2;
		break;
	}
	h->grav = p;
	h->gravstatic = (xs.win_gravity == StaticGravity);
}

Rectangle
sizehint(WinHints *h, Rectangle r) {
	Point p, aspect, origin;

	if(h == nil)
		return r;

	origin = r.min;
	r = rectsubpt(r, origin);

	/* Min/max */
	r.max.x = max(r.max.x, h->min.x);
	r.max.y = max(r.max.y, h->min.y);
	r.max.x = min(r.max.x, h->max.x);
	r.max.y = min(r.max.y, h->max.y);

	/* Increment */
	p = subpt(r.max, h->base);
	r.max.x -= p.x % h->inc.x;
	r.max.y -= p.y % h->inc.y;

	/* Aspect */
	p = subpt(r.max, h->baspect);
	p.y = max(p.y, 1);

	aspect = h->aspect.min;
	if(p.x * aspect.y / p.y < aspect.x)
		r.max.y = h->baspect.y
			+ p.x * aspect.y / aspect.x;

	aspect = h->aspect.max;
	if(p.x * aspect.y / p.y > aspect.x)
		r.max.x = h->baspect.x
		        + p.y * aspect.x / aspect.y;

	return rectaddpt(r, origin);
}

Rectangle
gravitate(Rectangle rc, Rectangle rf, Point grav) {
	Point d;

	/* Get delta between frame and client rectangles */
	d = subpt(subpt(rf.max, rf.min),
		  subpt(rc.max, rc.min));

	/* Divide by 2 and apply gravity */
	d = divpt(d, Pt(2, 2));
	d = mulpt(d, grav);

	return rectsubpt(rc, d);
}

