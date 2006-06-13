/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

BlitzWin *
blitz_create_win(unsigned long mask, int x, int y, int w, int h)
{
	BlitzWin *win = cext_emallocz(sizeof(BlitzWin));
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

	win->next = __blitz.wins;
	__blitz.wins = win;

	return win;
}

void
blitz_resize_win(BlitzWin *win, int x, int y, int w, int h)
{
	XMoveResizeWindow(__blitz.display, win->drawable, x, y, w, h);
}

void
blitz_destroy_win(BlitzWin *win)
{
	BlitzWin **w;
	for(w = &__blitz.wins; *w && *w != win; w = &(*w)->next);
	cext_assert(*w != win);
	*w = win->next;
	XFreeGC(__blitz.display, win->gc);
	XDestroyWindow(__blitz.display, win->drawable);
	free(win);
}
