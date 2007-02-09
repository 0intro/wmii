/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int
max(int a, int b) {
	if(a > b)
		return a;
	return b;
}

Client *
sel_client_of_area(Area *a) {               
	if(a && a->sel)
		return a->sel->client;
	return nil;
}

Area *
create_area(View *v, Area *pos, unsigned int w) {
	static unsigned short id = 1;
	unsigned int area_num, col_num, i;
	unsigned int min_width;
	Area *ta, *a, **p;

	min_width = screen->rect.width/NCOL;
	p = pos ? &pos->next : &v->area;

	area_num = 0;
	i = 0;
	for(a = v->area; a != *p; a = a->next)
		area_num++, i++;
	for(; a; a = a->next) area_num++;

	col_num = max((area_num - 1), 0);
	if(!w) {
		if(area_num)
			w = screen->rect.width / (col_num + 1);
		else
			w = screen->rect.width;
	}
	if(w < min_width)
		w = min_width;
	if(col_num && (col_num * min_width + w) > screen->rect.width)
		return nil;
	if(i)
		scale_view(v, screen->rect.width - w);

	a = ixp_emallocz(sizeof(Area));
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

	if(i) write_event("CreateColumn %d\n", i);

	i = 0;
	for(ta=v->area; ta != v->sel; ta=ta->next)
		i++;
	if(i) write_event("ColumnFocus %d\n", i);
	else write_event("FocusFloating\n");
	return a;
}

void
destroy_area(Area *a) {
	Client *c;
	Area *ta;
	View *v;
	unsigned int i;

	v = a->view;

	assert(!a->frame && "wmiiwm: fatal, destroying non-empty area");
	if(v->revert == a)
		v->revert = nil;
	for(c=client; c; c=c->next)
		if(c->revert == a)
			c->revert = nil;

	i = 0;
	for(ta=v->area; ta && ta->next != a; ta=ta->next)
		i++;
	if(ta) {
		ta->next = a->next;
		if(v->sel == a) {
			v->sel = ta->floating ? ta->next : ta;
			if(i) write_event("ColumnFocus %d\n", i + 1);
			else write_event("FocusFloating\n");
		}
	}
	free(a);
}

static void
place_client(Area *a, Client *c) {
	static unsigned int mx, my;
	static Bool *field;
	BlitzAlign align;
	XPoint p1 = {0, 0};
	XPoint p2 = {0, 0};
	XRectangle *rects;
	Frame *f, *fr;
	Bool fit;
	unsigned int i, j, x, y, dx, dy, cx, cy, maxx, maxy, diff, num;
	int snap;

	snap = screen->rect.height / 66;
	num = 0;
	fit = False;
	align = CENTER;
	field = nil;

	f = c->sel;

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
		field = ixp_emallocz(my * mx * sizeof(Bool));
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
send_to_area(Area *to, Area *from, Frame *f) {
	assert(to->view == f->view);
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
attach_to_area(Area *a, Frame *f, Bool send) {
	unsigned int h, n_frame;
	Frame *ft;
	Client *c;
	View *v;

	v = a->view;
	c = f->client;
	h = 0;

	n_frame = 1;
	for(ft=a->frame; ft; ft=ft->anext)
		n_frame++;

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
	f->area = a;
	if(a->sel)
		insert_frame(nil, f, False);
	else
		insert_frame(a->sel, f, False);
	a->sel = f;
	if(!c->floating) { /* column */
		f->rect.height = h;
		arrange_column(a, False);
	}else /* floating */
		place_client(a, c);
}

void
detach_from_area(Area *a, Frame *f) {
	Frame *pr;
	Client *c;
	View *v;
	Area *ta;
	unsigned int i;

	v = a->view;
	c = f->client;

	for(pr = a->frame; pr; pr = pr->anext)
		if(pr->anext == f) break;
	remove_frame(f);
	if(a->sel == f)
		a->sel = pr;

	if(!a->floating) {
		if(a->frame)
			arrange_column(a, False);
		else {
			i = 0;
			for(ta=v->area; ta && ta != a; ta=ta->next)
				i++;
			if(v->area->next->next)
				destroy_area(a);
			else if(!a->frame && v->area->frame) {
				/* focus floating area if it contains something */
				focus(v->area->sel->client, False);
				write_event("FocusFloating\n");
			}
			arrange_view(v);
			if(i) write_event("DestroyColumn %d\n", i);
		}
	}
	else if(!a->frame) {
		if(c->trans) {
			/* focus area of transient, if possible */
			Client *cl = client_of_win(c->trans);
			if(cl && cl->frame) {
				a = cl->sel->area;
				if(a->view == v)
					focus(a->sel->client, False);
			}
		}
		else if(v->area->next->frame)
			focus(v->area->next->sel->client, False); /* focus first col as fallback */
		i = 0;
		for(ta=v->area; ta && ta != v->sel; ta=ta->next)
			i++;
		if(i) write_event("ColumnFocus %d\n", i);
		else write_event("FocusFloating\n");
	}
}

char *
select_area(Area *a, char *arg) {
	Area *new, *ta;
	unsigned int i;
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
		a->sel = p;
		frame_to_top(p);
		focus(p->client, True);
		if(v == screen->sel)
			restack_view(v);
		flush_masked_events(EnterWindowMask);
		return nil;
	}
	else if(!strncmp(arg, "down", 5)) {
		if(!f)
			return Ebadvalue;
		p = f->anext ? f->anext : a->frame;
		a->sel = p;
		frame_to_top(p);
		focus(p->client, True);
		if(v == screen->sel)
			restack_view(v);
		flush_masked_events(EnterWindowMask);
		return nil;
	}
	else {
		if(sscanf(arg, "%d", &i) != 1)
			return Ebadvalue;
		for(new=view->area; i && new->next; new=new->next)
			i--;
	}
	if(new->sel)
		focus_client(new->sel->client, True);
	if(v->sel != new) {
		i = 0;
		for(ta=v->area; ta && ta != new; ta=ta->next)
			i++;
		if(i) write_event("ColumnFocus %d\n", i);
		else write_event("FocusFloating\n");
		v->sel = new;
	}
	return nil;
}
