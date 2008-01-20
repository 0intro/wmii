/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Image *divimg, *divmask;
static CTuple divc;
static Handlers handlers;

static Divide*
getdiv(Divide **dp) {
	WinAttr wa;
	Divide *d;

	if(*dp)
		return *dp;

	d = emallocz(sizeof *d);

	wa.override_redirect = True;
	wa.cursor = cursor[CurDHArrow];
	wa.event_mask =
		  ExposureMask
		| EnterWindowMask
		| ButtonPressMask
		| ButtonReleaseMask;
	d->w = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth, InputOutput, &wa,
		  CWOverrideRedirect
		| CWEventMask
		| CWCursor);
	d->w->aux = d;
	sethandler(d->w, &handlers);

	*dp = d;
	return d;
}

static void
mapdiv(Divide *d) {
	mapwin(d->w);
}

static void
unmapdiv(Divide *d) {
	unmapwin(d->w);
}

void
div_set(Divide *d, int x) {
	Rectangle r;

	d->x = x;
	r = rectaddpt(divimg->r, Pt(x - Dx(divimg->r)/2, 0));
	r.max.y = screen->brect.min.y;

	reshapewin(d->w, r);
	mapdiv(d);
}

static void
drawimg(Image *img, ulong cbg, ulong cborder) {
	Point pt[6];

	pt[0] = Pt(0, 0);
	pt[1] = Pt(Dx(img->r)/2 - 1, Dx(img->r)/2 - 1);

	pt[2] = Pt(pt[1].x, Dy(img->r));
	pt[3] = Pt(Dx(img->r)/2, pt[2].y);

	pt[4] = Pt(pt[3].x, Dx(img->r)/2 - 1);
	pt[5] = Pt(Dx(img->r) - 1, 0);

	fillpoly(img, pt, nelem(pt), cbg);
	drawpoly(img, pt, nelem(pt), CapNotLast, 1, cborder);
}

static void
drawdiv(Divide *d) {
	copyimage(d->w, divimg->r, divimg, ZP);
	setshapemask(d->w, divmask, ZP);
}

static void
update_imgs(void) {
	Divide *d;
	int w, h;

	w = 2 * (labelh(def.font) / 3);
	w = max(w, 10);
	h = Dy(screen->sel->r);

	if(divimg) {
		if(w == Dx(divimg->r) && h == Dy(divimg->r)
		&& !memcmp(&divc, &def.normcolor, sizeof(divc)))
			return;
		freeimage(divimg);
		freeimage(divmask);
	}

	divimg = allocimage(w, h, scr.depth);
	divmask = allocimage(w, h, 1);
	divc = def.normcolor;

	fill(divmask, divmask->r, 0);
	drawimg(divmask, 1, 1);
	drawimg(divimg, divc.bg, divc.border);

	for(d = divs; d && d->w->mapped; d = d->next)
		drawdiv(d);
}

void
div_update_all(void) {
	Divide **dp, *d;
	Area *a;
	View *v;

	update_imgs();

	v = screen->sel;
	dp = &divs;
	for(a = v->area->next; a; a = a->next) {
		d = getdiv(dp);
		dp = &d->next;
		div_set(d, a->r.min.x);

		if(!a->next) {
			d = getdiv(dp);
			dp = &d->next;
			div_set(d, a->r.max.x);
		}
	}
	for(d = *dp; d; d = d->next)
		unmapdiv(d);
}

/* Div Handlers */
static void
bdown_event(Window *w, XButtonEvent *e) {
	Divide *d;

	USED(e);
	
	d = w->aux;
	mouse_resizecol(d);
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Divide *d;
	
	USED(e);
	
	d = w->aux;
	drawdiv(d);
}

static Handlers handlers = {
	.bdown = bdown_event,
	.expose = expose_event,
};

