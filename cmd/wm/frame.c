/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "wm.h"

Vector *
vector_of_frames(FrameVector *fv)
{
	return (Vector *) fv;
}

Frame *
create_frame(Area *a, Client *c)
{
	static unsigned short id = 1;
	Frame *f = cext_emallocz(sizeof(Frame));

	f->id = id++;
	f->area = a;
	f->client = c;
	f->rect = c->rect;
	f->rect.width += 2 * def.border;
	f->rect.height += def.border + height_of_bar();
	cext_vattach(vector_of_frames(&c->frame), f);
	c->sel = c->frame.size - 1;
	cext_vattach(vector_of_frames(&a->frame),f);
	a->sel = a->frame.size - 1;

	return f;
}

void
destroy_frame(Frame *f)
{
	Client *c = f->client;
	Area *a = f->area;

	cext_vdetach(vector_of_frames(&c->frame), f);
	cext_vdetach(vector_of_frames(&a->frame), f);
	free(f);
	if(c->sel > 0)
		c->sel--;
	if(a->sel > 0)
		a->sel--;
}

int
idx_of_frame_id(Area *a, unsigned short id)
{
	int i;
	for(i = 0; i < a->frame.size; i++)
		if(a->frame.data[i]->id == id)
			return i;
	return -1;
}

int
idx_of_frame(Frame *f)
{
	int i;
	Area *a = f->area;
	for(i = 0; i < a->frame.size; i++)
		if(a->frame.data[i] == f)
			return i;
	return -1;
}

Client *
frame_of_win(Window w)
{
	unsigned int i;
	for(i = 0; (i < client.size) && client.data[i]; i++)
		if(client.data[i]->framewin == w)
			return client.data[i];
	return nil;
}

static void
xinsert(FrameVector *fv, Frame *f, unsigned int idx, Bool before)
{
	FrameVector tmp = {0};
	unsigned int i;

	for(i = 0; i < fv->size; i++) {
		if(before && (i == idx))
			cext_vattach(vector_of_frames(&tmp), f);
		cext_vattach(vector_of_frames(&tmp), fv->data[i]);
		if(!before && (i == idx))
			cext_vattach(vector_of_frames(&tmp), f);
	}

	while(fv->size)
		cext_vdetach(vector_of_frames(fv), fv->data[0]);
	for(i = 0; i < tmp.size; i++)
		cext_vattach(vector_of_frames(fv), tmp.data[i]);
	while(tmp.size)
		cext_vdetach(vector_of_frames(&tmp), tmp.data[0]);
}

void
insert_before_idx(FrameVector *fv, Frame *f, unsigned int idx)
{
	xinsert(fv, f, idx, True);
}

void
insert_after_idx(FrameVector *fv, Frame *f, unsigned int idx)
{
	xinsert(fv, f, idx, False);
}
