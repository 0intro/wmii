/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include "fns.h"

static Image *divimg, *divmask;
static CTuple divc;
static Handlers divhandler;

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

int
str2colmode(const char *str) {
	int i;
	
	for(i = 0; i < nelem(modes); i++)
		if(!strcasecmp(str, modes[i]))
			return i;
	return -1;
}

char*
colmode2str(int i) {
	if(i < nelem(modes))
		return modes[i];
	return nil;
}

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
	sethandler(d->w, &divhandler);

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
	h = Dy(screen->r);

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

static Handlers divhandler = {
	.bdown = bdown_event,
	.expose = expose_event,
};

Area*
column_new(View *v, Area *pos, uint w) {
	Area *a;

	a = area_create(v, pos, w);
	if(!a)
		return nil;

	view_arrange(v);
	if(v == screen->sel)
		view_focus(screen, v);
	return a;
}

static void
scale_column(Area *a) {
	Frame *f, **fp;
	uint minh, yoff, dy;
	uint ncol, nuncol;
	uint colh, uncolh;
	int surplus, i, j;

	if(!a->frame)
		return;

	minh = labelh(def.font);
	colh = labelh(def.font);
	uncolh = minh + colh +1;

	ncol = 0;
	nuncol = 0;
	dy = 0;
	for(f=a->frame; f; f=f->anext) {
		frame_resize(f, f->r);
		if(f->collapsed)
			ncol++;
		else
			nuncol++;
	}

	surplus = Dy(a->r) - (ncol * colh) - (nuncol * uncolh);

	/* Collapse until there is room */
	if(surplus < 0) {
		i = ceil((float)(-surplus) / (uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	/* Push to the floating layer until there is room */
	if(surplus < 0) {
		i = ceil((float)(-surplus)/colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		/* surplus += i * colh; */
	}

	j = nuncol - 1;
	i = ncol - 1;
	/* Decide  which to collapse, float */
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f == a->sel)
			i++, j++;
		if(f->collapsed) {
			if(i < 0 && (f != a->sel)) {
				f->collapsed = False;
				area_moveto(f->view->area, f);
				continue;
			}
			i--;
		}else {
			if(j < 0 && (f != a->sel))
				f->collapsed = True;
			j--;
		}
		/* Doesn't change if we 'continue' */
		fp = &f->anext;
	}

	surplus = 0;
	for(f=a->frame; f; f=f->anext) {
		f->r = rectsubpt(f->r, f->r.min);
		f->crect = rectsubpt(f->crect, f->crect.min);
		f->r.max.x = Dx(a->r);

		if(f->collapsed) {
			f->r.max.y = colh;
		}else {
			f->r.max.y = uncolh;
			dy += Dy(f->crect);
		}
		surplus += Dy(f->r);
	}
	for(f = a->frame; f; f = f->anext)
		f->ratio = (float)Dy(f->crect)/dy;

	j = 0;
	surplus = Dy(a->r) - surplus;
	while(surplus > 0 && surplus != j) {
		j = surplus;
		dy = 0;
		for(f=a->frame; f; f=f->anext) {
			if(!f->collapsed)
				f->r.max.y += f->ratio * surplus;
			frame_resize(f, f->r);
			dy += Dy(f->r);
		}
		surplus = Dy(a->r) - dy;
	}
	for(f=a->frame; f && surplus > 0; f=f->anext) {
		if(!f->collapsed) {
			dy = Dy(f->r);
			f->r.max.y += surplus;
			frame_resize(f, f->r);
			f->r.max.y = Dy(f->crect) + labelh(def.font) + 1;
			surplus -= Dy(f->r) - dy;
		}
	}

	yoff = a->r.min.y;
	i = nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->r = rectaddpt(f->r, Pt(a->r.min.x, yoff));

		if(!f->collapsed) {
			i--;
			f->r.max.y += surplus / nuncol;
			if(!i)
				f->r.max.y += surplus % nuncol;
		}
		yoff = f->r.max.y;
	}
}

void
column_arrange(Area *a, bool dirty) {
	Frame *f;

	if(a->floating || !a->frame)
		return;

	switch(a->mode) {
	case Coldefault:
		if(dirty)
			for(f=a->frame; f; f=f->anext)
				f->r = Rect(0, 0, 100, 100);
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			f->r = a->r;
		}
		goto resize;
	default:
		die("can't get here");
		break;
	}
	scale_column(a);
resize:
	if(a->view == screen->sel) {
		view_restack(a->view);
		client_resize(a->sel->client, a->sel->r);

		for(f=a->frame; f; f=f->anext)
			if(!f->collapsed && f != a->sel)
				client_resize(f->client, f->r);
		for(f=a->frame; f; f=f->anext)
			if(f->collapsed && f != a->sel)
				client_resize(f->client, f->r);
	}
}

void
column_resize(Area *a, int w) {
	Area *an;
	int dw;

	an = a->next;
	assert(an != nil);

	dw = w - Dx(a->r);
	a->r.max.x += dw;
	an->r.min.x += dw;

	view_arrange(a->view);
	view_focus(screen, a->view);
}

static void
resize_colframeh(Frame *f, Rectangle *r) {
	Area *a;
	Frame *fn, *fp;
	uint minh;

	minh = 2 * labelh(def.font);

	a = f->area;
	fn = f->anext;
	fp = f->aprev;

	if(fp)
		r->min.y = max(r->min.y, fp->r.min.y + minh);
	else
		r->min.y = max(r->min.y, a->r.min.y);

	if(fn)
		r->max.y = min(r->max.y, fn->r.max.y - minh);
	else
		r->max.y = min(r->max.y, a->r.max.y);

	if(fp) {
		fp->r.max.y = r->min.y;
		frame_resize(fp, fp->r);
	}
	if(fn) {
		fn->r.min.y = r->max.y;
		frame_resize(fn, fn->r);
	}

	frame_resize(f, *r);
}

void
column_resizeframe(Frame *f, Rectangle *r) {
	Area *a, *al, *ar;
	View *v;
	uint minw;
	int dx, dw, maxx;

	a = f->area;
	v = a->view;
	maxx = r->max.x;

	minw = Dx(screen->r) / NCOL;

	al = a->prev;
	ar = a->next;

	if(al)
		r->min.x = max(r->min.x, al->r.min.x + minw);
	else
		r->min.x = max(r->min.x, 0);

	if(ar)
		maxx = min(maxx, a->r.max.x - minw);
	else
		maxx = min(maxx, screen->r.max.x);

	dx = a->r.min.x - r->min.x;
	dw = maxx - a->r.max.x;
	if(al) {
		al->r.max.x -= dx;
		column_arrange(al, False);
	}
	if(ar) {
		ar->r.max.x -= dw;
		column_arrange(ar, False);
	}

	resize_colframeh(f, r);

	a->r.max.x = maxx;
	view_arrange(a->view);

	view_focus(screen, v);
}
