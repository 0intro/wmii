/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cext.h>
#include "blitz.h"

void
blitz_draw_tile(BlitzBrush *b)
{
	blitz_drawbg(b->blitz->dpy, b->drawable, b->gc, b->rect,
			b->color, b->border);
}

void
blitz_draw_label(BlitzBrush *b, char *text)
{
	unsigned int x, y, w, h, shortened, len;
	static char buf[2048];
	XGCValues gcv;

	blitz_draw_tile(b);

	if (!text)
		return;

	x = y = shortened = 0;
	w = h = 1;
	cext_strlcpy(buf, text, sizeof(buf));
	len = strlen(buf);
	gcv.foreground = b->color.fg;
	gcv.background = b->color.bg;
	if(b->font->set)
		XChangeGC(b->blitz->dpy, b->gc, GCForeground | GCBackground, &gcv);
	else {
		gcv.font = b->font->xfont->fid;
		XChangeGC(b->blitz->dpy, b->gc, GCForeground | GCBackground | GCFont, &gcv);
	}

	h = b->font->ascent + b->font->descent;
	y = b->rect.y + b->rect.height / 2 - h / 2 + b->font->ascent;

	/* shorten text if necessary */
	while (len && (w = blitz_textwidth(b->font, buf)) > b->rect.width - h) {
		buf[--len] = 0;
		shortened = 1;
	}

	if (w > b->rect.width)
		return;

	/* mark shortened info in the string */
	if (shortened) {
		if (len > 3)
			buf[len - 3] = '.';
		if (len > 2)
			buf[len - 2] = '.';
		if (len > 1)
			buf[len - 1] = '.';
	}
	/* shorten text more if necessary */
	while (len && (w = blitz_textwidth(b->font, buf)) > b->rect.width - h)
		buf[--len] = 0;

	switch (b->align) {
	case EAST:
		x = b->rect.x + b->rect.width - (h / 2 + w);
		break;
	case CENTER:
		x = b->rect.x + h / 2 + (b->rect.width - h - w) / 2;
		break;
	default:
		x = b->rect.x + h / 2;
		break;
	}
	if(b->font->set)
		XmbDrawImageString(b->blitz->dpy, b->drawable, b->font->set, b->gc,
				x, y, buf, len);
	else
		XDrawImageString(b->blitz->dpy, b->drawable, b->gc, x, y, buf, len);
}
