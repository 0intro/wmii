/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "wm.h"

Frame *
create_frame(Area *a, Client *c)
{
	static unsigned short id = 1;
	Frame *f = cext_emallocz(sizeof(Frame));
	Frame **fa = a->sel ? &a->sel->anext : &a->frame;
	Frame **fc = c->sel ? &c->sel->cnext : &c->frame;

	f->id = id++;
	f->area = a;
	f->client = c;
	f->rect = c->rect;
	f->rect.width += 2 * def.border;
	f->rect.height += def.border + height_of_bar();
	a->sel = f;
	c->sel = f;

	f->anext = *fa;
	*fa = f;
	f->cnext = *fc;
	*fc = f;

	return f;
}

void
remove_frame(Frame *f)
{
	Area *a = f->area;
	Frame **ft = &a->frame;
	for(; *ft && *ft != f; ft=&(*ft)->anext);
	cext_assert(*ft == f);
	*ft = f->anext;
}

void
insert_frame(Frame *pos, Frame *f, Bool before)
{
	Area *a = f->area;
	if(before) {
		Frame *ft;
		for(ft=a->frame; ft && ft->anext != pos; ft=ft->anext);
		pos=ft;
	}
	Frame **p = pos ? &pos->anext : &a->frame;
	f->anext = *p;
	*p = f;
}

void
destroy_frame(Frame *f)
{
	Client *c = f->client;
	Area *a = f->area;
	Frame **ft, *pr = nil;

	for(ft=&c->frame; *ft && *ft != f; pr = *ft, ft=&(*ft)->cnext);
	cext_assert(*ft == f);
	*ft = f->cnext;
	if(c->sel == f)
		c->sel = pr ? pr : *ft;

	for(ft=&a->frame; *ft && *ft != f; pr = *ft, ft=&(*ft)->anext);
	cext_assert(*ft == f);
	*ft = f->anext;
	if(a->sel == f)
		a->sel = pr ? pr : *ft;

	free(f);
}

Frame *
frame_of_id(Area *a, unsigned short id)
{
	Frame *f;
	for(f=a->frame; f && f->id != id; f=f->anext);
	return f;
}

int
idx_of_frame(Frame *f)
{
	Frame *t;
	int i = 0;
	for(t=f->area->frame; t && t != f; t=t->anext);
	return t ? i : -1;
}

Client *
frame_of_win(Window w)
{
	Client *c;
	for(c=client; c && c->framewin != w; c=c->next);
	return c;
}
