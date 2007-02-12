/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <stdlib.h>
#include <string.h>

char *
str_of_column_mode(int mode) {
	switch(mode) {
	case Coldefault: return "default"; break;
	case Colstack: return "stack"; break;
	case Colmax: return "max"; break;
	default: break;
	}
	return nil;
}

int
column_mode_of_str(char *arg) {
	if(!strncmp("default", arg, 8))
		return Coldefault;
	if(!strncmp("stack", arg, 6))
		return Colstack;
	if(!strncmp("max", arg, 4))
		return Colmax;
	return -1;
}

static void
relax_column(Area *a) {
	uint frame_size, yoff, h;
	Frame *f;
	int hdiff;
	Bool fallthrough = False;

	if(!a->frame)
		return;
	frame_size = 0;
	for(f=a->frame; f; f=f->anext)
		frame_size++;
	switch(a->mode) {
	case Coldefault:
		h = a->rect.height / frame_size;
		if(h < 2 * labelh(&def.font))
			fallthrough = True;
		break;
	case Colstack:
		h = a->rect.height - (frame_size - 1) * labelh(&def.font);
		if(h < 3 * labelh(&def.font))
			fallthrough = True;
	default:
		yoff = a->rect.y;
		break;
	}
	if(fallthrough) {
		for(f=a->frame; f; f=f->anext) {
			f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
			f->rect.y = a->rect.y + (a->rect.height - f->rect.height) / 2;
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
				uint tmp = f->rect.height;
				f->rect.height += hx;
				hdiff -= (f->rect.height - tmp);
			}
	}
	if(hdiff < 0)
		hdiff = 0;
	hdiff /= frame_size;
	yoff = a->rect.y + hdiff / 2;
	for(f=a->frame; f; f=f->anext) {
		f->rect.y = yoff;
		if(a->mode != Colmax || f == a->sel)
			yoff = r_south(&f->rect) + hdiff;
	}
}

void
scale_column(Area *a, float h) {
	uint yoff, frame_size = 0;
	Frame *f;
	uint min_height = 2 * labelh(&def.font);
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
	for(f=a->frame, frame_size--; f; f=f->anext, frame_size--) {
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
arrange_column(Area *a, Bool dirty) {
	Frame *f;
	uint num_frames = 0, yoff = a->rect.y, h;
	uint min_height = 2 * labelh(&def.font);

	if(a->floating || !a->frame)
		return;
	for(f=a->frame; f; f=f->anext)
		num_frames++;
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
		}
		break;
	case Colstack:
		h = a->rect.height - (num_frames - 1) * labelh(&def.font);
		if(h < 3 * labelh(&def.font))
			goto Fallthrough;
		for(f=a->frame; f; f=f->anext) {
			f->rect = a->rect;
			f->rect.y = yoff;
			if(f == a->sel)
				f->rect.height = h;
			else
				f->rect.height = labelh(&def.font);
			yoff += f->rect.height;
		}
		break;
Fallthrough:
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->rect = a->rect;
		}
		break;
	default:
		break;
	}
	relax_column(a);
	for(f=a->frame; f; f=f->anext)
		resize_client(f->client, &f->rect);
	flush_masked_events(EnterWindowMask);
}

static void
match_horiz(Area *a, XRectangle *r) {
	Frame *f;

	for(f=a->frame; f; f=f->anext) {
		f->rect.x = r->x;
		f->rect.width = r->width;
		resize_frame(f, &f->rect);
	}
}

void
resize_column(Client *c, XRectangle *new) {
	Area *west, *east, *a;
	Frame *north, *south, *f;
	View *v;
	BlitzAlign sticky;
	uint min_height;
	uint min_width;

	f = c->sel;
	a = f->area;
	v = a->view;
	min_height = 2 * labelh(&def.font);
	min_width = screen->rect.width/NCOL;

	for(west=v->area->next; west; west=west->next)
		if(west->next == a) break;
	east = a->next;
	for(north=a->frame; north; north=north->anext)
		if(north->anext == f) break;
	south = f->anext;
	/* validate (and trim if necessary) horizontal resize */
	sticky = get_sticky(&f->rect, new);
	if(new->width < min_width) {
		if(sticky & EAST)
			new->x = r_east(&a->rect) - min_width;
		new->width = min_width;
	}
	if(west && !(sticky & WEST)) {
		if(new->x < 0 || new->x < (west->rect.x + min_width)) {
			new->width -= (west->rect.x + min_width) - new->x;
			new->x = west->rect.x + min_width;
		}
	} else {
		new->width += new->x - a->rect.x;
		new->x = a->rect.x;
	}
	if(east && !(sticky & EAST)) {
		if(r_east(new) > r_east(&east->rect) - min_width)
			new->width = r_east(&east->rect) - min_width - new->x;
	} else
		new->width = r_east(&a->rect) - new->x;
	if(new->width < min_width)
		goto AfterHorizontal;
	/* horizontal resize */
	sticky = get_sticky(&a->rect, new);
	if(west && !(sticky & WEST)) {
		west->rect.width = new->x - west->rect.x;
		a->rect.width += a->rect.x - new->x;
		a->rect.x = new->x;
		match_horiz(a, &a->rect);
		match_horiz(west, &west->rect);
		relax_column(west);
	}
	if(east && !(sticky & EAST)) {
		east->rect.width -= r_east(new) - east->rect.x;
		east->rect.x = r_east(new);
		a->rect.width = r_east(new) - a->rect.x;
		match_horiz(a, &a->rect);
		match_horiz(east, &east->rect);
		relax_column(east);
	}
AfterHorizontal:
	/* skip vertical resize unless the column is in equal mode */
	if(a->mode != Coldefault)
		goto AfterVertical;
	/* validate (and trim if necessary) vertical resize */
	sticky = get_sticky(&f->rect, new);
	if(new->height < min_height) {
		if((f->rect.height < min_height) && sticky & (NORTH|SOUTH))
			goto AfterVertical;
		if(sticky & SOUTH)
			new->y = r_south(&f->rect) - min_height;
		new->height = min_height;
	}
	if(north && !(sticky & NORTH))
		if(new->y < 0 || new->y < (north->rect.y + min_height)) {
			new->height -= (north->rect.y + min_height) - new->y;
			new->y = north->rect.y + min_height;
		}
	if(south && !(sticky & SOUTH)) {
		if(r_south(new) > r_south(&south->rect) - min_height)
			new->height = r_south(&south->rect) - min_height - new->y;
	}
	if(new->height < min_height)
		goto AfterVertical;
	/* vertical resize */
	if(north && !(sticky & NORTH)) {
		north->rect.height = new->y - north->rect.y;
		f->rect.height += f->rect.y - new->y;
		f->rect.y = new->y;
		resize_frame(north, &north->rect);
		resize_frame(f, &f->rect);
	}
	if(south && !(sticky & SOUTH)) {
		south->rect.height -= r_south(new) - south->rect.y;
		south->rect.y = r_south(new);
		f->rect.y = new->y;
		f->rect.height = new->height;
		resize_frame(f, &f->rect);
		resize_frame(south, &south->rect);
	}
AfterVertical:
	relax_column(a);
	focus_view(screen, v);
}

Area *
new_column(View *v, Area *pos, uint w) {
	Area *a = create_area(v, pos, w);
	if(!a)
		return nil;
	arrange_view(v);
	return a;
}
