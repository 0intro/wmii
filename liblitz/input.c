/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>
#include "blitz.h"

BlitzWidget *
blitz_create_input(Drawable drawable, GC gc)
{
	BlitzWidget *i = cext_emallocz(sizeof(BlitzWidget));
	i->drawable = drawable;
	i->gc = gc;
	return i;
}

void
blitz_draw_input(BlitzWidget *i)
{


}

void
blitz_destroy_input(BlitzWidget *i)
{
	free(i);
}
