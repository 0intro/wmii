/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Area *
alloc_area(Tag *t)
{
	static unsigned short id = 1;
	Area *a = cext_emallocz(sizeof(Area));
	a->tag = t;
	a->id = id++;
	update_area_geometry(a);
	t->area = (Area **)cext_array_attach((void **)t->area, a, sizeof(Area *), &t->areasz);
	t->sel = t->narea;
	fprintf(stderr, "alloc_area: t->sel == %d\n", t->sel);
	t->narea++;
    return a;
}

void
update_area_geometry(Area *a)
{
	a->rect = rect;
	a->rect.height -= brect.height;
}

void
destroy_area(Area *a)
{
	Tag *t = a->tag;
	if(a->nframe)
		return;
	if(a->frame)
		free(a->frame);
	cext_array_detach((void **)t->area, a, &t->areasz);
	t->narea--;
	if(t->sel == t->narea) {
		if(t->narea)
			t->sel = t->narea - 1;
		else 
			t->sel = 0;
	}
	free(a);
}

int
area2index(Area *a)
{
	int i;
	Tag *t = a->tag;
	for(i = 0; i < t->narea; i++)
		if(t->area[i] == a)
			return i;
	return -1;
}

int
aid2index(Tag *t, unsigned short id)
{
	int i;
	for(i = 0; i < t->narea; i++)
		if(t->area[i]->id == id)
			return i;
	return -1;
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	Tag *t = a->tag;
	int i = area2index(a);
	if(i == -1)
		return;
	if(!strncmp(arg, "prev", 5)) {
		if(i == 1)
			i = t->narea - 1;
		else
			i--;
	} else if(!strncmp(arg, "next", 5)) {
		if(i + 1 < t->narea)
			i++;
		else
			i = 1;
	}
	else {
		const char *errstr;
		i = cext_strtonum(arg, 0, t->narea - 1, &errstr);
		if(errstr)
			return;
	}
	new = t->area[i];
	if(new->nframe)
		focus_client(new->frame[new->sel]->client);
	t->sel = i;
	fprintf(stderr, "select_area: t->sel == %d\n", t->sel);
}

void
send_toarea(Area *to, Client *c)
{
	detach_fromarea(c);
	attach_toarea(to, c);
	focus_client(c);
}

void
attach_toarea(Area *a, Client *c)
{
	static unsigned short id = 1;
	Frame *f = cext_emallocz(sizeof(Frame));

	f->id = id++;
	f->area = a;
	f->client = c;
	f->rect = c->rect;
    f->rect.width += 2 * def.border;
    f->rect.height += def.border + bar_height();
	c->frame = f;
	a->frame = (Frame **)cext_array_attach(
			(void **)a->frame, f, sizeof(Frame *), &a->framesz);
	a->nframe++;
	if(!strstr(c->tags, a->tag->name)) {
		if(c->tags[0] != 0)
			cext_strlcat(c->tags, " ", sizeof(c->tags));
		cext_strlcat(c->tags, a->tag->name, sizeof(c->tags));
	}
	if(area2index(a)) /* column */
		arrange_area(a);
	else /* floating */
		resize_client(c, &f->rect, nil, False);
	update_ctags();
}

void
detach_fromarea(Client *c)
{
	Frame *f = c->frame;
	Area *a = f->area;
	cext_array_detach((void **)a->frame, f, &a->framesz);
	free(f);
	a->nframe--;
	c->frame = nil;
	if(a->nframe) {
		if(a->sel >= a->nframe)
			a->sel = 0;
		arrange_area(a);
	}
	else {
		Tag *t = a->tag;
		if(t->narea > 2)
			destroy_area(a);
		arrange_tag(t, True);
	}
	update_ctags();
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

	if(!a->nframe)
		return;

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
			resize_client(f->client, &f->rect, nil, True);
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
		resize_client(f->client, &f->rect, nil, False);
	}
}

void
arrange_area(Area *a)
{
	unsigned int i, yoff, h;

	if(!a->nframe)
		return;

	switch(a->mode) {
	case Colequal:
		h = a->rect.height;
		h /= a->nframe;
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			f->rect.y += i * h;
			if(i + 1 < a->nframe)
				f->rect.height = h;
			else
				f->rect.height =
					a->rect.height - f->rect.y + a->rect.y;
			resize_client(f->client, &f->rect, nil, True);
		}
		break;
	case Colstack:
		yoff = a->rect.y;
		h = a->rect.height - (a->nframe - 1) * bar_height();
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			f->rect.y = yoff;
			if(i == a->sel)
				f->rect.height = h;
			else
				f->rect.height = bar_height();
			yoff += f->rect.height;
			resize_client(f->client, &f->rect, nil, True);
		}
		break;
	case Colmax:
		for(i = 0; i < a->nframe; i++) {
			Frame *f = a->frame[i];
			f->rect = a->rect;
			resize_client(f->client, &f->rect, nil, True);
		}
		break;
	default:
		break;
	}

	relax_area(a);
}

void
arrange_tag(Tag *t, Bool updategeometry)
{
	unsigned int i;
	unsigned int width;

	if(t->narea == 1)
		return;
	
	width = rect.width / (t->narea - 1);
	for(i = 1; i < t->narea; i++) {
		Area *a = t->area[i];
		if(updategeometry) {
			update_area_geometry(a);
			a->rect.x = (i - 1) * width;
			a->rect.width = width;
		}
		arrange_area(a);
	}
}

static void
match_horiz(Area *a, XRectangle *r)
{
	unsigned int i;

	for(i = 0; i < a->nframe; i++) {
		Frame *f = a->frame[i];
        f->rect.x = r->x;
        f->rect.width = r->width;
        resize_client(f->client, &f->rect, nil, False);
    }
}

static void
drop_resize(Frame *f, XRectangle *new)
{
    Area *west = nil, *east = nil, *a = f->area;
	Tag *t = a->tag;
    Frame *north = nil, *south = nil;
	unsigned int i;

	for(i = 1; (i < t->narea) && (t->area[i] != a); i++);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
    west = (i > 1) ? t->area[i - 1] : nil;
    east = i + 1 < t->narea ? t->area[i + 1] : nil;

	for(i = 1; (i < a->nframe) && (a->frame[i] != f); i++);
    north = i ? a->frame[i - 1] : nil;
    south = i + 1 < a->nframe ? a->frame[i + 1] : nil;

    /* horizontal resize */
    if(west && (new->x != f->rect.x)) {
        west->rect.width = new->x - west->rect.x;
        a->rect.width += f->rect.x - new->x;
        a->rect.x = new->x;
        match_horiz(west, &west->rect);
        match_horiz(a, &a->rect);
		relax_area(west);
    }
    if(east && (new->x + new->width != f->rect.x + f->rect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        a->rect.x = new->x;
        a->rect.width = new->width;
        match_horiz(a, &a->rect);
        match_horiz(east, &east->rect);
		relax_area(east);
    }

    /* vertical resize */
    if(north && (new->y != f->rect.y)) {
        north->rect.height = new->y - north->rect.y;
        f->rect.height += f->rect.y - new->y;
        f->rect.y = new->y;
        resize_client(north->client, &north->rect, nil, False);
        resize_client(f->client, &f->rect, nil, False);
    }
    if(south && (new->y + new->height != f->rect.y + f->rect.height)) {
        south->rect.height -= new->y + new->height - south->rect.y;
        south->rect.y = new->y + new->height;
        f->rect.y = new->y;
        f->rect.height = new->height;
        resize_client(f->client, &f->rect, nil, False);
        resize_client(south->client, &south->rect, nil, False);
    }
	relax_area(a);
}

static void
drop_moving(Frame *f, XRectangle *new, XPoint * pt)
{
    Area *tgt = nil, *src = f->area;
	Tag *t = src->tag;
	unsigned int i;

    if(!pt || src->nframe < 2)
        return;

	for(i = 1; (i < t->narea) &&
			!blitz_ispointinrect(pt->x, pt->y, &t->area[i]->rect); i++);
	if((tgt = ((i < t->narea) ? t->area[i] : nil))) {
        if(tgt != src) {
			send_toarea(tgt, f->client);
			arrange_area(tgt);
		}
        else {
			for(i = 0; (i < src->nframe) && !blitz_ispointinrect(
						pt->x, pt->y, &src->frame[i]->rect); i++);
			if((i < src->nframe) && (f != src->frame[i])) {
				unsigned int j = frame2index(f);
				Frame *tmp = src->frame[j];
				src->frame[j] = src->frame[i];
				src->frame[i] = tmp;
				arrange_area(src);
				focus_client(f->client);
            }
        }
    }
}

void
resize_area(Client *c, XRectangle *r, XPoint *pt)
{
	Frame *f = c->frame;
    if((f->rect.width == r->width)
       && (f->rect.height == r->height))
        drop_moving(f, r, pt);
    else
        drop_resize(f, r);
}
