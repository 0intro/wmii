/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Area *
alloc_area(View *v)
{
	static unsigned short id = 1;
	Area *a = cext_emallocz(sizeof(Area));
	a->view = v;
	a->id = id++;
	a->rect = rect;
	a->rect.height = rect.height - brect.height;
	v->area = (Area **)cext_array_attach((void **)v->area, a,
			sizeof(Area *), &v->areasz);
	v->sel = v->narea;
	v->narea++;
	return a;
}

void
destroy_area(Area *a)
{
	unsigned int i;
	View *v = a->view;
	if(a->nframe)
		return;
	if(a->frame)
		free(a->frame);
	if(v->revert == area2index(a))
		v->revert = 0;
	for(i = 0; i < nclient; i++)
		if(client[i]->revert == a)
			client[i]->revert = 0;
	cext_array_detach((void **)v->area, a, &v->areasz);
	v->narea--;
	if(v->sel > 1)
		v->sel--;
	free(a);
}

int
area2index(Area *a)
{
	int i;
	View *v = a->view;
	for(i = 0; i < v->narea; i++)
		if(v->area[i] == a)
			return i;
	return -1;
}

int
aid2index(View *v, unsigned short id)
{
	int i;
	for(i = 0; i < v->narea; i++)
		if(v->area[i]->id == id)
			return i;
	return -1;
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	View *v = a->view;
	int i = area2index(a);

	if(i == -1)
		return;
	if(i)
		v->revert = i;

	if(!strncmp(arg, "toggle", 7)) {
		if(i)
			i = 0;
		else if(v->revert > 0 && v->revert < v->narea)
			i = v->revert;
		else
			i = 1;
	} else if(!strncmp(arg, "prev", 5)) {
		if(i > 0) {
			if(i == 1)
				i = v->narea - 1;
			else
				i--;
		}
	} else if(!strncmp(arg, "next", 5)) {
		if(i > 0) {
			if(i + 1 < v->narea)
				i++;
			else
				i = 1;
		}
	}
	else {
		const char *errstr;
		i = cext_strtonum(arg, 0, v->narea - 1, &errstr);
		if(errstr)
			return;
	}
	new = v->area[i];
	if(new->nframe)
		focus_client(new->frame[new->sel]->client);
	v->sel = i;
	for(i = 0; i < a->nframe; i++)
		draw_client(a->frame[i]->client);

	if(!new->nframe) {
		update_view_label(v);
		draw_bar();
	}
}

void
send2area(Area *to, Area *from, Client *c)
{
	c->revert = from;
	detach_fromarea(from, c);
	attach_toarea(to, c);
	focus_client(c);
}

void
attach_toarea(Area *a, Client *c)
{
	static unsigned short id = 1;
	Frame *f;

	if(clientofview(a->view, c))
		return;
	c->floating = !area2index(a); /* set floating flag */
	f = cext_emallocz(sizeof(Frame));
	f->id = id++;
	f->area = a;
	f->client = c;
	f->rect = c->rect;
	f->rect.width += 2 * def.border;
	f->rect.height += def.border + bar_height();
	c->frame = (Frame **)cext_array_attach(
			(void **)c->frame, f, sizeof(Frame *), &c->framesz);
	c->nframe++;
	c->sel = c->nframe - 1;
	a->frame = (Frame **)cext_array_attach(
			(void **)a->frame, f, sizeof(Frame *), &a->framesz);
	a->nframe++;
	a->sel = a->nframe - 1;
	if(area2index(a)) /* column */
		arrange_column(a);
	else /* floating */
		resize_client(c, &f->rect,  False);
}

void
detach_fromarea(Area *a, Client *c)
{
	Frame *f;
	View *v = a->view;
	int i;

	for(i = 0; i < c->nframe; i++)
		if(c->frame[i]->area == a) {
			f = c->frame[i];
			break;
		}

	cext_array_detach((void **)c->frame, f, &c->framesz);
	cext_array_detach((void **)a->frame, f, &a->framesz);
	free(f);
	c->nframe--;
	if(c->sel > 0)
		c->sel--;
	a->nframe--;
	if(a->sel > 0)
		a->sel--;

	i = area2index(a);
	if(i && a->nframe)
		arrange_column(a);
	else {
		if(i) {
		    if(v->narea > 2)
				destroy_area(a);
			else if(!a->nframe && v->area[0]->nframe)
				v->sel = 0; /* focus floating area if it contains something */
			arrange_view(v, True);
		}
		else if(!i && !a->nframe) {
			if(c->trans) {
				/* focus area of transient, if possible */
				Client *cl = win2client(c->trans);
				if(cl && cl->nframe) {
				   a = cl->frame[cl->sel]->area;
				   if(a->view == v)
					   v->sel = area2index(a);
				}
			}
			else if(v->area[1]->nframe)
				v->sel = 1; /* focus first col as fallback */
		}
	}
}

char *
mode2str(int mode)
{
	switch(mode) {
	case Colequal: return "equal"; break;
	case Colstack: return "stack"; break;
	case Colmax: return "max"; break;
	default: break;
	}
	return nil;
}

int
str2mode(char *arg)
{
	if(!strncmp("equal", arg, 6))
		return Colequal;
	if(!strncmp("stack", arg, 6))
		return Colstack;
	if(!strncmp("max", arg, 4))
		return Colmax;
	return -1;
}

static void
relax_area(Area *a)
{
	unsigned int i, yoff, h, hdiff;
	Bool fallthrough = False;

	if(!a->nframe)
		return;

	switch(a->mode) {
	case Colequal:
		h = a->rect.height;
		h /= a->nframe;
		if(h < 2 * bar_height())
			fallthrough = True;
		break;
	case Colstack:
		yoff = a->rect.y;
		h = a->rect.height - (a->nframe - 1) * bar_height();
		if(h < 3 * bar_height())
			fallthrough = True;
		break;
	default:
		break;
	}

	if(fallthrough) {
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
			f->rect.y = a->rect.y + (a->rect.height - f->rect.height) / 2;
			resize_client(f->client, &f->rect, False);
		}
		return;
	}

	/* some relaxing from potential increment gaps */
	h = 0;
	for(i = 0; i < a->nframe; i++) {
		Frame *f = a->frame[i];
		if(a->mode == Colmax) {
			if(h < f->rect.height)
				h = f->rect.height;
		}
		else
			h += f->rect.height;
	}

	/* try to add rest space to all clients if not COL_STACK mode */
	if(a->mode != Colstack) {
		for(i = 0; (h < a->rect.height) && (i < a->nframe); i++) {
			Frame *f = a->frame[i];
			unsigned int tmp = f->rect.height;
			f->rect.height += (a->rect.height - h);
			resize_client(f->client, &f->rect, True);
			h += (f->rect.height - tmp);
		}
	}

	hdiff = (a->rect.height - h) / a->nframe;
	yoff = a->rect.y + hdiff / 2;
	for(i = 0; i < a->nframe; i++) {
		Frame *f = a->frame[i];
		f->rect.x = a->rect.x + (a->rect.width - f->rect.width) / 2;
		f->rect.y = yoff;
		if(a->mode != Colmax)
			yoff = f->rect.y + f->rect.height + hdiff;
		resize_client(f->client, &f->rect, False);
	}
}

void
arrange_column(Area *a)
{
	unsigned int i, yoff, h;

	if(!a->nframe)
		return;

	switch(a->mode) {
	case Colequal:
		h = a->rect.height;
		h /= a->nframe;
		if(h < 2 * bar_height())
			goto Fallthrough;
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			f->rect.y += i * h;
			if(i + 1 < a->nframe)
				f->rect.height = h;
			else
				f->rect.height =
					a->rect.height - f->rect.y + a->rect.y;
			resize_client(f->client, &f->rect, True);
		}
		break;
	case Colstack:
		yoff = a->rect.y;
		h = a->rect.height - (a->nframe - 1) * bar_height();
		if(h < 3 * bar_height())
			goto Fallthrough;
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			f->rect.y = yoff;
			if(i == a->sel)
				f->rect.height = h;
			else
				f->rect.height = bar_height();
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, True);
		}
		break;
Fallthrough:
	case Colmax:
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			resize_client(f->client, &f->rect, True);
		}
		break;
	default:
		break;
	}

	relax_area(a);
}

static void
match_horiz(Area *a, XRectangle *r)
{
	unsigned int i;

	for(i = 0; i < a->nframe; i++) {
		Frame *f = a->frame[i];
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
	unsigned int min_height = 2 * bar_height();

	for(i = 1; (i < v->narea) && (v->area[i] != a); i++);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
	west = (i > 1) ? v->area[i - 1] : nil;
	east = i + 1 < v->narea ? v->area[i + 1] : nil;

	for(i = 0; (i < a->nframe) && (a->frame[i] != f); i++);
	north = i ? a->frame[i - 1] : nil;
	south = i + 1 < a->nframe ? a->frame[i + 1] : nil;

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
		relax_area(west);
	}
	if(east && (new->x + new->width != a->rect.x + a->rect.width)) {
		east->rect.width -= new->x + new->width - east->rect.x;
		east->rect.x = new->x + new->width;
		a->rect.width = (new->x + new->width) - a->rect.x;
		match_horiz(a, &a->rect);
		match_horiz(east, &east->rect);
		relax_area(east);
	}
AfterHorizontal:

	/* skip vertical resize unless the column is in equal mode */
	if(a->mode != Colequal)
		goto AfterVertical;
	
	/* validate (and trim if necessary) vertical resize */
	if(new->height < min_height) {
		if(f->rect.height < min_height && (new->y == f->rect.y || new->y + new->height == f->rect.y + f->rect.height))
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

	relax_area(a);
}

static void
drop_moving(Frame *f, XRectangle *new, XPoint * pt)
{
	Area *tgt = nil, *src = f->area;
	View *v = src->view;
	unsigned int i;

	if(!pt || src->nframe < 2)
		return;

	for(i = 1; (i < v->narea) &&
			!blitz_ispointinrect(pt->x, pt->y, &v->area[i]->rect); i++);
	if((tgt = ((i < v->narea) ? v->area[i] : nil))) {
		if(tgt != src)
			send2area(tgt, src, f->client);
		else {
			for(i = 0; (i < src->nframe) && !blitz_ispointinrect(
						pt->x, pt->y, &src->frame[i]->rect); i++);
			if((i < src->nframe) && (f != src->frame[i])) {
				unsigned int j = frame2index(f);
				Frame *tmp = src->frame[j];
				src->frame[j] = src->frame[i];
				src->frame[i] = tmp;
				arrange_column(src);
				focus_client(f->client);
			}
		}
	}
}

void
resize_area(Client *c, XRectangle *r, XPoint *pt)
{
	Frame *f = c->frame[c->sel];
	if((f->rect.width == r->width) && (f->rect.height == r->height))
		drop_moving(f, r, pt);
	else
		drop_resize(f, r);
}

Bool
clientofarea(Area *a, Client *c)
{
	unsigned int i;
	for(i = 0; i < a->nframe; i++)
		if(a->frame[i]->client == c)
			return True;
	return False;
}

