/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Vector *
vector_of_areas(AreaVector *av)
{
	return (Vector *) av;
}

Area *
create_area(View *v, unsigned int pos)
{
	static unsigned short id = 1;
	unsigned int w;
	Area *a = nil;

	if(v->area.size > 1)
		w = rect.width / v->area.size - 1;
	else
		w = rect.width;
	if(w < MIN_COLWIDTH)
		w = MIN_COLWIDTH;

	if(v->area.size >= 2 && (v->area.size - 1) * MIN_COLWIDTH + w > rect.width)
		return nil;

	if(v->area.size > 1)
		scale_view(v, rect.width - w);
	a = cext_emallocz(sizeof(Area));
	a->view = v;
	a->id = id++;
	a->rect = rect;
	a->rect.height = rect.height - brect.height;
	a->mode = def.colmode;
	a->rect.width = w;
	cext_vattachat(vector_of_areas(&v->area), a, pos);
	v->sel = pos;
	return a;
}

void
destroy_area(Area *a)
{
	unsigned int i;
	View *v = a->view;
	if(a->frame.size) {
		fprintf(stderr, "%s", "wmiiwm: fatal, destroying non-empty area\n");
		exit(1);
	}
	if(a->frame.data)
		free(a->frame.data);
	if(v->revert == idx_of_area(a))
		v->revert = 0;
	for(i = 0; i < client.size; i++)
		if(client.data[i]->revert == a)
			client.data[i]->revert = 0;
	cext_vdetach(vector_of_areas(&v->area), a);
	if(v->sel > 1)
		v->sel--;
	free(a);
}

int
idx_of_area(Area *a)
{
	int i;
	View *v = a->view;
	for(i = 0; i < v->area.size; i++)
		if(v->area.data[i] == a)
			return i;
	return -1;
}

int
idx_of_area_id(View *v, unsigned short id)
{
	int i;
	for(i = 0; i < v->area.size; i++)
		if(v->area.data[i]->id == id)
			return i;
	return -1;
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	View *v = a->view;
	int i = idx_of_area(a);

	if(i == -1)
		return;
	if(i)
		v->revert = i;

	if(!strncmp(arg, "toggle", 7)) {
		if(i)
			i = 0;
		else if(v->revert > 0 && v->revert < v->area.size)
			i = v->revert;
		else
			i = 1;
	} else if(!strncmp(arg, "prev", 5)) {
		if(i <= 1)
			return;
		else
			i--;
	} else if(!strncmp(arg, "next", 5)) {
		if(i > 0 && (i + 1 < v->area.size))
			i++;
		else
			return;
	}
	else {
		if(sscanf(arg, "%d", &i) != 1)
			return;
	}
	new = v->area.data[i];
	if(new->frame.size)
		focus_client(new->frame.data[new->sel]->client, True);
	v->sel = i;
	draw_clients();
}

void
send_to_area(Area *to, Area *from, Client *c)
{
	c->revert = from;
	detach_from_area(from, c);
	attach_to_area(to, c);
	focus_client(c, True);
}

static void
place_client(Area *a, Client *c)
{
	static unsigned int mx, my;
	static BlitzAlign align = CENTER;
	static Bool *field = nil;
	Bool fit = False;
	unsigned int i, j, k, x, y, maxx, maxy, dx, dy, cx, cy, diff, num = 0;
	XPoint p1 = {0, 0}, p2 = {0, 0};
	Frame *f = c->frame.data[c->sel];
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
	for(k = 0; k < a->frame.size; k++) {
		Frame *fr = a->frame.data[k];
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
attach_to_area(Area *a, Client *c)
{
	View *v = a->view;
	unsigned int h = 0, aidx = idx_of_area(a);
	Frame *f;

	c->floating = !aidx;
	if(aidx) {
		h = a->rect.height / (a->frame.size + 1);
		if(a->frame.size)
			scale_column(a, a->rect.height - h);
	}

	if(aidx) { /* column */
		unsigned int nc = ncol_of_view(v);
		if(v->area.data[1]->frame.size && nc && nc > v->area.size - 1) {
			a = create_area(v, ++v->sel);
			arrange_view(v);
		}
	}

	f = create_frame(a, c);

	if(aidx) { /* column */
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
	int i;

	for(i = 0; i < c->frame.size; i++)
		if(c->frame.data[i]->area == a) {
			destroy_frame(c->frame.data[i]);
			break;
		}

	i = idx_of_area(a);
	if(i && a->frame.size)
		arrange_column(a, False);
	else {
		if(i) {
		    if(v->area.size > 2)
				destroy_area(a);
			else if(!a->frame.size && v->area.data[0]->frame.size)
				v->sel = 0; /* focus floating area if it contains something */
			arrange_view(v);
		}
		else if(!i && !a->frame.size) {
			if(c->trans) {
				/* focus area of transient, if possible */
				Client *cl = client_of_win(c->trans);
				if(cl && cl->frame.size) {
				   a = cl->frame.data[cl->sel]->area;
				   if(a->view == v)
					   v->sel = idx_of_area(a);
				}
			}
			else if(v->area.data[1]->frame.size)
				v->sel = 1; /* focus first col as fallback */
		}
	}
}

Bool
is_of_area(Area *a, Client *c)
{
	unsigned int i;
	for(i = 0; i < a->frame.size; i++)
		if(a->frame.data[i]->client == c)
			return True;
	return False;
}

Client *        
sel_client_of_area(Area *a)
{               
	if(a) {
		return (a->frame.size) ? a->frame.data[a->sel]->client : nil;
	}
	return nil;
}
