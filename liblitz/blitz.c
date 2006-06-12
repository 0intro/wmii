/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

/* blitz.c */
Blitz *
init_blitz(Display *dpy)
{
	Blitz *b = cext_emallocz(sizeof(Blitz));
	b->display = dpy;
	b->screen = DefaultScreen(dpy);
	b->root = DefaultRootWindow(dpy);
	return b;
}

void
deinit_blitz(Blitz *blitz)
{
	free(blitz);
}
