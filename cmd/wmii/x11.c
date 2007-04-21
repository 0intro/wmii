/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

Point ZP = {0, 0};
Rectangle ZR = {{0, 0}, {0, 0}};

Map wmap, amap;
MapEnt *wbucket[137];
MapEnt *abucket[137];

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

/* Init */
void
initdisplay() {
	display = XOpenDisplay(nil);
	scr.screen = DefaultScreen(display);
	scr.colormap = DefaultColormap(display, scr.screen);
	scr.visual = DefaultVisual(display, scr.screen);
	scr.gc = DefaultGC(display, scr.screen);
	scr.depth = DefaultDepth(display, scr.screen);
	
	scr.white = WhitePixel(display, scr.screen);
	scr.black = BlackPixel(display, scr.screen);
	
	scr.root.w = RootWindow(display, scr.screen);
	scr.root.r = Rect(0, 0, DisplayWidth(display, scr.screen), DisplayHeight(display, scr.screen));
	scr.rect = scr.root.r;
	
	scr.root.parent = &scr.root;

	wmap.bucket = wbucket;
	wmap.nhash = nelem(wbucket);
	amap.bucket = abucket;
	amap.nhash = nelem(abucket);
}

/* Images */
Image *
allocimage(int w, int h, int depth) {
	Image *img;

	img = emallocz(sizeof *img);
	img->type = WImage;
	img->image = XCreatePixmap(display, scr.root.w, w, h, depth);
	img->gc = XCreateGC(display, img->image, 0, nil);
	img->depth = depth;
	img->r = Rect(0, 0, w, h);
	return img;
}

void
freeimage(Image *img) {
	assert(img->type == WImage);

	XFreePixmap(display, img->image);
	XFreeGC(display, img->gc);
}

/* Windows */
Window *
createwindow(Window *parent, Rectangle r, int depth, uint class,
		WinAttr *wa, int valmask)
		{
	Window *w;

	assert(parent->type == WWindow);

	w = emallocz(sizeof *w);
	w->type = WWindow;
	w->parent = parent;

	w->w =  XCreateWindow(display, parent->w, r.min.x, r.min.y, Dx(r), Dy(r),
				0 /* border */, depth, class, scr.visual, valmask, wa);

	if(class != InputOnly) {
		w->gc = XCreateGC(display, w->w, 0, nil);
		w->image = w->w;
	}

	w->r = r;
	w->depth = depth;
	return w;
}

void
reparentwindow(Window *w, Window *par, Point p) {
	XReparentWindow(display, w->w, par->w, p.x, p.y);
	w->parent = par;
}

void
destroywindow(Window *w) {
	assert(w->type == WWindow);
	sethandler(w, nil);
	if(w->gc)
		XFreeGC(display, w->gc);
	XDestroyWindow(display, w->w);
}

void
setwinattr(Window *w, WinAttr *wa, int valmask) {
	assert(w->type == WWindow);
	XChangeWindowAttributes(display, w->w, valmask, wa);
}

void
reshapewin(Window *w, Rectangle r) {
	assert(w->type == WWindow);
	if(!eqrect(r, w->r))
		XMoveResizeWindow(display, w->w, r.min.x, r.min.y, Dx(r), Dy(r));
	w->r = r;
}

void
movewin(Window *w, Point pt) {
	Rectangle r;

	assert(w->type == WWindow);
	r = rectsubpt(w->r, w->r.min);
	r = rectaddpt(r, pt);
	reshapewin(w, r);
}

int
mapwin(Window *w) {
	if(!w->mapped) {
		XMapWindow(display, w->w);
		w->mapped = 1;
		return 1;
	}
	return 0;
}

int
unmapwin(Window *w) {
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
	XRaiseWindow(display, w->w);
}

void
lowerwin(Window *w) {
	XLowerWindow(display, w->w);
}

Handlers*
sethandler(Window *w, Handlers *new) {
	Handlers *old;
	MapEnt *e;

	assert(w->type == WWindow);
	assert((w->prev != nil && w->next != nil) || w->next == w->prev);

	if(new == nil)
		maprm(&wmap, (ulong)w->w);
	else {
		e = mapget(&wmap, (ulong)w->w, 1);
		e->val = w;
	}
	old = w->handler;
	w->handler = new;
	return old;
}

Window*
findwin(XWindow w) {
	MapEnt *e;
	
	e = mapget(&wmap, (ulong)w, 0);
	if(e)
		return e->val;
	return nil;
}

uint
winprotocols(Window *w) {
	Atom *protocols;
	Atom actual, delete;
	int i, n, protos;

	n = getproperty(w, "_WM_PROTOCOLS", "ATOM", &actual, 0L, (uchar**)&protocols, 20L);
	if(n == 0)
		return 0;

	protos = 0;
	delete = xatom("WM_DELETE_WINDOW");
	for(i = 0; i < n; i++) {
		if(protocols[i] == delete)
			protos |= WM_PROTOCOL_DELWIN;
	}

	free(protocols);
	return protos;
}

/* Shape */
void
setshapemask(Window *dst, Image *src, Point pt) {
	XShapeCombineMask (display, dst->w,
		ShapeBounding, pt.x, pt.y, src->image, ShapeSet);
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
	XSetForeground(display, dst->gc, col);
	XDrawRectangle(display, dst->image, dst->gc,
			r.min.x, r.min.y, Dx(r), Dy(r));
}

void
fill(Image *dst, Rectangle r, ulong col) {
	XSetForeground(display, dst->gc, col);
	XFillRectangle(display, dst->image, dst->gc,
		r.min.x, r.min.y, Dx(r), Dy(r));
}

static XPoint*
convpts(Point *pt, int np) {
	XPoint *rp;
	int i;
	
	rp = emalloc(np * sizeof(*rp));
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
	XSetForeground(display, dst->gc, col);
	XDrawLines(display, dst->image, dst->gc, xp, np, CoordModeOrigin);
	free(xp);
}

void
fillpoly(Image *dst, Point *pt, int np, ulong col) {
	XPoint *xp;

	xp = convpts(pt, np);
	XSetForeground(display, dst->gc, col);
	XFillPolygon(display, dst->image, dst->gc, xp, np, Complex, CoordModeOrigin);
	free(xp);
}

void
drawline(Image *dst, Point p1, Point p2, int cap, int w, ulong col) {
	XSetLineAttributes(display, dst->gc, w, LineSolid, cap, JoinMiter);
	XSetForeground(display, dst->gc, col);
	XDrawLine(display, dst->image, dst->gc, p1.x, p1.y, p2.x, p2.y);
}

void
drawstring(Image *dst, Font *font,
		Rectangle r, Align align,
		char *text, ulong col) {
	char *buf;
	uint x, y, w, h, len;
	Bool shortened;

	shortened = 0;

	len = strlen(text);
	buf = emalloc(len+1);
	memcpy(buf, text, len+1);

	h = font->ascent + font->descent;
	y = r.min.y + Dy(r) / 2 - h / 2 + font->ascent;

	/* shorten text if necessary */
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
	case EAST:
		x = r.max.x - (w + (font->height / 2));
		break;
	default:
		x = r.min.x + (font->height / 2);
		break;
	}

	XSetForeground(display, dst->gc, col);
	if(font->set)
		Xutf8DrawString(display, dst->image, 
				font->set, dst->gc,
				x, y,
				buf, len);
	else {
		XSetFont(display, dst->gc, font->xfont->fid);
		XDrawString(display, dst->image, dst->gc,
				x, y,
				buf, len);
	}

done:
	free(buf);
}

void
copyimage(Image *dst, Rectangle r, Image *src, Point p) {
	XCopyArea(display,
		src->image, dst->image,
		dst->gc,
		r.min.x, r.min.y, Dx(r), Dy(r),
		p.x, p.y);
}

/* Colors */
Bool
namedcolor(char *name, ulong *ret) {
	XColor c, c2;

	if(XAllocNamedColor(display, scr.colormap, name, &c, &c2)) {
		*ret = c.pixel;
		return 1;
	}
	return 0;
}

Bool
loadcolor(CTuple *c, char *str) {
	char buf[24];

	strncpy(buf, str, sizeof buf);
	memcpy(c->colstr, str, sizeof c->colstr);

	buf[7] = buf[15] = buf[23] = '\0';
	return namedcolor(buf, &c->fg)
		&& namedcolor(buf+8, &c->bg)
		&& namedcolor(buf+16, &c->border);
}

/* Fonts */
Font *
loadfont(char *name) {
	Font *f;
	char **missing = nil, *def = "?";
	int n, i;

	f = emallocz(sizeof *f);
	f->name = estrdup(name);

	f->set = XCreateFontSet(display, name, &missing, &n, &def);
	if(missing) {
		fprintf(stderr, "%s: missing fontset%s for '%s':", argv0,
				(n > 1 ? "s":""), name);
		for(i = 0; i < n; i++)
			 fprintf(stderr, "%s %s", (i ? ",":""), missing[i]);
		fprintf(stderr, "\n");
		freestringlist(missing);
	}

	if(f->set) {
		XFontStruct **xfonts;
		char **font_names;

		XFontsOfFontSet(f->set, &xfonts, &font_names);
		f->ascent = xfonts[0]->ascent;
		f->descent = xfonts[0]->descent;
	}
	else {
		f->xfont = XLoadQueryFont(display, name);
		if(!f->xfont) {
			fprintf(stderr, "%s: cannot load font: %s\n", argv0, name);
			freefont(f);
			return nil;
		}

		f->ascent = f->xfont->ascent;
		f->descent = f->xfont->descent;
	}
	f->height = f->ascent + f->descent;
	return f;
}

void
freefont(Font *f) {
	if(f->set)
		XFreeFontSet(display, f->set);
	if(f->xfont)
		XFreeFont(display, f->xfont);
	free(f->name);
	free(f);
}

uint
textwidth_l(Font *font, char *text, uint len) {
	XRectangle r;

	if(font->set) {
		Xutf8TextExtents(font->set, text, len, &r, nil);
		return r.width;
	}
	return XTextWidth(font->xfont, text, len);
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
	MapEnt *e;
	
	e = hashget(&amap, name, 1);
	if(e->val == nil)
		e->val = (void*)XInternAtom(display, name, False);
	return (Atom)e->val;
}

void
changeproperty(Window *w, char *prop, char *type, int width, uchar *data, int n) {
	XChangeProperty(display, w->w, xatom(prop), xatom(type), width, PropModeReplace, data, n);
}

void
freestringlist(char *list[]) {
	XFreeStringList(list);
}

ulong
getproperty(Window *w, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length) {
	Atom typea;
	ulong n, extra;
	int status, format;
	
	typea = (type ? xatom(type) : 0L);

	status = XGetWindowProperty(display, w->w,
		xatom(prop), offset, length, False /* delete */,
		typea, actual, &format, &n, &extra, ret);

	if(status != Success)
		return 0;
	if(n == 0)
		free(*ret);
	return n;
}

int
gettextlistproperty(Window *w, char *name, char **ret[]) {
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

char *
gettextproperty(Window *w, char *name) {
	char **list, *str;
	int n;

	str = nil;

	n = gettextlistproperty(w, name, &list);
	if(n > 0)
		str = estrdup(*list);
	freestringlist(list);

	return str;
}

void
setfocus(Window *w, int mode) {
	XSetInputFocus(display, w->w, mode, CurrentTime);
}

/* Mouse */
Point
querypointer(Window *w) {
	XWindow dummy;
	Point pt;
	uint ui;
	int i;
	
	XQueryPointer(display, w->w, &dummy, &dummy, &i, &i, &pt.x, &pt.y, &ui);
	return pt;
}

void
warppointer(Point pt) {
	XWarpPointer(display,
		/* src, dest w */ None, scr.root.w,
		/* src_rect */	0, 0, 0, 0,
		/* target */	pt.x, pt.y);
}

Point
translate(Window *src, Window *dst, Point sp) {
	Point pt;
	XWindow w;

	XTranslateCoordinates(display, src->w, dst->w, sp.x, sp.y, &pt.x, &pt.y, &w);
	return pt;
}

int
grabpointer(Window *w, Window *confine, Cursor cur, int mask) {
	XWindow cw;
	
	cw = None;
	if(confine)
		cw = confine->w;
	return XGrabPointer(display, w->w, False /* owner events */, mask,
		GrabModeAsync, GrabModeAsync, cw, cur, CurrentTime
		) == GrabSuccess;
}

void
ungrabpointer() {
	XUngrabPointer(display, CurrentTime);
}

/* Insanity */
void
sethints(Window *w) {
	enum { MaxInt = ((uint)(1<<(8*sizeof(int)-1))-1) };
	XSizeHints xs;
	WinHints *h;
	Point p;
	long size;

	if(!XGetWMNormalHints(display, w->w, &xs, &size)) {
		free(w->hints);
		w->hints = nil;
		return;
	}

	if(w->hints == nil)
		w->hints = emalloc(sizeof *h);
	h = w->hints;
	memset(h, 0, sizeof *h);

	h->max = Pt(MaxInt, MaxInt);
	if(xs.flags&PMinSize) {
		p.x = xs.min_width;
		p.y = xs.min_height;
		h->min = p;
	}
	if(xs.flags&PMaxSize) {
		p.x = xs.max_width;
		p.y = xs.max_height;
		h->max = p;
	}

	h->base = h->min;
	if(xs.flags&PBaseSize) {
		p.x = xs.base_width;
		p.y = xs.base_height;
		h->base = p;
		h->baspect = p;
	}

	h->inc = Pt(1,1);
	if(xs.flags&PResizeInc) {
		h->inc.x = xs.width_inc;
		h->inc.y = xs.height_inc;
	}

	if(xs.flags&PAspect) {
		p.x = xs.min_aspect.x;
		p.y = xs.min_aspect.y;
		h->aspect.min = p;
		p.x = xs.max_aspect.x;
		p.y = xs.max_aspect.y;
		h->aspect.max = p;
	}

	p = ZP;
	if((xs.flags&PWinGravity) == 0)
		xs.win_gravity = NorthWestGravity;

	switch (xs.win_gravity) {
	case EastGravity: case CenterGravity: case WestGravity:
		p.y = 1;
		break;
	case SouthEastGravity: case SouthGravity: case SouthWestGravity:
		p.y = 2;
		break;
	}
	switch (xs.win_gravity) {
	case NorthGravity: case CenterGravity: case SouthGravity:
		p.x = 1;
		break;
	case NorthEastGravity: case EastGravity: case SouthEastGravity:
		p.x = 2;
		break;
	}
	h->grav = p;
	h->gravstatic = (xs.win_gravity==StaticGravity);
}

Rectangle
sizehint(WinHints *h, Rectangle r) {
	Point p, p2, o;
	
	o = r.min;
	r = rectsubpt(r, o);

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
	p2 = h->aspect.min;
	if(p.x * p2.y / p.y < p2.x)
		r.max.y = h->baspect.y + p.x * p2.y / p2.x;
	p2 = h->aspect.max;
	if(p.x * p2.y / p.y > p2.x)
		r.max.x = h->baspect.x + p.y * p2.x / p2.y;
	
	return rectaddpt(r, o);
}

Rectangle
gravitate(Rectangle rc, Rectangle rf, Point grav) {
	Point d;

	rf = rectsubpt(rf, rf.min);

	d = subpt(rc.max, rc.min);
	d = subpt(rf.max, d);

	d = divpt(d, Pt(2, 2));
	d = mulpt(d, grav);

	return rectaddpt(rc, d);
}
