/* ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "wmii.h"

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
scale_column(Area *a) {
	Frame *f, **fp;
	uint min_height, yoff, dy;
	uint num_col, num_uncol;
	uint col_h, uncol_h;
	int surplus, i, j;

	if(!a->frame)
		return;

	/* This works by comparing heights based on a surplus of their
	 * minimum size. We start by subtracting the minimum size, then
	 * scale the surplus, and add back the minimum size later. This
	 * is based on the size of the client, rather than the frame, so
	 * increment gaps can be equalized later */
	/* Frames that can't be accomodated are pushed to the floating layer */

	min_height = labelh(&def.font);
	col_h = labelh(&def.font);
	uncol_h = min_height + frame_delta_h();

	num_col = 0;
	num_uncol = 0;
	dy = 0;
	for(f=a->frame; f; f=f->anext)
		if(f->collapsed)
			num_col++;
		else
			num_uncol++;

	surplus = a->rect.height;
	surplus -= num_col * col_h;
	surplus -= num_uncol * uncol_h;
	if(surplus < 0) {
		i = ceil((float)(-surplus)/(uncol_h - col_h));
		if(i >= num_uncol)
			i = num_uncol - 1;
		num_uncol -= i;
		num_col += i;
		surplus += i * (uncol_h - col_h);
	}
	if(surplus < 0) {
		i = ceil((float)(-surplus)/col_h);
		if(i > num_col)
			i = num_col;
		num_col -= i;
		surplus += i * col_h;
	}

	i = num_col - 1;
	j = num_uncol - 1;
	for(f=a->frame; f; f=f->anext) {
		if(f == a->sel)
			j++;
		if(!f->collapsed) {
			if(j < 0 && f != a->sel)
				f->collapsed = True;
			else {
				if(f->crect.height <= min_height)
					f->crect.height = 1;
				else
					f->crect.height -= min_height;
				dy += f->crect.height;
			}
			j--;
		}
	}
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f == a->sel)
			i++;
		if(f->collapsed) {
			if(i < 0 && f != a->sel) {
				f->collapsed = False;
				send_to_area(f->view->area, f);
				continue;
			}
			i--;
		}
		fp=&f->anext;
	}

	i = num_uncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect.x = a->rect.x;
		f->rect.width = a->rect.width;
		if(!f->collapsed) {
			i--;
			f->rect.height = (float)f->crect.height / dy * surplus;
			if(!i)
				f->rect.height = surplus;
			f->rect.height += min_height + frame_delta_h();
			apply_sizehints(f->client, &f->rect, False, True, NWEST);

			dy -= f->crect.height;
			surplus -= f->rect.height - frame_delta_h() - min_height;
		}else
			f->rect.height = labelh(&def.font);
	}

	yoff = a->rect.y;
	i = num_uncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect.y = yoff;
		f->rect.x = a->rect.x;
		f->rect.width = a->rect.width;
		if(f->collapsed)
			yoff += f->rect.height;
		else{
			i--;
			f->rect.height += surplus / num_uncol;
			if(!i)
				f->rect.height += surplus % num_uncol;
			yoff += f->rect.height;
		}
	}
}

void
arrange_column(Area *a, Bool dirty) {
	Frame *f;

	if(a->floating || !a->frame)
		return;

	switch(a->mode) {
	case Coldefault:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			if(dirty)
				f->crect.height = 100;
		}
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			f->rect = a->rect;
		}
		goto resize;
	default:
		break;
	}
	scale_column(a);
resize:
	if(a->view == screen->sel) {
		restack_view(a->view);
		resize_client(a->sel->client, &a->sel->rect);
		for(f=a->frame; f; f=f->anext)
			if(!f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
		for(f=a->frame; f; f=f->anext)
			if(f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
	}
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
		//relax_column(west);
	}
	if(east && !(sticky & EAST)) {
		east->rect.width -= r_east(new) - east->rect.x;
		east->rect.x = r_east(new);
		a->rect.width = r_east(new) - a->rect.x;
		match_horiz(a, &a->rect);
		match_horiz(east, &east->rect);
		//relax_column(east);
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
	arrange_column(a, False);
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
