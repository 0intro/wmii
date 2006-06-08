/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wm.h"

Area *
create_area(View *v, Area *pos, unsigned int w)
{
	static unsigned short id = 1;
	unsigned int area_size;
	Area *a, **p = pos ? &pos->next : &v->area;

	for(area_size = 0, a=v->area; a; a=a->next, area_size++);

	if(!w) {
		if(area_size > 1)
			w = rect.width / area_size - 1;
		else
			w = rect.width;
	}
	if(w < MIN_COLWIDTH)
		w = MIN_COLWIDTH;

	if(area_size >= 2 && (area_size - 1) * MIN_COLWIDTH + w > rect.width)
		return nil;

	if(area_size > 1)
		scale_view(v, rect.width - w);
	a = cext_emallocz(sizeof(Area));
	a->view = v;
	a->id = id++;
	a->rect = rect;
	a->rect.height = rect.height - brect.height;
	a->mode = def.colmode;
	a->rect.width = w;
	a->frame = nil;
	a->sel = nil;

	a->next = *p;
	*p = a;

	v->sel = a;
	return a;
}

void
destroy_area(Area *a)
{
	Client *c;
	Area *t;
	View *v = a->view;
	if(a->frame) {
		fprintf(stderr, "%s", "wmiiwm: fatal, destroying non-empty area\n");
		exit(1);
	}

	if(v->revert == a)
		v->revert = nil;

	for(c=client; c; c=c->next)
		if(c->revert == a)
			c->revert = nil;

	for(t=v->area; t && t->next != a; t=t->next);
	if(t) {
		t->next = a->next;
		if(v->sel == a)
			v->sel = t == v->area ? t->next : t;
	}
	free(a);
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	unsigned int i;
	View *v = a->view;

	v->revert = a;

	if(!strncmp(arg, "toggle", 7)) {
		if(a != v->area)
			new = v->area;
		else if(v->revert && v->revert != v->area)
			new = v->revert;
		else
			new = v->area->next;
	} else if(!strncmp(arg, "prev", 5)) {
		if(a == v->area)
			return;
		for(new=v->area->next;
			new && new->next != a;
			new=new->next);
		if(!new)
			new=v->area->next;
	} else if(!strncmp(arg, "next", 5)) {
		if(a == v->area)
			return;
		new = a->next ? a->next : a;
	}
	else {
		if(sscanf(arg, "%d", &i) != 1)
			return;
		for(new=view->area; i && new->next; new=new->next, i--);
	}
	if(new->sel)
		focus_client(new->sel->client, True);
	v->sel = new;
	draw_clients();
}

void
send_to_area(Area *to, Area *from, Client *c)
{
	c->revert = from;
	detach_from_area(from, c);
	attach_to_area(to, c, True);
	focus_client(c, True);
}

static void
place_client(Area *a, Client *c)
{
	static unsigned int mx, my;
	static Bool *field = nil;
	Frame *fr;
	Bool fit = False;
	BlitzAlign align = CENTER;
	unsigned int i, j, x, y, maxx, maxy, dx, dy, cx, cy, diff, num = 0;
	XPoint p1 = {0, 0}, p2 = {0, 0};
	Frame *f = c->sel;
	int snap = rect.height / 66;
	XRectangle *rects;

	if(c->trans)
		return;
	if(c->rect.width >= a->rect.width
		|| c->rect.height >= a->rect.height
		|| c->size.flags & USPosition
		|| c->size.flags & PPosition)
		return;

	rects = rects_of_view(a->view, &num);
	if(!field) {
		mx = rect.width / 8;
		my = rect.height / 8;
		field = cext_emallocz(my * mx * sizeof(Bool));
	}

	for(y = 0; y < my; y++)
		for(x = 0; x < mx; x++)
			field[y*mx + x] = True;

	dx = rect.width / mx;
	dy = rect.height / my;
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
		maxx = (fr->rect.x + fr->rect.width) / dx;
		maxy = (fr->rect.y + fr->rect.height) / dy;
		for(j = y; j < my && j < maxy; j++)
			for(i = x; i < mx && i < maxx; i++)
				field[j*mx + i] = False;
	}

	for(y = 0; y < my; y++)
		for(x = 0; x < mx; x++) {
			if(field[y*mx + x]) {
				for(i = x; (i < mx) && field[y*mx + i]; i++);
				for(j = y; (j < my) && field[j*mx + x]; j++);
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

	if(fit && (p1.x + f->rect.width < a->rect.x + a->rect.width))
		f->rect.x = p1.x;
	else {
		diff = a->rect.width - f->rect.width;
		f->rect.x = a->rect.x + (random() % (diff ? diff : 1));
	}

	if(fit && (p1.y + f->rect.height < a->rect.y + a->rect.height))
		f->rect.y = p1.y;
	else {
		diff = a->rect.height - f->rect.height;
		f->rect.y = a->rect.y + (random() % (diff ? diff : 1));
	}

	snap_rect(rects, num, &f->rect, &align, snap);
	if(rects)
		free(rects);
}

void
attach_to_area(Area *a, Client *c, Bool send)
{
	View *v = a->view;
	unsigned int h = 0, i;
	Frame *f;
	for(f=a->frame, i=1; f; f=f->anext, i++);

	c->floating = (a == v->area);
	if(!c->floating) {
		h = a->rect.height / i;
		if(a->frame)
			scale_column(a, a->rect.height - h);
	}

	if(!send && !c->floating) { /* column */
		unsigned int w = newcolw_of_view(v);
		if(v->area->next->frame && w) {
			a = new_column(v, a, w);
			arrange_view(v);
		}
	}

	f = create_frame(a, c);

	if(!c->floating) { /* column */
		f->rect.height = h;
		arrange_column(a, False);
	}
	else { /* floating */
		place_client(a, c);
		resize_client(c, &f->rect,  False);
	}
}

void
detach_from_area(Area *a, Client *c)
{
	View *v = a->view;
	Frame *f;

	for(f=c->frame; f && f->area != a; f=f->cnext);
	if(f)
		destroy_frame(f);

	if(a != a->view->area) {
		if(a->frame)
			arrange_column(a, False);
		else {
			if(v->area->next->next)
				destroy_area(a);
			else if(!a->frame && v->area->frame)
				v->sel = v->area; /* focus floating area if it contains something */
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
					v->sel = a;
			}
		}
		else if(v->area->next->frame)
			v->sel = v->area->next; /* focus first col as fallback */
	}
}

Bool
is_of_area(Area *a, Client *c)
{
	Frame *f;
	for(f=a->frame; f; f=f->anext)
		if(f->client == c)
			return True;
	return False;
}

int
idx_of_area(Area *a)
{
	Area *t;
	int i = 0;
	for(t=a->view->area; t && t != a; t=t->next, i++);
	return t ? i : -1;
}

Area *
area_of_id(View *v, unsigned short id)
{
	Area *a;
	for(a=v->area; a && a->id != id; a=a->next);
	return a;
}

Client *        
sel_client_of_area(Area *a)
{               
	return a && a->sel ? a->sel->client : nil;
}
