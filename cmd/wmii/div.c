/* Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
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

	if(**dp) {
		d = **dp;
		d->side = 0;
	}else {
		d = emallocz(sizeof *d);

		wa.override_redirect = true;
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
drawimg(Image *img, ulong cbg, ulong cborder, int side) {
	Point pt[6];

	pt[0] = Pt(0, 0);
	pt[1] = Pt(Dx(img->r)/2 - 1, Dx(img->r)/2 - 1);

	pt[2] = Pt(pt[1].x, Dy(img->r));
	pt[3] = Pt(Dx(img->r)/2, pt[2].y);

	pt[4] = Pt(pt[3].x, Dx(img->r)/2 - 1);
	pt[5] = Pt(Dx(img->r) - 1, 0);

	if (side & 1)
		pt[0].x = pt[1].x = pt[2].x + 1;
	if (side & 2)
		pt[5].x = pt[4].x = pt[3].x - 1;

	fillpoly(img, pt, nelem(pt), cbg);
	drawpoly(img, pt, nelem(pt), CapNotLast, 1, cborder);
}

static void
drawdiv(Divide *d) {

	fill(divmask, divmask->r, 0);
	drawimg(divmask, 1, 1, d->side);
	drawimg(divimg, divcolor.bg, divcolor.border, d->side);

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
	h = Dy(selview->screenr);

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
		ap = a;
		if (!ap)
			d->side |= 1;

		if(!a->next) {
			d = getdiv(&dp);
			d->left = a;
			d->right = nil;
			div_set(d, a->r.max.x);
			d->side |= 2;
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

