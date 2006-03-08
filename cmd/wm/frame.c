/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "wm.h"

int
frid2index(Area *a, unsigned short id)
{
	int i;
	for(i = 0; i < a->nframe; i++)
		if(a->frame[i]->id == id)
			return i;
	return -1;
}

int
frame2index(Frame *f)
{
	int i;
	Area *a = f->area;
	for(i = 0; i < a->nframe; i++)
		if(a->frame[i] == f)
			return i;
	return -1;
}

Client *
win2clientframe(Window w)
{
	unsigned int i;
	for(i = 0; (i < clientsz) && client[i]; i++)
		if(client[i]->framewin == w)
			return client[i];
	return nil;
}
