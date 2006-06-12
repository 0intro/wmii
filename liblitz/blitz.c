/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "blitz.h"

/* blitz.c */
void
blitz_init(Blitz *blitz, Display *dpy)
{
	blitz->display = dpy;
	blitz->screen = DefaultScreen(dpy);
	blitz->root = DefaultRootWindow(dpy);
}

void
blitz_deinit(Blitz *blitz)
{
}
