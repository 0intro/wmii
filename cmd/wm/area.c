/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

#include <cext.h>

/*static Area     zero_area = {0};*/

void 
free_area(Area* a)
{
	ixp_remove_file(ixps, a->files[A_PREFIX]);
	free(a);
}

void
destroy_area(Area *a)
{
	unsigned int i;
	a->layout->deinit(a);
	for(i = 0; a->frames && a->frames[i]; i++);
		destroy_frame(a->frames[i]);
	free(a->frames);
	free_area(a);
}

void 
focus_area(Area *a, int raise, int up, int down)
{
	Page           *p = a->page;
	if (!p)
		return;

	if (down && a->frames)
		focus_frame(a->frames[a->sel], raise, 0, down);
	p->sel = index_item((void **)p->areas, a);
	p->files[P_SEL_AREA]->content = a->files[A_PREFIX]->content;
	if (up)
		focus_page(p, raise, 0);
}

void 
attach_frame_to_area(Area *a, Frame * f)
{	

}

void
detach_frame_from_area(Frame * f, int ignore_focus_and_destroy)
{

}

void
draw_area(Area *a)
{
}

void
hide_area(Area *a)
{
}

void
show_area(Area *a)
{
}
