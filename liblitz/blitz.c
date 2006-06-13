/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <cext.h>

#include "blitz.h"

/* blitz.c */
void
blitz_x11_init(Display *dpy)
{
	__blitz.display = dpy;
	__blitz.screen = DefaultScreen(dpy);
	__blitz.root = DefaultRootWindow(dpy);
	__blitz.wins = nil;
}

void
blitz_process_x11_event()
{



}
