/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "wm.h"

int
frid2index(Area *a, unsigned short id)
{
	int i;
	for(i = 0; i < a->frame.size; i++)
		if(a->frame.data[i]->id == id)
			return i;
	return -1;
}

int
frame2index(Frame *f)
{
	int i;
	Area *a = f->area;
	for(i = 0; i < a->frame.size; i++)
		if(a->frame.data[i] == f)
			return i;
	return -1;
}

Client *
win2clientframe(Window w)
{
	unsigned int i;
	for(i = 0; (i < client.size) && client.data[i]; i++)
		if(client.data[i]->framewin == w)
			return client.data[i];
	return nil;
}
