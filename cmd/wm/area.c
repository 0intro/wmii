/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wm.h"

Client *        
sel_client_of_area(Area *a)
{               
	return a && a->sel ? a->sel->client : nil;
}

Area *
create_area(View *v, Area *pos, unsigned int w)
{
	static unsigned short id = 1;
	unsigned int area_size;
	Area *a, **p = pos ? &pos->next : &v->area;

	for(area_size = 0, a=v->area; a; a=a->next, area_size++);

	if(!w) {
		if(area_size > 1)
			w = screen->rect.width / area_size - 1;
		else
			w = screen->rect.width;
	}
	if(w < MIN_COLWIDTH)
		w = MIN_COLWIDTH;

	if(area_size >= 2 && (area_size - 1) * MIN_COLWIDTH + w > screen->rect.width)
		return nil;

	if(area_size > 1)
		scale_view(v, screen->rect.width - w);
	a = cext_emallocz(sizeof(Area));
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

	v->sel = a;
	return a;
}

void
destroy_area(Area *a)
{
	Client *c;
	Area *ta;
	View *v = a->view;
	
	cext_assert(!a->frame && "wmiiwm: fatal, destroying non-empty area");

	if(v->revert == a)
		v->revert = nil;

	for(c=client; c; c=c->next)
		if(c->revert == a)
			c->revert = nil;

	for(ta=v->area; ta && ta->next != a; ta=ta->next);
	if(ta) {
		ta->next = a->next;
		if(v->sel == a)
			v->sel = ta->floating ? ta->next : ta;
	}
	free(a);
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
	int snap = screen->rect.height / 66;
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
		mx = screen->rect.width / 8;
		my = screen->rect.height / 8;
		field = cext_emallocz(my * mx * sizeof(Bool));
	}

	for(y = 0; y < my; y++)
		for(x = 0; x < mx; x++)
			field[y*mx + x] = True;

	dx = screen->rect.width / mx;
	dy = screen->rect.height / my;
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
send_to_area(Area *to, Area *from, Frame *f)
{
	cext_assert(to->view == f->view);

	if(to->floating != from->floating) {
		XRectangle temp = f->revert;
		f->revert = f->rect;
		f->rect = temp;
	}
	f->client->revert = from;
	detach_from_area(from, f);
	attach_to_area(to, f, True);
	focus_client(f->client, True);
}

void
attach_to_area(Area *a, Frame *f, Bool send)
{
	unsigned int h, n_frame;
	Frame **fa, *ft;
	View *v = a->view;
	Client *c = f->client;

	for(ft=a->frame, n_frame=1; ft; ft=ft->anext, n_frame++);

	h = 0;
	c->floating = a->floating;
	if(!c->floating) {
		h = a->rect.height / n_frame;
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

	fa = a->sel ? &a->sel->anext : &a->frame;
	f->anext = *fa;
	*fa = f;

	f->area = a;
	a->sel = f;

	if(!c->floating) { /* column */
		f->rect.height = h;
		arrange_column(a, False);
	}
	else { /* floating */
		place_client(a, c);
		resize_client(c, &f->rect,  True);
	}
}

void
detach_from_area(Area *a, Frame *f)
{
	Frame **ft, *pr = nil;
	Client *c = f->client;
	View *v = a->view;

	for(ft=&a->frame; *ft; ft=&(*ft)->anext) {
		if(*ft == f) break;
		pr = *ft;
	}
	cext_assert(*ft == f);
	*ft = f->anext;

	if(a->sel == f)
		a->sel = pr ? pr : *ft;

	if(!a->floating) {
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

char *
select_area(Area *a, char *arg)
{
	Area *new;
	unsigned int i;
	Frame *p, *f;
	View *v;
	static char Ebadvalue[] = "bad value";

	v = a->view;
	f = a->sel;

	if(!strncmp(arg, "toggle", 7)) {
		if(a != v->area)
			new = v->area;
		else if(v->revert)
			new = v->revert;
		else
			new = v->area->next;
	} else if(!strncmp(arg, "left", 5)) {
		if(a->floating)
			return Ebadvalue;
		for(new=v->area->next;
			new && new->next != a;
			new=new->next);
		if(!new)
			new=v->area->next;
	} else if(!strncmp(arg, "right", 5)) {
		if(a->floating)
			return Ebadvalue;
		new = a->next ? a->next : a;
	}
	else if(!strncmp(arg, "up", 3)) {
		if(!f)
			return Ebadvalue;
		for(p=a->frame; p->anext; p=p->anext)
			if(p->anext == f) break;
		a->sel = p;
		arrange_column(a, False);
		if(v == screen->sel)
			focus_view(screen, v);
		flush_masked_events(EnterWindowMask);
		return nil;
	}
	else if(!strncmp(arg, "down", 5)) {
		if(!f)
			return Ebadvalue;
		p = f->anext ? f->anext : a->frame;
		a->sel = p;
		arrange_column(a, False);
		if(v == screen->sel)
			focus_view(screen, v);
		flush_masked_events(EnterWindowMask);
		return nil;
	}
	else {
		if(sscanf(arg, "%d", &i) != 1)
			return Ebadvalue;
		for(new=view->area; i && new->next; new=new->next, i--);
	}
	if(new->sel)
		focus_client(new->sel->client, True);
	v->sel = new;
	if(a->floating != new->floating)
		v->revert = a;
	draw_frames();
	return nil;
}
