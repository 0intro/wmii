/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <strings.h>
#include "fns.h"

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
colmode2str(uint i) {
	if(i < nelem(modes))
		return modes[i];
	return nil;
}

Area*
column_new(View *v, Area *pos, uint w) {
	Area *a;

	a = area_create(v, pos, w);
	return a;
#if 0
	if(!a)
		return nil;

	view_arrange(v);
	view_update(v);
#endif
}

void
column_insert(Area *a, Frame *f, Frame *pos) {

	f->area = a;
	f->client->floating = false;
	f->column = area_idx(a);
	frame_insert(f, pos);
	if(a->sel == nil)
		area_setsel(a, f);
}

void
column_attach(Area *a, Frame *f) {
	uint nframe;
	Frame *ft;

	nframe = 0;
	for(ft=a->frame; ft; ft=ft->anext)
		nframe++;
	nframe = max(nframe, 1);

	f->r = a->r;
	f->r.max.y = Dy(a->r) / nframe;
	/* COLR */
	f->colr = a->r;
	f->colr.max.y = Dy(a->r) / nframe;

	column_insert(a, f, a->sel);
	column_arrange(a, false);
}

static void column_scale(Area*);

void
column_attachrect(Area *a, Frame *f, Rectangle r) {
	Frame *fp, *pos;
	int before, after;

	pos = nil;
	for(fp=a->frame; fp; pos=fp, fp=fp->anext) {
		if(r.max.y < fp->r.min.y)
			continue;
		if(r.min.x > fp->r.max.y)
			continue;
		before = fp->r.min.y - r.min.y;
		after = r.max.y - fp->r.max.y;
		if(abs(before) <= abs(after))
			break;
	}
	if(Dy(a->r) > Dy(r)) {
		/* Kludge. */
		a->r.max.y -= Dy(r);
		column_scale(a);
		a->r.max.y += Dy(r);
	}
	column_insert(a, f, pos);
	for(fp=f->anext; fp; fp=fp->anext) {
		fp->r.min.y += Dy(r);
		fp->r.max.y += Dy(r);
	}
	column_resizeframe(f, r);
}

void
column_remove(Frame *f) {
	Frame *pr;
	Area *a;

	a = f->area;
	pr = f->aprev;

	frame_remove(f);

	f->area = nil;
	if(a->sel == f) {
		if(!pr)
			pr = a->frame;
		a->sel = nil;
		area_setsel(a, pr);
	}
}

void
column_detach(Frame *f) {
	Area *a;

	a = f->area;
	column_remove(f);
	if(a->frame)
		column_arrange(a, false);
	else if(a->view->area->next->next)
		area_destroy(a);
}

static int
column_surplus(Area *a) {
	Frame *f;
	int surplus;

	surplus = Dy(a->r);
	for(f=a->frame; f; f=f->anext)
		surplus -= Dy(f->r);
	return surplus;
}

static void
column_fit(Area *a, uint *ncolp, uint *nuncolp) {
	Frame *f, **fp;
	uint minh, dy;
	uint ncol, nuncol;
	uint colh, uncolh;
	int surplus, i, j;

	/* The minimum heights of collapsed and uncollpsed frames.
	 */
	minh = labelh(def.font);
	colh = labelh(def.font);
	uncolh = minh + colh + 1;

	/* Count collapsed and uncollapsed frames. */
	ncol = 0;
	nuncol = 0;
	for(f=a->frame; f; f=f->anext) {
		frame_resize(f, f->colr);
		if(f->collapsed)
			ncol++;
		else
			nuncol++;
	}

	if(nuncol == 0) {
		nuncol++;
		ncol--;
		if(a->sel)
			a->sel->collapsed = false;
		else
			a->frame->collapsed = false;
	}

	/* FIXME: Kludge. */
	dy = Dy(a->view->r) - Dy(a->r);
	minh = colh * (ncol + nuncol - 1) + uncolh;
	if(dy && Dy(a->r) < minh)
		a->r.max.y += min(dy, minh - Dy(a->r));

	surplus = Dy(a->r)
		- (ncol * colh)
		- (nuncol * uncolh);

	/* Collapse until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / (uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	/* Push to the floating layer until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		surplus += i * colh;
	}

	/* Decide which to collapse and which to float. */
	j = nuncol - 1;
	i = ncol - 1;
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f != a->sel) {
			if(!f->collapsed) {
				if(j < 0)
					f->collapsed = true;
				j--;
			}
			if(f->collapsed) {
				if(i < 0) {
					f->collapsed = false;
					area_moveto(f->view->area, f);
					continue;
				}
				i--;
			}
		}
		/* Doesn't change if we 'continue' */
		fp = &f->anext;
	}

	if(ncolp) *ncolp = ncol;
	if(nuncolp) *nuncolp = nuncol;
}

static void
column_settle(Area *a) {
	Frame *f;
	uint yoff, yoffcr;
	int i, surplus, nuncol, n;

	nuncol = 0;
	surplus = column_surplus(a);
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) nuncol++;

	yoff = a->r.min.y;
	yoffcr = yoff;
	i = nuncol;
	n = 0;
	if(surplus / nuncol == 0)
		n = surplus;
	for(f=a->frame; f; f=f->anext) {
		f->r = rectsetorigin(f->r, Pt(a->r.min.x, yoff));
		f->colr = rectsetorigin(f->colr, Pt(a->r.min.x, yoffcr));
		f->r.min.x = a->r.min.x;
		f->r.max.x = a->r.max.x;
		if(!f->collapsed) {
			if(n == 0)
				f->r.max.y += surplus / nuncol;
			else if(n-- > 0)
				f->r.max.y++;
			if(--i == 0)
				f->r.max.y = a->r.max.y;
		}
		yoff = f->r.max.y;
		yoffcr = f->colr.max.y;
	}
}

static int
foo(Frame *f) {
	WinHints h;
	int maxh;

	h = frame_gethints(f);
	maxh = 0;
	if(h.aspect.max.x)
		maxh = h.baspect.y +
		       (Dx(f->r) - h.baspect.x) *
		       h.aspect.max.y / h.aspect.max.x;
	maxh = max(maxh, h.max.y);

	if(Dy(f->r) > maxh)
		return 0;
	return h.inc.y - (Dy(f->r) - h.base.y) % h.inc.y;
}

static int
comp_frame(const void *a, const void *b) {
	Frame *fa, *fb;
	int ia, ib;

	fa = *(Frame**)a;
	fb = *(Frame**)b;
	ia = foo(fa);
	ib = foo(fb);
	return ia < ib             ? -1 :
	       ia > ib             ?  1 :
	       /* Favor the selected client. */
	       fa == fa->area->sel ? -1 :
	       fb == fa->area->sel ?  1 :
	                              0;
}

static void
column_squeeze(Area *a) {
	static Vector_ptr fvec; 
	WinHints h;
	Frame **fp;
	Frame *f;
	int surplus, osurplus, dy;

	fvec.n = 0;
	for(f=a->frame; f; f=f->anext) {
		h = frame_gethints(f);
		f->r = sizehint(&h, f->r);
		vector_ppush(&fvec, f);
	}
	fp = (Frame**)fvec.ary;
	/* I would prefer an unstable sort. Unfortunately, the GNU people
	 * provide a stable one, so, this works better on BSD.
	 */
	qsort(fp, fvec.n, sizeof *fp, comp_frame);

	surplus = column_surplus(a);
	for(osurplus=0; surplus != osurplus;) {
		osurplus = surplus;
		for(; f=*fp; fp++) {
			dy = foo(f);
			if(dy > surplus)
				break;
			surplus -= dy;
			f->r.max.y += dy;
		}
	}
}

void
column_frob(Area *a) {
	Frame *f;

	for(f=a->frame; f; f=f->anext)
		f->r = f->colr;
	column_settle(a);
	if(a->view == screen->sel)
	for(f=a->frame; f; f=f->anext)
		client_resize(f->client, f->r);
}

static void
column_scale(Area *a) {
	Frame *f;
	uint dy;
	uint ncol, nuncol;
	uint colh;
	int surplus;

	if(!a->frame)
		return;

	column_fit(a, &ncol, &nuncol);
	colh = labelh(def.font);
	surplus = Dy(a->r);

	/* Distribute the surplus.
	 */
	dy = 0;
	surplus = Dy(a->r);
	for(f=a->frame; f; f=f->anext) {
		if(f->collapsed)
			f->colr.max.y = f->colr.min.y + colh;
		surplus -= Dy(f->colr);
		if(!f->collapsed)
			dy += Dy(f->colr);
	}
	for(f=a->frame; f; f=f->anext) {
		WinHints h;

		f->dy = Dy(f->r);
		f->colr.min.x = a->r.min.x;
		f->colr.max.x = a->r.max.x;
		if(!f->collapsed)
			f->colr.max.y += ((float)f->dy / dy) * surplus;
		frame_resize(f, f->colr);
		if(!f->collapsed) {
			h = frame_gethints(f);
			f->r = sizehint(&h, f->r);
		}
	}

	if(def.incmode == ISqueeze)
		column_squeeze(a);
	column_settle(a);
}

void
column_arrange(Area *a, bool dirty) {
	Frame *f;
	View *v;

	if(a->floating || !a->frame)
		return;

	v = a->view;

	switch(a->mode) {
	case Coldefault:
		if(dirty)
			for(f=a->frame; f; f=f->anext)
				f->r = Rect(0, 0, 100, 100);
		/* COLR */
		if(dirty)
			for(f=a->frame; f; f=f->anext)
				f->colr = Rect(0, 0, 100, 100);
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = false;
			f->r = a->r;
		}
		goto resize;
	default:
		die("can't get here");
		break;
	}
	column_scale(a);
resize:
	if(v == screen->sel) {
		view_restack(v);
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

	/* view_arrange(a->view); */
	view_update(a->view);
}

static void
column_resizeframe_h(Frame *f, Rectangle r) {
	Area *a;
	Frame *fn, *fp;
	uint minh;

	minh = labelh(def.font);

	a = f->area;
	fn = f->anext;
	fp = f->aprev;

	if(fp)
		r.min.y = max(r.min.y, fp->r.min.y + minh);
	else /* XXX. */
		r.min.y = max(r.min.y, a->r.min.y);

	if(fn)
		r.max.y = min(r.max.y, fn->r.max.y - minh);
	else
		r.max.y = min(r.max.y, a->r.max.y);

	if(fp) {
		fp->r.max.y = r.min.y;
		frame_resize(fp, fp->r);
	}
	if(fn) {
		fn->r.min.y = r.max.y;
		frame_resize(fn, fn->r);
	}

	frame_resize(f, r);
}

void
column_resizeframe(Frame *f, Rectangle r) {
	Area *a, *al, *ar;
	View *v;
	uint minw;

	a = f->area;
	v = a->view;

	minw = Dx(v->r) / NCOL;

	ar = a->next;
	al = a->prev;
	if(al == v->area)
		al = nil;

	if(al)
		r.min.x = max(r.min.x, al->r.min.x + minw);
	else { /* Hm... */
		r.min.x = max(r.min.x, v->r.min.x);
		r.max.x = max(r.max.x, r.min.x + minw);
	}

	if(ar)
		r.max.x = min(r.max.x, ar->r.max.x - minw);
	else {
		r.max.x = min(r.max.x, v->r.max.x);
		r.min.x = min(r.min.x, r.max.x - minw);
	}

	a->r.min.x = r.min.x;
	a->r.max.x = r.max.x;
	if(al) {
		al->r.max.x = a->r.min.x;
		column_arrange(al, false);
	}
	if(ar) {
		ar->r.min.x = a->r.max.x;
		column_arrange(ar, false);
	}

	column_resizeframe_h(f, r);

	/* view_arrange(v); */
	view_update(v);
}

