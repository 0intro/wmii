/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
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

static void place_frame(Frame *f);

Client *
area_selclient(Area *a) {               
	if(a && a->sel)
		return a->sel->client;
	return nil;
}

Area *
create_area(View *v, Area *pos, uint w) {
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
			w = newcolw(v, i);
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
		scale_view(v, Dx(screen->r) - w);

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

	if((v->sel == nil) || (v->sel->floating && v->area == a->prev && a->next == nil))
		focus_area(a);

	if(!a->floating)
		write_event("CreateColumn %d\n", i);
	return a;
}

void
destroy_area(Area *a) {
	Client *c;
	Area *ta;
	View *v;
	uint i;

	v = a->view;

	if(a->frame)
		fatal("destroying non-empty area");

	if(v->revert == a)
		v->revert = nil;

	for(c=client; c; c=c->next)
		if(c->revert == a)
			c->revert = nil;

	i = 0;
	for(ta=v->area; ta != a; ta=ta->next)
		i++;
	
	if(a->prev)
		ta = a->prev;
	else
		ta = a->next;

	assert(a->prev || a->next == nil);
	if(a->prev)
		a->prev->next = a->next;
	if(a->next)
		a->next->prev = a->prev;

	if(ta && v->sel == a) {
		if(ta->floating && ta->next)
			ta = ta->next;
		focus_area(ta);
	}
	write_event("DestroyColumn %d\n", i);

	free(a);
}

void
send_to_area(Area *to, Frame *f) {
	Area *from;

	assert(to->view == f->view);

	from = f->area;

	if(to->floating != from->floating) {
		Rectangle temp = f->revert;
		f->revert = f->r;
		f->r = temp;
	}
	f->client->revert = from;

	detach_from_area(f);
	attach_to_area(to, f, True);
}

void
attach_to_area(Area *a, Frame *f, Bool send) {
	uint h, n_frame;
	Frame *ft;
	Client *c;
	View *v;

	v = a->view;
	c = f->client;
	h = 0;

	f->area = a;

	n_frame = 0;
	for(ft=a->frame; ft; ft=ft->anext)
		n_frame++;
	n_frame = max(n_frame, 1);

	c->floating = a->floating;
	if(!a->floating) {
		f->r = a->r;
		f->r.max.y = f->r.min.y + Dy(a->r) / n_frame;
	}

	insert_frame(a->sel, f, False);

	if(a->floating)
		place_frame(f);

	focus_frame(f, False);
	resize_frame(f, f->r);
	restack_view(a->view);

	if(!a->floating)
		arrange_column(a, False);

	if(a->frame)
		assert(a->sel);
}

void
detach_from_area(Frame *f) {
	Frame *pr;
	Client *c, *cp;
	Area *a;
	View *v;
	Area *ta;
	uint i;

	a = f->area;
	v = a->view;
	c = f->client;

	pr = f->aprev;
	remove_frame(f);

	if(a->sel == f) {
		if(!pr)
			pr = a->frame;
		if(pr && (v->sel == a))
			focus_frame(pr, False);
		else
			a->sel = pr;
	}

	if(!a->floating) {
		if(a->frame)
			arrange_column(a, False);
		else {
			i = 0;
			for(ta=v->area; ta != a; ta=ta->next)
				i++;

			if(v->area->next->next)
				destroy_area(a);
			else if((a->frame == nil) && (v->area->frame))
				focus_area(v->area);

			arrange_view(v);
		}
	}
	else if(!a->frame) {
		if(c->trans) {
			cp = win2client(c->trans);
			if(cp && cp->frame) {
				a = cp->sel->area;
				if(a->view == v)
					focus_area(a);
			}
		}
		else if(v->area->next->frame)
			focus_area(v->area->next);
	}else
		assert(a->sel);
}

static void
bit_set(uint *field, uint width, uint x, uint y, Bool set) {
	enum { devisor = sizeof(uint) * 8 };
	uint bx, mask;

	bx = x / devisor;
	mask = 1 << x % devisor;
	if(set)
		field[y*width + bx] |= mask;
	else
		field[y*width + bx] &= ~mask;
}

static Bool
bit_get(uint *field, uint width, uint x, uint y) {
	enum { devisor = sizeof(uint) * 8 };
	uint bx, mask;

	bx = x / devisor;
	mask = 1 << x % devisor;

	return (field[y*width + bx] & mask) != 0;
}

static void
place_frame(Frame *f) {
	enum { devisor = sizeof(uint) * 8 };
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
	Bool fit;
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
	if(c->fullscreen || c->w.hints->position)
		return;
	if(!field) {
		mx = Dx(screen->r) / dx;
		my = Dy(screen->r) / dy;
		mwidth = ceil((float)mx / devisor);
		field = emallocz(sizeof(uint) * mwidth * my);
	}

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
		p1.x = a->r.min.x + (random() % min(diff, 1));
	}

	if(!fit && (p1.y + Dy(f->r) > a->r.max.y)) {
		diff = Dy(a->r) - Dy(f->r);
		p1.y = a->r.min.y + (random() % min(diff, 1));
	}

	p1 = subpt(p1, f->r.min);
	f->r = rectaddpt(f->r, p1);

	rects = rects_of_view(a->view, &num, nil);
	snap_rect(rects, num, &f->r, &align, snap);
	if(rects)
		free(rects);
}

void
focus_area(Area *a) {
	Frame *f;
	View *v;
	Area *old_a;
	int i;

	v = a->view;
	f = a->sel;
	old_a = v->sel;

	v->sel = a;

	if((old_a) && (a->floating != old_a->floating))
		v->revert = old_a;

	if(v != screen->sel)
		return;

	if(f)
		focus_client(f->client);
	else
		focus_client(nil);

	if(a != old_a) {
		i = 0;
		for(a = v->area; a != v->sel; a = a->next)
			i++;
		if(a->floating)
			write_event("FocusFloating\n");
		else
			write_event("ColumnFocus %d\n", i);
		if(a->frame)
			write_event("ClientFocus 0x%x\n", a->sel->client->w.w);
	}
}

char *
select_area(Area *a, char *arg) {
	static char Ebadvalue[] = "bad value";
	uint i;
	Frame *p, *f;
	Area *ap;
	View *v;

	v = a->view;
	f = a->sel;
	if(!strcmp(arg, "toggle")) {
		if(!a->floating)
			ap = v->area;
		else if(v->revert)
			ap = v->revert;
		else
			ap = v->area->next;
	}
	else if(!strcmp(arg, "left")) {
		if(a->floating)
			return Ebadvalue;
		for(ap=v->area->next; ap->next; ap=ap->next)
			if(ap->next == a) break;
	} 
	else if(!strcmp(arg, "right")) {
		if(a->floating)
			return Ebadvalue;
		ap = a->next;
		if(ap == nil)
			ap = v->area->next;
	}
	else if(!strcmp(arg, "up")) {
		if(!f)
			return Ebadvalue;
		for(p = f->area->frame; p->anext; p = p->anext)
			if(p->anext == f) break;
		goto focus_frame;
	}
	else if(!strcmp(arg, "down")) {
		if(!f)
			return Ebadvalue;
		p = f->anext;
		if(p == nil)
			p = a->frame;
		goto focus_frame;
	}
	else if(!strcmp(arg, "~"))
		ap = v->area;
	else {
		if(sscanf(arg, "%u", &i) != 1 || i == 0)
			return Ebadvalue;
		for(ap=v->area->next; ap; ap=ap->next)
			if(!--i) break;
	}
	focus_area(ap);
	return nil;

focus_frame:
	focus_frame(p, False);
	frame_to_top(p);
	if(v == screen->sel)
		restack_view(v);
	return nil;
}
