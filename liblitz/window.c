/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

BlitzWindow *
blitz_create_win(unsigned long mask, int x, int y, int w, int h)
{
	BlitzWindow *win = cext_emallocz(sizeof(BlitzWindow));
	XSetWindowAttributes wa;
	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = mask;

	win->drawable = XCreateWindow(__blitz.display, __blitz.root,
			x, y, w, h, 0, DefaultDepth(__blitz.display, __blitz.screen),
			CopyFromParent, DefaultVisual(__blitz.display, __blitz.screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	win->gc = XCreateGC(__blitz.display, win->drawable, 0, 0);
	win->rect.x = x;
	win->rect.y = y;
	win->rect.width = w;
	win->rect.height = h;
	return win;
}

void
blitz_resize_win(BlitzWindow *win, int x, int y, int w, int h)
{
	XMoveResizeWindow(__blitz.display, win->drawable, x, y, w, h);
}

void
blitz_destroy_win(BlitzWindow *win)
{
	XFreeGC(__blitz.display, win->gc);
	XDestroyWindow(__blitz.display, win->drawable);
	free(win);
}
