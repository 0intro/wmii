/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

#include <cext.h>

static Area     zero_area = {0};

void 
free_area(Area* a)
{
	ixp_remove_file(ixps, a->files[A_PREFIX]);
	free(f);
}

void
destroy_area(Area *a)
{
	unsigned int i;
	a->layout->deinit(a);
	for(i = 0; a->frames && a->frames[i]; i++);
		destroy_frame(i);
	free_area(a);
}

void 
focus_area(Area *a, int raise, int up, int down)
{
	Page           *p = f->page;
	Frame          *old;
	if (!p)
		return;

	old = get_selected(p);
	if (down && f->clients)
		focus_client(f->clients[f->sel], raise, 0);

	if (is_managed_frame(f)) {
		p->managed_stack = (Frame **)
			attach_item_begin(detach_item
					  ((void **) p->managed_stack, f,
				      sizeof(Frame *)), f, sizeof(Frame *));
		p->files[P_MANAGED_SELECTED]->content =
			f->files[F_PREFIX]->content;
		p->files[P_MODE]->content = p->files[P_MANAGED_PREFIX]->content;
	} else {
		p->floating_stack = (Frame **)
			attach_item_begin(detach_item
					  ((void **) p->floating_stack, f,
				      sizeof(Frame *)), f, sizeof(Frame *));
		p->files[P_FLOATING_SELECTED]->content =
			f->files[F_PREFIX]->content;
		p->files[P_MODE]->content = p->files[P_FLOATING_PREFIX]->content;
		if (raise)
			XRaiseWindow(dpy, f->win);
	}
	if (up)
		focus_page(p, raise, 0);
}

