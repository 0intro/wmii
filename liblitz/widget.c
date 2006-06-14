/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include <cext.h>

#include "blitz.h"

void
blitz_add_widget(BlitzWidget *w)
{
	BlitzWidget **wp;
	for(wp = &__blitz.widgets; *wp; wp = &(*wp)->next);
	w->next = nil;
	*wp = w;
}

void
blitz_rm_widget(BlitzWidget *w)
{
	BlitzWidget **wp;
	for(wp = &__blitz.widgets; *wp && *wp != w; wp = &(*wp)->next);
	cext_assert(*wp == w);
	*wp = w->next;
}
