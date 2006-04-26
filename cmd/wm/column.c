/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

char *
str_of_column_mode(int mode)
{
	switch(mode) {
	case Coldefault: return "default"; break;
	case Colstack: return "stack"; break;
	case Colmax: return "max"; break;
	default: break;
	}
	return nil;
}

int
column_mode_of_str(char *arg)
{
	if(!strncmp("default", arg, 8))
		return Coldefault;
	if(!strncmp("stack", arg, 6))
		return Colstack;
	if(!strncmp("max", arg, 4))
		return Colmax;
	return -1;
}

static void
relax_column(Area *a)
{
	unsigned int i, yoff, h;
	int hdiff;
	Bool fallthrough = False;

	if(!a->frame.size)
		return;

	switch(a->mode) {
	case Coldefault:
		h = a->rect.height;
		h /= a->frame.size;
		if(h < 2 * height_of_bar())
			fallthrough = True;
		break;
	case Colstack:
		h = a->rect.height - (a->frame.size - 1) * height_of_bar();
		if(h < 3 * height_of_bar())
			fallthrough = True;
	default:
		yoff = a->rect.y;
		break;
	}

	if(fallthrough) {
		for(i = 0; i < a->frame.size; i++) {
			Frame *f = a->frame.data[i];
			f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
			f->rect.y = a->rect.y + (a->rect.height - f->rect.height) / 2;
			resize_client(f->client, &f->rect, False);
		}
		return;
	}

	/* some relaxing from potential increment gaps */
	h = 0;
	for(i = 0; i < a->frame.size; i++) {
		Frame *f = a->frame.data[i];
		if(a->mode == Colmax) {
			if(h < f->rect.height)
				h = f->rect.height;
		}
		else
			h += f->rect.height;
	}

	hdiff = a->rect.height - h;
	if((a->mode == Coldefault) && (hdiff > 0)) {
		int hx;
		for(hx = 1; hx < hdiff; hx++)
			for(i = 0; (hx < hdiff) && (i < a->frame.size); i++) {
				Frame *f = a->frame.data[i];
				unsigned int tmp = f->rect.height;
				f->rect.height += hx;
				resize_client(f->client, &f->rect, True);
				hdiff -= (f->rect.height - tmp);
			}
	}

	if(hdiff < 0)
		hdiff = 0;
	hdiff /= a->frame.size;
	yoff = a->rect.y + hdiff / 2;
	for(i = 0; i < a->frame.size; i++) {
		Frame *f = a->frame.data[i];
		f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
		f->rect.y = yoff;
		if(a->mode != Colmax)
			yoff = f->rect.y + f->rect.height + hdiff;
		resize_client(f->client, &f->rect, False);
	}
}

void
scale_column(Area *a, float h)
{
	unsigned int i;
	float scale, dy = 0;

	if(!a->frame.size)
		return;

	for(i = 0; i < a->frame.size; i++)
		dy += a->frame.data[i]->rect.height;
	scale = h / dy;
	for(i = 0; i < a->frame.size; i++) {
		Frame *f = a->frame.data[i];
		f->rect.height *= scale;
	}
}

void
arrange_column(Area *a, Bool dirty)
{
	unsigned int i, yoff = a->rect.y, h;
	unsigned int min_height = 2 * height_of_bar();
	int hdiff;

	if(!a->frame.size)
		return;

	switch(a->mode) {
	case Coldefault:
		h = a->rect.height / a->frame.size;
		if(h < min_height)
			goto Fallthrough;
		if(dirty) {
			for(i = 0; i < a->frame.size; i++)
				a->frame.data[i]->rect.height = h;
		}
		scale_column(a, a->rect.height);
		for(i = 0; i < a->frame.size; i++) {
			Frame *f = a->frame.data[i];
			f->rect.x = a->rect.x;
			f->rect.y = yoff;
			f->rect.width = a->rect.width;
			if(f->rect.height < min_height)
				f->rect.height = min_height;
			else if((hdiff = yoff + f->rect.height - a->rect.height + (a->frame.size - i) * min_height) > 0)
				f->rect.height -= hdiff;
			if(i == a->frame.size - 1)
				f->rect.height = a->rect.height - yoff;
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, True);
		}
		break;
	case Colstack:
		h = a->rect.height - (a->frame.size - 1) * height_of_bar();
		if(h < 3 * height_of_bar())
			goto Fallthrough;
		for(i = 0; i < a->frame.size; i++) {
			Frame *f = a->frame.data[i];
			f->rect = a->rect;
			f->rect.y = yoff;
			if(i == a->sel)
				f->rect.height = h;
			else
				f->rect.height = height_of_bar();
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, True);
		}
		break;
Fallthrough:
	case Colmax:
		for(i = 0; i < a->frame.size; i++) {
			Frame *f = a->frame.data[i];
			f->rect = a->rect;
			resize_client(f->client, &f->rect, True);
		}
		break;
	default:
		break;
	}

	relax_column(a);
}

static void
match_horiz(Area *a, XRectangle *r)
{
	unsigned int i;

	for(i = 0; i < a->frame.size; i++) {
		Frame *f = a->frame.data[i];
		f->rect.x = r->x;
		f->rect.width = r->width;
		resize_client(f->client, &f->rect, False);
	}
}


static void
drop_resize(Frame *f, XRectangle *new)
{
	Area *west = nil, *east = nil, *a = f->area;
	View *v = a->view;
	Frame *north = nil, *south = nil;
	unsigned int i;
	unsigned int min_height = 2 * height_of_bar();

	for(i = 1; (i < v->area.size) && (v->area.data[i] != a); i++);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
	west = (i > 1) ? v->area.data[i - 1] : nil;
	east = i + 1 < v->area.size ? v->area.data[i + 1] : nil;

	for(i = 0; (i < a->frame.size) && (a->frame.data[i] != f); i++);
	north = i ? a->frame.data[i - 1] : nil;
	south = i + 1 < a->frame.size ? a->frame.data[i + 1] : nil;

	/* validate (and trim if necessary) horizontal resize */
	if(new->width < MIN_COLWIDTH) {
		if(new->x + new->width == f->rect.x + f->rect.width)
			new->x = a->rect.x + a->rect.width - MIN_COLWIDTH;
		new->width = MIN_COLWIDTH;
	}
	if(west && (new->x != f->rect.x)) {
		if(new->x < 0 || new->x < (west->rect.x + MIN_COLWIDTH)) {
			new->width -= (west->rect.x + MIN_COLWIDTH) - new->x;
			new->x = west->rect.x + MIN_COLWIDTH;
		}
	} else {
		new->width += new->x - a->rect.x;
		new->x = a->rect.x;
	}
	if(east && (new->x + new->width != f->rect.x + f->rect.width)) {
		if((new->x + new->width) > (east->rect.x + east->rect.width - MIN_COLWIDTH))
			new->width = (east->rect.x + east->rect.width - MIN_COLWIDTH) - new->x;
	} else
		new->width = (a->rect.x + a->rect.width) - new->x;
	if(new->width < MIN_COLWIDTH)
		goto AfterHorizontal;

	/* horizontal resize */
	if(west && (new->x != a->rect.x)) {
		west->rect.width = new->x - west->rect.x;
		a->rect.width += a->rect.x - new->x;
		a->rect.x = new->x;
		match_horiz(a, &a->rect);
		match_horiz(west, &west->rect);
		relax_column(west);
	}
	if(east && (new->x + new->width != a->rect.x + a->rect.width)) {
		east->rect.width -= new->x + new->width - east->rect.x;
		east->rect.x = new->x + new->width;
		a->rect.width = (new->x + new->width) - a->rect.x;
		match_horiz(a, &a->rect);
		match_horiz(east, &east->rect);
		relax_column(east);
	}
AfterHorizontal:

	/* skip vertical resize unless the column is in equal mode */
	if(a->mode != Coldefault)
		goto AfterVertical;
	
	/* validate (and trim if necessary) vertical resize */
	if(new->height < min_height) {
		if(f->rect.height < min_height
			&& (new->y == f->rect.y || new->y + new->height == f->rect.y + f->rect.height))
			goto AfterVertical;
		if(new->y + new->height == f->rect.y + f->rect.height)
			new->y = f->rect.y + f->rect.height - min_height;
		new->height = min_height;
	}
	if(north && (new->y != f->rect.y))
		if(new->y < 0 || new->y < (north->rect.y + min_height)) {
			new->height -= (north->rect.y + min_height) - new->y;
			new->y = north->rect.y + min_height;
		}
	if(south && (new->y + new->height != f->rect.y + f->rect.height)) {
		if((new->y + new->height) > (south->rect.y + south->rect.height - min_height))
			new->height = (south->rect.y + south->rect.height - min_height) - new->y;
	}
	if(new->height < min_height)
		goto AfterVertical;

	/* vertical resize */
	if(north && (new->y != f->rect.y)) {
		north->rect.height = new->y - north->rect.y;
		f->rect.height += f->rect.y - new->y;
		f->rect.y = new->y;
		resize_client(north->client, &north->rect, False);
		resize_client(f->client, &f->rect, False);
	}
	if(south && (new->y + new->height != f->rect.y + f->rect.height)) {
		south->rect.height -= new->y + new->height - south->rect.y;
		south->rect.y = new->y + new->height;
		f->rect.y = new->y;
		f->rect.height = new->height;
		resize_client(f->client, &f->rect, False);
		resize_client(south->client, &south->rect, False);
	}
AfterVertical:

	relax_column(a);
}

static void
drop_moving(Frame *f, XRectangle *new, XPoint *pt)
{
	Area *tgt = nil, *src = f->area;
	View *v = src->view;
	unsigned int i;

	if(!pt)
		return;

	for(i = 1; (i < v->area.size) &&
			!blitz_ispointinrect(pt->x, pt->y, &v->area.data[i]->rect); i++);
	if((tgt = ((i < v->area.size) ? v->area.data[i] : nil))) {
		if(src->frame.size > 1 || src != tgt) {
			int x = new->x + (new->width / 2);
			if(x < 0)
				tgt = new_left_column(v);
			else if(x > rect.width)
				tgt = new_right_column(v);
		}
		if(tgt != src)
			send_to_area(tgt, src, f->client);
		else {
			for(i = 0; (i < src->frame.size) && !blitz_ispointinrect(
						pt->x, pt->y, &src->frame.data[i]->rect); i++);
			if((i < src->frame.size) && (f != src->frame.data[i])) {
				unsigned int j = idx_of_frame(f);
				Frame *tmp = src->frame.data[j];
				src->frame.data[j] = src->frame.data[i];
				src->frame.data[i] = tmp;
				arrange_column(src, False);
				focus_client(f->client, True);
			}
		}
	}
}

void
resize_column(Client *c, XRectangle *r, XPoint *pt)
{
	Frame *f = c->frame.data[c->sel];
	if((f->rect.width == r->width) && (f->rect.height == r->height))
		drop_moving(f, r, pt);
	else
		drop_resize(f, r);
}

Area *
new_left_column(View *v) {
	Area *a, *p, *n;
	unsigned int i;
	if(!(a = p = create_area(v)))
		return nil;
	for(i = 1; i < v->area.size; i++) {
		n = v->area.data[i];
		v->area.data[i] = p;
		p = n;
	}
	arrange_view(v);
	return a;
}

Area *
new_right_column(View *v)
{
	Area *a;
	if(!(a = create_area(v)))
		return nil;
	arrange_view(v);
	return a;
}
