/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <assert.h>
#include <math.h>
#include "fns.h"

static void place_frame(Frame *f);

Client*
area_selclient(Area *a) {               
	if(a && a->sel)
		return a->sel->client;
	return nil;
}

uint
area_idx(Area *a) {
	View *v;
	Area *ap;
	uint i;

	v = a->view;
	for(i = 0, ap = v->area; a != ap; ap = ap->next)
		i++;
	return i;
}

char*
area_name(Area *a) {
	static char buf[16];
	
	if(a->floating)
		return "~";
	snprint(buf, sizeof(buf), "%d", area_idx(a));
	return buf;
}

Area*
area_create(View *v, Area *pos, uint w) {
	static ushort id = 1;
	uint areanum, i;
	uint minwidth;
	int colnum;
	Area *a;

	minwidth = Dx(screen->r)/NCOL;

	i = 0;
	for(a = v->area; a != pos; a = a->next)
		 i++;
	areanum = 0;
	for(a = v->area; a; a = a->next)
		areanum++;

	colnum = areanum - 1;
	if(w == 0) {
		if(colnum >= 0) {
			w = view_newcolw(v, i);
			if (w == 0)
				w = Dx(screen->r) / (colnum + 1);
		}
		else
			w = Dx(screen->r);
	}

	if(w < minwidth)
		w = minwidth;
	if(colnum && (colnum * minwidth + w) > Dx(screen->r))
		return nil;

	if(pos)
		view_scale(v, Dx(screen->r) - w);

	a = emallocz(sizeof *a);
	a->view = v;
	a->id = id++;
	a->mode = def.colmode;
	a->frame = nil;
	a->sel = nil;

	a->r = screen->r;
	a->r.min.x = 0;
	a->r.max.x = w;
	a->r.max.y = screen->brect.min.y;

	if(pos) {
		a->next = pos->next;
		a->prev = pos;
	}else {
		a->next = v->area;
		v->area = a;
	}
	if(a->prev)
		a->prev->next = a;
	if(a->next)
		a->next->prev = a;

	if(a == v->area)
		a->floating = True;

	if(v->sel == nil)
		area_focus(a);

	if(!a->floating)
		event("CreateColumn %ud\n", i);
	return a;
}

void
area_destroy(Area *a) {
	Client *c;
	Area *ta;
	View *v;
	int idx;

	v = a->view;

	if(a->frame)
		die("destroying non-empty area");

	if(v->revert == a)
		v->revert = nil;

	for(c=client; c; c=c->next)
		if(c->revert == a)
			c->revert = nil;

	idx = area_idx(a);

	if(a->prev && !a->prev->floating)
		ta = a->prev;
	else
		ta = a->next;

	if(a == v->colsel)
		v->colsel = ta;

	/* Can only destroy the floating area when destroying a
	 * view---after destroying all columns.
	 */
	assert(a->prev || a->next == nil);
	if(a->prev)
		a->prev->next = a->next;
	if(a->next)
		a->next->prev = a->prev;

	if(ta && v->sel == a) {
		area_focus(ta);
	}
	event("DestroyArea %d\n", idx);
	/* Deprecated */
	event("DestroyColumn %d\n", idx);

	free(a);
}

void
area_moveto(Area *to, Frame *f) {
	Area *from;

	assert(to->view == f->view);

	from = f->area;

	if(to->floating != from->floating) {
		Rectangle tr;
		
		tr = f->revert;
		f->revert = f->r;
		f->r = temp;
	}
	f->client->revert = from;

	area_detach(f);
	area_attach(to, f);
}

void
area_setsel(Area *a, Frame *f) {
	View *v;

	v = a->view;
	if(a == v->sel && f)
		frame_focus(f);
	else
		a->sel = f;
}

void
area_attach(Area *a, Frame *f) {
	uint nframe;
	Frame *ft;
	Client *c;

	c = f->client;

	f->area = a;

	nframe = 0;
	for(ft=a->frame; ft; ft=ft->anext)
		nframe++;
	nframe = max(nframe, 1);

	c->floating = a->floating;
	if(!a->floating) {
		f->r = a->r;
		f->r.max.y = Dy(a->r) / n_frame;
	}

	frame_insert(a->sel, f);

	if(a->floating) {
		place_frame(f);
		client_resize(f->client, f->r);
	}

	if(!a->sel)
		area_setsel(a, f);
	view_restack(a->view);

	if(!a->floating)
		column_arrange(a, False);

	if(a->frame)
		assert(a->sel);
}

void
area_detach(Frame *f) {
	Frame *pr;
	Client *c;
	Area *a;
	View *v;

	a = f->area;
	v = a->view;
	c = f->client;

	pr = f->aprev;
	frame_remove(f);

	if(!a->floating) {
		if(a->frame)
			column_arrange(a, False);
		else {
			if(v->area->next->next)
				area_destroy(a);
			else if((a->frame == nil) && (v->area->frame))
				area_focus(v->area);
			view_arrange(v);
		}
	}else if(v->oldsel)
		area_focus(v->oldsel);
	else if(!a->frame) {
		if(v->colsel->frame)
			area_focus(v->colsel);
	}else
		assert(a->sel);

	if(a->sel == f) {
		if(!pr)
			pr = a->frame;
		area_setsel(a, pr);
	}
}

static void
bit_set(uint *field, uint width, uint x, uint y, int set) {
	enum { divisor = sizeof(uint) * 8 };
	uint bx, mask;
	div_t d;

	d = div(x, divisor);
	bx = d.quot;
	mask = 1 << d.rem;
	if(set)
		field[y*width + bx] |= mask;
	else
		field[y*width + bx] &= ~mask;
}

static Bool
bit_get(uint *field, uint width, uint x, uint y) {
	enum { divisor = sizeof(uint) * 8 };
	uint bx, mask;
	div_t d;

	d = div(x, divisor);
	bx = d.quot;
	mask = 1 << d.rem;

	return (field[y*width + bx] & mask) != 0;
}

static void
place_frame(Frame *f) {
	enum { divisor = sizeof(uint) * 8 };
	enum { dx = 8, dy = 8 };

	static uint mwidth, mx, my;
	static uint *field = nil;
	Align align;
	Point p1 = ZP;
	Point p2 = ZP;
	Rectangle *rects;
	Frame *fr;
	Client *c;
	Area *a;
	bool fit;
	uint i, j, x, y, cx, cy, maxx, maxy, diff, num;
	int snap;

	snap = Dy(screen->r) / 66;
	num = 0;
	fit = False;
	align = CENTER;

	a = f->area;
	c = f->client;

	if(c->trans)
		return;
	if(c->fullscreen || c->w.hints->position || starting) {
		f->r = gravclient(c, c->r);
		return;
	}
	if(!field) {
		mx = Dx(screen->r) / dx;
		my = Dy(screen->r) / dy;
		mwidth = ceil((float)mx / divisor);
		field = emallocz(sizeof(uint) * mwidth * my);
	}

	SET(cx);
	SET(cy);
	memset(field, ~0, (sizeof(uint) * mwidth * my));
	for(fr=a->frame; fr; fr=fr->anext) {
		if(fr == f) {
			cx = Dx(f->r) / dx;
			cy = Dx(f->r) / dy;
			continue;
		}

		if(fr->r.min.x < 0)
			x = 0;
		else
			x = fr->r.min.x / dx;

		if(fr->r.min.y < 0)
			y = 0;
		else
			y = fr->r.min.y / dy;

		maxx = fr->r.max.x / dx;
		maxy = fr->r.max.y / dy;
		for(j = y; j < my && j < maxy; j++)
			for(i = x; i < mx && i < maxx; i++)
				bit_set(field, mwidth, i, j, False);
	}

	for(y = 0; y < my; y++)
		for(x = 0; x < mx; x++) {
			if(bit_get(field, mwidth, x, y)) {
				for(i = x; i < mx; i++)
					if(bit_get(field, mwidth, i, y) == 0)
						break;
				for(j = y; j < my; j++)
					if(bit_get(field, mwidth, x, j) == 0)
						break;

				if(((i - x) * (j - y) > (p2.x - p1.x) * (p2.y - p1.y)) 
					&& (i - x > cx) && (j - y > cy))
				{
					fit = True;
					p1.x = x;
					p1.y = y;
					p2.x = i;
					p2.y = j;
				}
			}
		}

	if(fit) {
		p1.x *= dx;
		p1.y *= dy;
	}

	if(!fit || (p1.x + Dx(f->r) > a->r.max.x)) {
		diff = Dx(a->r) - Dx(f->r);
		p1.x = a->r.min.x + (random() % max(diff, 1));
	}

	if(!fit && (p1.y + Dy(f->r) > a->r.max.y)) {
		diff = Dy(a->r) - Dy(f->r);
		p1.y = a->r.min.y + (random() % max(diff, 1));
	}

	p1 = subpt(p1, f->r.min);
	f->r = rectaddpt(f->r, p1);

	rects = rects_of_view(a->view, &num, nil);
	snap_rect(rects, num, &f->r, &align, snap);
	if(rects)
		free(rects);
}

void
area_focus(Area *a) {
	Frame *f;
	View *v;
	Area *old_a;

	v = a->view;
	f = a->sel;
	old_a = v->sel;

	v->sel = a;

	if((old_a) && (a->floating != old_a->floating))
		v->revert = old_a;

	if(v != screen->sel)
		return;

	move_focus(old_a->sel, f);

	if(f)
		client_focus(f->client);
	else
		client_focus(nil);

	if(a != old_a) {
		event("AreaFocus %s\n", area_name(a));
		/* Deprecated */
		if(a->floating)
			event("FocusFloating\n");
		else
			event("ColumnFocus %d\n", area_idx(a));
	}
}
