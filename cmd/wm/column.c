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
	unsigned int frame_size, yoff, h;
	Frame *f;
	int hdiff;
	Bool fallthrough = False;

	if(!a->frame)
		return;

	for(f=a->frame, frame_size=0; f; f=f->anext, frame_size++);

	switch(a->mode) {
	case Coldefault:
		h = a->rect.height / frame_size;
		if(h < 2 * height_of_bar())
			fallthrough = True;
		break;
	case Colstack:
		h = a->rect.height - (frame_size - 1) * height_of_bar();
		if(h < 3 * height_of_bar())
			fallthrough = True;
	default:
		yoff = a->rect.y;
		break;
	}

	if(fallthrough) {
		for(f=a->frame; f; f=f->anext) {
			f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
			f->rect.y = a->rect.y + (a->rect.height - f->rect.height) / 2;
			resize_client(f->client, &f->rect, False);
		}
		return;
	}

	/* some relaxing from potential increment gaps */
	h = 0;
	for(f=a->frame; f; f=f->anext) {
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
			for(f=a->frame; f && (hx < hdiff); f=f->anext) {
				unsigned int tmp = f->rect.height;
				f->rect.height += hx;
				resize_client(f->client, &f->rect, True);
				hdiff -= (f->rect.height - tmp);
			}
	}

	if(hdiff < 0)
		hdiff = 0;
	hdiff /= frame_size;
	yoff = a->rect.y + hdiff / 2;
	for(f=a->frame; f; f=f->anext) {
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
	unsigned int yoff, frame_size = 0;
	Frame *f;
	unsigned int min_height = 2 * height_of_bar();
	float scale, dy = 0;
	int hdiff;

	if(!a->frame)
		return;

	for(f=a->frame; f; f=f->anext, frame_size++)
		dy += f->rect.height;

	scale = h / dy;
	yoff = 0;
	for(f=a->frame; f; f=f->anext) {
		f->rect.height *= scale;
		if(!f->anext)
			f->rect.height = h - yoff;
		yoff += f->rect.height;
	}

	/* min_height can only be respected when there is enough space; the caller should guarantee this */
	if(frame_size * min_height > h)
		return;
	yoff = 0;
	for(f=a->frame; f; f=f->anext, frame_size--) {
		if(f->rect.height < min_height)
			f->rect.height = min_height;
		else if((hdiff = yoff + f->rect.height - h + frame_size * min_height) > 0)
			f->rect.height -= hdiff;
		if(!f->anext)
			f->rect.height = h - yoff;
		yoff += f->rect.height;
	}
}

void
arrange_column(Area *a, Bool dirty)
{
	unsigned int num_frames = 0, yoff = a->rect.y, h;
	Frame *f;
	unsigned int min_height = 2 * height_of_bar();

	if(!a->frame)
		return;

	for(f=a->frame; f; f=f->anext, num_frames++);

	switch(a->mode) {
	case Coldefault:
		h = a->rect.height / num_frames;
		if(h < min_height)
			goto Fallthrough;
		if(dirty) {
			for(f=a->frame; f; f=f->anext)
				f->rect.height = h;
		}
		scale_column(a, a->rect.height);
		for(f=a->frame; f; f=f->anext) {
			f->rect.x = a->rect.x;
			f->rect.y = yoff;
			f->rect.width = a->rect.width;
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, True);
		}
		break;
	case Colstack:
		h = a->rect.height - (num_frames - 1) * height_of_bar();
		if(h < 3 * height_of_bar())
			goto Fallthrough;
		for(f=a->frame; f; f=f->anext) {
			f->rect = a->rect;
			f->rect.y = yoff;
			if(f == a->sel)
				f->rect.height = h;
			else
				f->rect.height = height_of_bar();
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, True);
		}
		break;
Fallthrough:
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->rect = a->rect;
			resize_client(f->client, &f->rect, True);
		}
		break;
	default:
		break;
	}

	relax_column(a);
	flush_masked_events(EnterWindowMask);
}

static void
match_horiz(Area *a, XRectangle *r)
{
	Frame *f;

	for(f=a->frame; f; f=f->anext) {
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
	unsigned int min_height = 2 * height_of_bar();

	for(west=v->area->next; west && west->next != a; west=west->next);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
	east = (west && west->next && west->next->next) ? west->next->next : nil;

	for(north=a->frame; north && north->anext != f; north=north->anext);
	south = (north && north->anext && north->anext->anext) ? north->anext->anext : nil;

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

static Frame *
frame_of_point(XPoint *pt)
{
	Frame *f = nil;
	Area *a;
	View *v = sel;

	if(!v)
		return nil;

	for(a=v->area->next; a && !blitz_ispointinrect(pt->x, pt->y, &a->rect);
		a=a->next);
	if(a)
		for(f=a->frame; f && !blitz_ispointinrect(pt->x, pt->y, &f->rect);
			f=f->anext);
	return f;
}

static void
drop_move(Frame *f, XRectangle *new, XPoint *pt)
{
	Area *tgt = nil, *src = f->area;
	View *v = src->view;
	Frame *ft, *tf;

	if(!pt)
		return;

	for(tgt=v->area->next; tgt && !blitz_ispointinrect(pt->x, pt->y, &tgt->rect);
		tgt=tgt->next);
	if(tgt) {
		if(pt->x < 16) {
			if((src->frame && src->frame->anext) || (src != v->area->next)) {
				tgt = new_column(v, v->area->next, 0);
				send_to_area(tgt, src, f->client);
			}
		}
		else if(pt->x >= rect.width - 16) {
			if((src->frame && src->frame->anext) || src->next) {
				for(tgt=src; tgt->next; tgt=tgt->next);
				tgt = new_column(v, tgt, 0);
				send_to_area(tgt, src, f->client);
			}
		}
		else if(src != tgt) {
			Client *c = f->client;
			Bool before;
			if(!(ft = frame_of_point(pt)) || (f == ft))
				return;
			before = pt->y < (ft->rect.y + ft->rect.height / 2);
			send_to_area(tgt, src, c);

			f = c->sel;
			remove_frame(f);

			if(before)
				insert_frame(tf, f, True);
			else
				insert_frame(ft, f, False);

			tgt->sel = f;
			arrange_column(tgt, False);
		}
		else { /* !tgt */
			if(!(ft = frame_of_point(pt)) || (f == ft))
				return;
			remove_frame(f);

			if(pt->y < (ft->rect.y + ft->rect.height / 2))
				insert_frame(tf, f, True);
			else
				insert_frame(ft, f, False);

			tgt->sel = f;
			arrange_column(tgt, False);
		}
	}
}

void
resize_column(Client *c, XRectangle *r, XPoint *pt)
{
	Frame *f = c->sel;
	if((f->rect.width == r->width) && (f->rect.height == r->height))
		drop_move(f, r, pt);
	else
		drop_resize(f, r);
}

Area *
new_column(View *v, Area *pos, unsigned int w) {
	Area *a = create_area(v, pos, w);
	if(!a)
		return nil;
	arrange_view(v);
	return a;
}
