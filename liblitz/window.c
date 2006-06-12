/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

void 
blitz_create_win(Blitz *blitz, BlitzWindow *win, unsigned long mask,
				int x, int y, int w, int h)
{
	XSetWindowAttributes wa;
	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = mask;

	win->drawable = XCreateWindow(blitz->display, blitz->root,
			x, y, w, h, 0, DefaultDepth(blitz->display, blitz->screen),
			CopyFromParent, DefaultVisual(blitz->display, blitz->screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	win->gc = XCreateGC(blitz->display, win->drawable, 0, 0);
	win->rect.x = x;
	win->rect.y = y;
	win->rect.width = w;
	win->rect.height = h;
}

void
blitz_resize_win(Blitz *blitz, BlitzWindow *win, int x, int y, int w, int h)
{
	XMoveResizeWindow(blitz->display, win->drawable, x, y, w, h);
}

void
blitz_destroy_win(Blitz *blitz, BlitzWindow *win)
{
	XFreeGC(blitz->display, win->gc);
	XDestroyWindow(blitz->display, win->drawable);
	free(win);
}
