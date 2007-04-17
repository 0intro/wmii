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

Window wlist;

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
divpt(Point p, Point q) {
	p.x *= q.x;
	p.y *= q.y;
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

	wlist.next = wlist.prev = &wlist;
}

/* Images */
Image *
allocimage(int w, int h, int depth) {
	Image *img;

	img = emallocz(sizeof *img);
	img->type = WImage;
	img->image = XCreatePixmap(display, scr.root.w,
			w, h, depth);
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
destroywindow(Window *w) {
	assert(w->type == WWindow);
	if(w->gc)
		XFreeGC(display, w->gc);
	XDestroyWindow(display, w->w);
	sethandler(w, nil);
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
	r = rectaddpt(w->r, pt);
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
	Window *wp;

	assert(w->type == WWindow);

	old = w->handler;
	if(new == nil && w->prev) {
		w->prev->next = w->next;
		w->next->prev = w->prev;
		w->next = w->prev = nil;
	}else {
		for(wp = wlist.next; wp != &wlist; wp = wp->next)
			if(w->w <= wp->w) break;
		if(wp->w != w->w) {
			w->next = wp;
			w->prev = wp->prev;
			wp->prev = w;
			w->prev->next = w;
		}
	}
	w->handler = new;
	return old;
}

Window*
findwin(XWindow w) {
	Window *wp;

	for(wp = wlist.next; wp != &wlist; wp=wp->next)
		if(wp->w >= w) break;
	if(wp->w == w)
		return wp;
	return nil;
}

uint
winprotocols(Window *w) {
	Atom *protocols;
	Atom real;
	ulong nitems, extra;
	int i, format, status, protos;

	status = XGetWindowProperty(
		display, w->w, atom[WMProtocols],
		/* offset, length, delete, req_type */
		0L, 20L, False, XA_ATOM,
		/* type, format, nitems, extra bytes returns */
		&real, &format, &nitems, &extra, 
		/* property return */
		(uchar**)&protocols);

	if(status != Success || protocols == nil)
		return 0;

	if(nitems == 0) {
		XFree(protocols);
		return 0;
	}

	protos = 0;
	for(i = 0; i < nitems; i++) {
		if(protocols[i] == atom[WMDelete])
			protos |= WM_PROTOCOL_DELWIN;
	}

	XFree(protocols);
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

		buf[--len] = '.';
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
		XmbDrawString(display, dst->image, 
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
		XFreeStringList(missing);
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
		XmbTextExtents(font->set, text, len, &r, nil);
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
	return XInternAtom(display, name, False);
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
