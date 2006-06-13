/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <cext.h>
#include <stdlib.h>

#include "blitz.h"

void
blitz_add_widget(BlitzWidget **l, BlitzWidget *w)
{
	BlitzWidget **wt;
	for(wt = l; *wt; wt = &(*wt)->next);
	w->next = nil;
	*wt = w;
}

void
blitz_rm_widget(BlitzWidget **l, BlitzWidget *w)
{
	BlitzWidget **wt;
	for(wt = l; *wt && *wt != w; wt = &(*wt)->next);
	cext_assert(*wt == w);
	*wt = w->next;
}
