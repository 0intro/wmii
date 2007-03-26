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
sel_client_of_area(Area *a) {               
	if(a && a->sel)
		return a->sel->client;
	return nil;
}

Area *
create_area(View *v, Area *pos, uint w) {
	static ushort id = 1;
	uint area_num, col_num, i;
	uint min_width;
	Area *a, **p;

	min_width = screen->rect.width/NCOL;
	p = pos ? &pos->next : &v->area;

	area_num = 0;
	i = 0;
	for(a = v->area; a != *p; a = a->next)
		area_num++, i++;
	for(; a; a = a->next) area_num++;

	col_num = max((area_num - 1), 0);
	if(w == 0) {
		if(col_num) {
			w = newcolw_of_view(v);
			if (w == 0)
				w = screen->rect.width / (col_num + 1);
		}
		else w = screen->rect.width;
	}
	if(w < min_width)
		w = min_width;
	if(col_num && (col_num * min_width + w) > screen->rect.width)
		return nil;
	if(pos)
		scale_view(v, screen->rect.width - w);

	a = emallocz(sizeof(Area));
	a->view = v;
	a->id = id++;
	a->rect = screen->rect;
	a->rect.height = screen->rect.height - screen->brect.height;
	a->mode = def.colmode;
	a->rect.width = w;
	a->frame = nil;
	a->sel = nil;
	a->next = *p;
	*p = a;

	if(a == v->area)
		a->floating = True;
	if((!v->sel) ||
	   (v->sel->floating && v->area->next == a && a->next == nil))
		focus_area(a);

	if(i)
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

	i = 1;
	for(ta=v->area; ta; ta=ta->next)
		if(ta->next == a) break;
		else i++;
	if(ta) {
		ta->next = a->next;
		if(ta->floating && ta->next)
			ta = ta->next;
		if(v->sel == a)
			focus_area(ta);
	}
	if(i) write_event("DestroyColumn %d\n", i);
	free(a);
}

void
send_to_area(Area *to, Frame *f) {
	Area *from;
	assert(to->view == f->view);
	from = f->area;
	if(to->floating != from->floating) {
		XRectangle temp = f->revert;
		f->revert = f->rect;
		f->rect = temp;
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
	if(n_frame == 0)
		n_frame = 1;

	c->floating = a->floating;
	if(!a->floating)
		f->rect.height = a->rect.height / n_frame;

	insert_frame(a->sel, f, False);

	if(a->floating)
		place_frame(f);

	focus_frame(f, False);
	resize_frame(f, &f->rect);
	restack_view(a->view);
	if(!a->floating)
		arrange_column(a, False);
	else
		resize_frame(f, &f->rect);

	update_client_grab(f->client);
	if(a->frame)
		assert(a->sel);
}

void
detach_from_area(Frame *f) {
	Frame *pr;
	Client *c;
	Area *a;
	View *v;
	Area *ta;
	uint i;

	a = f->area;
	v = a->view;
	c = f->client;

	for(pr = a->frame; pr; pr = pr->anext)
		if(pr->anext == f) break;
	remove_frame(f);

	if(a->sel == f) {
		if(!pr)
			pr = a->frame;
		if((a->view->sel == a) && (pr))
			focus_frame(pr, False);
		else
			a->sel = pr;
	}

	if(!a->floating) {
		if(a->frame)
			arrange_column(a, False);
		else {
			i = 0;
			for(ta=v->area; ta && ta != a; ta=ta->next)
				i++;
			if(v->area->next->next)
				destroy_area(a);
			else if(!a->frame && v->area->frame)
				/* focus floating area if it contains something */
				focus_area(v->area);
			arrange_view(v);
		}
	}
	else if(!a->frame) {
		if(c->trans) {
			/* focus area of transient, if possible */
			Client *cl = client_of_win(c->trans);
			if(cl && cl->frame) {
				a = cl->sel->area;
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
bit_twiddle(uint *field, uint width, uint x, uint y, Bool set) {
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
	BlitzAlign align;
	XPoint p1 = {0, 0};
	XPoint p2 = {0, 0};
	XRectangle *rects;
	Frame *fr;
	Client *c;
	Area *a;
	Bool fit;
	uint i, j, x, y, cx, cy, maxx, maxy, diff, num;
	int snap;

	snap = screen->rect.height / 66;
	num = 0;
	fit = False;
	align = CENTER;

	a = f->area;
	c = f->client;

	if(c->trans)
		return;
	if(c->rect.width >= a->rect.width
		|| c->rect.height >= a->rect.height
		|| c->size.flags & USPosition
		|| c->size.flags & PPosition)
		return;
	if(!field) {
		mx = screen->rect.width / dx;
		my = screen->rect.height / dy;
		mwidth = ceil((float)mx / devisor);
		field = emallocz(sizeof(uint) * mwidth * my);
	}
	memset(field, ~0, (sizeof(uint) * mwidth * my));
	for(fr=a->frame; fr; fr=fr->anext) {
		if(fr == f) {
			cx = f->rect.width / dx;
			cy = f->rect.height / dy;
			continue;
		}
		if(fr->rect.x < 0)
			x = 0;
		else
			x = fr->rect.x / dx;
		if(fr->rect.y < 0)
			y = 0;
		else
			y = fr->rect.y / dy;
		maxx = r_east(&fr->rect) / dx;
		maxy = r_south(&fr->rect) / dy;
		for(j = y; j < my && j < maxy; j++)
			for(i = x; i < mx && i < maxx; i++)
				bit_twiddle(field, mwidth, i, j, False);
	}
	for(y = 0; y < my; y++)
		for(x = 0; x < mx; x++) {
			if(bit_get(field, mwidth, x, y)) {
				for(i = x; (i < mx) && bit_get(field, mwidth, i, y); i++);
				for(j = y; (j < my) && bit_get(field, mwidth, x, j); j++);
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
	if(fit && (p1.x + f->rect.width < r_south(&a->rect)))
		f->rect.x = p1.x;
	else {
		diff = a->rect.width - f->rect.width;
		f->rect.x = a->rect.x + (random() % (diff ? diff : 1));
	}
	if(fit && (p1.y + f->rect.height < (r_south(&a->rect))))
		f->rect.y = p1.y;
	else {
		diff = a->rect.height - f->rect.height;
		f->rect.y = a->rect.y + (random() % (diff ? diff : 1));
	}

	rects = rects_of_view(a->view, &num, nil);
	snap_rect(rects, num, &f->rect, &align, snap);
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

	if((old_a)
	&& (a->floating != old_a->floating))
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
		if(i)
			write_event("ColumnFocus %d\n", i);
		else
			write_event("FocusFloating\n");
		if(a->frame)
			write_event("ClientFocus 0x%x\n", a->sel->client->win);
	}
}

char *
select_area(Area *a, char *arg) {
	Area *new;
	uint i;
	Frame *p, *f;
	View *v;
	static char Ebadvalue[] = "bad value";

	v = a->view;
	f = a->sel;
	if(!strncmp(arg, "toggle", 7)) {
		if(!a->floating)
			new = v->area;
		else if(v->revert)
			new = v->revert;
		else
			new = v->area->next;
	} else if(!strncmp(arg, "left", 5)) {
		if(a->floating)
			return Ebadvalue;
		for(new=v->area->next; new->next; new=new->next)
			if(new->next == a) break;
	} else if(!strncmp(arg, "right", 5)) {
		if(a->floating)
			return Ebadvalue;
		new = a->next ? a->next : v->area->next;
	}
	else if(!strncmp(arg, "up", 3)) {
		if(!f)
			return Ebadvalue;
		for(p=a->frame; p->anext; p=p->anext)
			if(p->anext == f) break;
		goto focus_frame;
	}
	else if(!strncmp(arg, "down", 5)) {
		if(!f)
			return Ebadvalue;
		p = f->anext ? f->anext : a->frame;
		goto focus_frame;
	}
	else if(!strncmp(arg, "~", 2)) {
		new = v->area;
	}
	else {
		if(sscanf(arg, "%u", &i) != 1 || i == 0)
			return Ebadvalue;
		for(new=v->area->next; new->next; new=new->next)
			if(!--i) break;
	}
	focus_area(new);
	return nil;

focus_frame:
	focus_frame(p, False);
	frame_to_top(p);
	if(v == screen->sel)
		restack_view(v);
	return nil;
}
