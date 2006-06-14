/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include <cext.h>

#include "blitz.h"

BlitzInput *
blitz_create_input(Drawable drawable, GC gc)
{
	BlitzInput *i = cext_emallocz(sizeof(BlitzInput));
	i->drawable = drawable;
	i->gc = gc;
	blitz_add_widget(BLITZWIDGET(i));
	return i;
}

void
blitz_destroy_input(BlitzInput *i)
{
	blitz_rm_widget(BLITZWIDGET(i));
	free(i);
}
