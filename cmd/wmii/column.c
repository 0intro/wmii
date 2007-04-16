/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

int divw, divh;
GC divgc;
GC maskgc;

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

Divide *
win2div(XWindow w) {
	Divide *d;
	
	for(d = divs; d; d = d->next)
		if(d->w->w == w) return d;
	return nil;
}

int
str2colmode(const char *str) {
	int i;
	
	for(i = 0; i < nelem(modes); i++)
		if(!strcasecmp(str, modes[i]))
			return i;
	return -1;
}

static void
draw_pmap(Image *img, ulong cbg, ulong cborder) {
	Point pt[6];

	pt[0] = Pt(0, 0);
	pt[1] = Pt(divw/2 - 1, divw/2 - 1);

	pt[2] = Pt(pt[1].x, divh);
	pt[3] = Pt(divw/2, pt[2].y);

	pt[4] = Pt(pt[3].x, divw/2 - 1);
	pt[5] = Pt(divw - 1, 0);

	fillpoly(img, pt, nelem(pt), cbg);
	drawpoly(img, pt, nelem(pt), CapNotLast, 1, cborder);
}

void
update_dividers() {
	if(divimg) {
		freeimage(divimg);
		freeimage(divmask);
	}

	divw = 2 * (labelh(def.font) / 3);
	divw = max(divw, 10);
	divh = Dy(screen->rect);

	divimg = allocimage(divw, divh, scr.depth);
	divmask = allocimage(divw, divh, 1);

	fill(divmask, divmask->r, 0);

	draw_pmap(divimg, def.normcolor.bg, def.normcolor.border);
	draw_pmap(divmask, 1, 1);
}

static Divide*
get_div(Divide **dp) {
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

	*dp = d;
	return d;
}

static void
map_div(Divide *d) {
	mapwin(d->w);
}

static void
unmap_div(Divide *d) {
	unmapwin(d->w);
}

void
setdiv(Divide *d, int x) {
	Rectangle r;

	d->x = x;
	r = rectaddpt(divimg->r, Pt(x - divw/2, 0));
	r.max.y = screen->brect.min.y;

	reshapewin(d->w, r);
	map_div(d);
}

void
update_divs() {
	Divide **dp, *d;
	Area *a;
	View *v;

	update_dividers();

	v = screen->sel;
	dp = &divs;
	for(a = v->area->next; a; a = a->next) {
		d = get_div(dp);
		dp = &d->next;
		setdiv(d, a->rect.min.x);

		if(!a->next) {
			d = get_div(dp);
			dp = &d->next;
			setdiv(d, a->rect.max.x);
		}
	}
	for(d = *dp; d; d = d->next)
		unmap_div(d);
}

void
draw_div(Divide *d) {
	copyimage(d->w, divimg->r, divimg, ZP);
	setshapemask(d->w, divmask, ZP);
}

Area *
new_column(View *v, Area *pos, uint w) {
	Area *a;
	
	a = create_area(v, pos, w);
	if(!a)
		return nil;

	arrange_view(v);
	if(v == screen->sel)
		focus_view(screen, v);
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

	/* This works by comparing heights based on a surplus of their
	 * minimum size. We start by subtracting the minimum size, then
	 * scale the surplus, and add back the minimum size later. This
	 * is based on the size of the client, rather than the frame, so
	 * increment gaps can be equalized later */
	/* Frames that can't be accomodated are pushed to the floating layer */

	minh = labelh(def.font);
	colh = labelh(def.font);
	uncolh = minh + frame_delta_h();

	ncol = 0;
	nuncol = 0;
	dy = 0;
	for(f=a->frame; f; f=f->anext)
		if(f->collapsed)
			ncol++;
		else
			nuncol++;

	surplus = Dy(a->rect);
	surplus -= ncol * colh;
	surplus -= nuncol * uncolh;
	if(surplus < 0) {
		i = ceil((float)(-surplus)/(uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	if(surplus < 0) {
		i = ceil((float)(-surplus)/colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		surplus += i * colh;
	}

	i = ncol - 1;
	j = nuncol - 1;
	for(f=a->frame; f; f=f->anext) {
		f->rect = rectsubpt(f->rect, f->rect.min);
		f->crect = rectsubpt(f->crect, f->crect.min);
		if(f == a->sel)
			j++;
		if(!f->collapsed) {
			if(j < 0 && f != a->sel)
				f->collapsed = True;
			else {
				if(Dx(f->crect) <= minh)
					f->crect.max.x = 1;
				else
					f->crect.max.x = minh;
				dy += Dy(f->crect);
			}
			j--;
		}
	}
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f == a->sel)
			i++;
		if(f->collapsed) {
			if(i < 0 && f != a->sel) {
				f->collapsed = False;
				send_to_area(f->view->area, f);
				continue;
			}
			i--;
		}
		fp=&f->anext;
	}

	i = nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect.max.x = Dx(a->rect);
		if(!f->collapsed) {
			i--;
			if(i)
				f->rect.max.y = (float)Dy(f->crect) / dy * surplus;
			else
				f->rect.max.y = surplus;
			f->rect.max.y += minh + frame_delta_h();

			apply_sizehints(f->client, &f->rect, False, True, NWEST);
			dy -= Dy(f->crect);
			resize_frame(f, f->rect);

			surplus -= Dy(f->rect) - frame_delta_h() - minh;
		}else
			f->rect.max.y = labelh(def.font);
	}

	yoff = a->rect.min.y;
	i = nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect = rectaddpt(f->rect, Pt(a->rect.min.x, yoff));
		f->rect.max.x = a->rect.max.x;
		if(!f->collapsed) {
			i--;
			f->rect.max.y += surplus / nuncol;
			if(!i)
				f->rect.max.y += surplus % nuncol;
		}
		yoff = f->rect.max.y;
	}
}

void
arrange_column(Area *a, Bool dirty) {
	Frame *f;

	if(a->floating || !a->frame)
		return;

	switch(a->mode) {
	case Coldefault:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			if(dirty)
				f->crect = Rect(0, 0, 100, 100);
		}
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			f->rect = a->rect;
		}
		goto resize;
	default:
		assert(!"Can't happen");
		break;
	}
	scale_column(a);
resize:
	if(a->view == screen->sel) {
		restack_view(a->view);
		resize_client(a->sel->client, &a->sel->rect);

		for(f=a->frame; f; f=f->anext)
			if(!f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
		for(f=a->frame; f; f=f->anext)
			if(f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
	}
}

void
resize_column(Area *a, int w) {
	Area *an;
	int dw;

	an = a->next;
	assert(an != nil);

	dw = w - Dx(a->rect);
	a->rect.max.x += dw;
	an->rect.min.x += dw;

	arrange_view(a->view);
	focus_view(screen, a->view);
}

static void
resize_colframeh(Frame *f, Rectangle *r) {
	Area *a;
	Frame *fa, *fb;
	uint minh;

	minh = 2 * labelh(def.font);

	a = f->area;
	fa = f->anext;
	for(fb = a->frame; fb; fb = fb->anext)
		if(fb->anext == f) break;

	if(fb)
		r->min.y = max(r->min.y, fb->rect.min.y + minh);
	else
		r->min.y = max(r->min.y, a->rect.min.y);

	if(fa)
		r->max.y = min(r->max.y, fa->rect.max.y - minh);
	else
		r->max.y = min(r->max.y, a->rect.max.y);

	if(fb) {
		fb->rect.max.y = r->min.y;
		resize_frame(fb, fb->rect);
	}
	if(fa) {
		fa->rect.min.y = r->max.y;
		resize_frame(fa, fa->rect);
	}

	resize_frame(f, *r);
}

void
resize_colframe(Frame *f, Rectangle *r) {
	Area *a, *al, *ar;
	View *v;
	uint minw;
	int dx, dw, maxx;

	a = f->area;
	v = a->view;
	maxx = r->max.x;

	minw = Dx(screen->rect) / NCOL;

	ar = a->next;
	for(al = v->area->next; al; al = al->next)
		if(al->next == a) break;

	if(al)
		r->min.x = max(r->min.x, al->rect.min.x + minw);
	else
		r->min.x = max(r->min.x, 0);

	if(ar) {
		if(maxx >= ar->rect.max.x - minw)
			maxx = ar->rect.max.x - minw;
	}
	else
		if(maxx > screen->rect.max.x)
			maxx = screen->rect.max.x;

	dx = a->rect.min.x - r->min.x;
	dw = maxx - a->rect.max.x;
	if(al) {
		al->rect.max.x -= dx;
		arrange_column(al, False);
	}
	if(ar) {
		ar->rect.max.x -= dw;
		arrange_column(ar, False);
	}

	resize_colframeh(f, r);

	a->rect.max.x = maxx;
	arrange_view(a->view);

	focus_view(screen, v);
}
