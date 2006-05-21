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
	a->sel = a->frame.size ? a->sel + 1 : 0;
	cext_vattachat(vector_of_frames(&a->frame), f, a->sel);
	c->sel = c->frame.size - 1;
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
