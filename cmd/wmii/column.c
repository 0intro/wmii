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
	if(v == screen->sel)
		view_focus(screen, v);
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

static void
column_scale(Area *a) {
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
				f->collapsed = false;
				area_moveto(f->view->area, f);
				continue;
			}
			i--;
		}else {
			if(j < 0 && (f != a->sel))
				f->collapsed = true;
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

	if(surplus < 0) {
		print("Badness: surplus = %d\n", surplus);
		surplus = 0;
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
	View *v;

	if(a->floating || !a->frame)
		return;

	v = a->view;

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
	view_focus(screen, a->view);
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
	else
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
	else
		r.min.x = max(r.min.x, v->r.min.x);

	if(ar)
		r.max.x = min(r.max.x, ar->r.max.x - minw);
	else
		r.max.x = min(r.max.x, v->r.max.x);

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
	if(v == screen->sel)
		view_focus(screen, v);
}

