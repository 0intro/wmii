/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Image*	divimg;
static Image*	divmask;
static CTuple	divcolor;
static Handlers	handlers;

static Divide*
getdiv(Divide ***dp) {
	WinAttr wa;
	Divide *d;

	if(**dp)
		d = **dp;
	else {
		d = emallocz(sizeof *d);

		wa.override_redirect = true;
		wa.cursor = cursor[CurDHArrow];
		wa.event_mask =
			  ExposureMask
			| EnterWindowMask
			| ButtonPressMask
			| ButtonReleaseMask;
		d->w = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth,
				    InputOutput, &wa,
			  CWOverrideRedirect
			| CWEventMask
			| CWCursor);
		d->w->aux = d;
		sethandler(d->w, &handlers);
		**dp = d;
	}
	*dp = &d->next;
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
	int scrn;

	scrn = d->left ? d->left->screen : d->right->screen;

	d->x = x;
	r = rectaddpt(divimg->r, Pt(x - Dx(divimg->r)/2, 0));
	r.min.y = selview->r[scrn].min.y;
	r.max.y = selview->r[scrn].max.y;

	reshapewin(d->w, r);
	mapdiv(d);
}

static void
drawimg(Image *img, Color cbg, Color cborder, Divide *d) {
	Point pt[8];
	int n, start, w;

	w = Dx(img->r)/2;
	n = 0;
	pt[n++] = Pt(w,		0);
	pt[n++] = Pt(0,		0);
	pt[n++] = Pt(w - 1,	w - 1);

	pt[n++] = Pt(w - 1,	Dy(img->r));
	pt[n++] = Pt(w,		pt[n-1].y);

	pt[n++] = Pt(w,		w - 1);
	pt[n++] = Pt(2*w - 1,	0);
	pt[n++] = Pt(w,		0);

	start = d->left		? 0 : n/2;
	n = d->right && d->left ? n : n/2;

	fillpoly(img, pt + start, n, cbg);
	drawpoly(img, pt + start, n, CapNotLast, 1, cborder);
}

static void
drawdiv(Divide *d) {

	fill(divmask, divmask->r, (Color){0});
	drawimg(divmask, (Color){1}, (Color){1}, d);
	drawimg(divimg, divcolor.bg, divcolor.border, d);

	copyimage(d->w, divimg->r, divimg, ZP);
	setshapemask(d->w, divmask, ZP);
}

static void
update_imgs(void) {
	Divide *d;
	int w, h;

	w = 2 * (labelh(def.font) / 3);
	w = max(w, 10);
	/* XXX: Multihead. */
	h = Dy(scr.rect);

	if(divimg) {
		if(w == Dx(divimg->r) && h == Dy(divimg->r)
		&& !memcmp(&divcolor, &def.normcolor, sizeof divcolor))
			return;
		freeimage(divimg);
		freeimage(divmask);
	}

	divimg = allocimage(w, h, scr.depth);
	divmask = allocimage(w, h, 1);
	divcolor = def.normcolor;

	for(d = divs; d && d->w->mapped; d = d->next)
		drawdiv(d);
}

void
div_update_all(void) {
	Divide **dp, *d;
	Area *a, *ap;
	View *v;
	int s;

	update_imgs();

	v = selview;
	dp = &divs;
	ap = nil;
	foreach_column(v, s, a) {
		if (ap && ap->screen != s)
			ap = nil;

		d = getdiv(&dp);
		d->left = ap;
		d->right = a;
		div_set(d, a->r.min.x);
		drawdiv(d);
		ap = a;

		if(!a->next) {
			d = getdiv(&dp);
			d->left = a;
			d->right = nil;
			div_set(d, a->r.max.x);
			drawdiv(d);
		}
	}
	for(d = *dp; d; d = d->next)
		unmapdiv(d);
}

/* Div Handlers */
static bool
bdown_event(Window *w, void *aux, XButtonEvent *e) {
	Divide *d;

	USED(e);
	
	d = aux;
	mouse_resizecol(d);
	return false;
}

static bool
expose_event(Window *w, void *aux, XExposeEvent *e) {
	Divide *d;
	
	USED(e);
	
	d = aux;
	drawdiv(d);
	return false;
}

static Handlers handlers = {
	.bdown = bdown_event,
	.expose = expose_event,
};

