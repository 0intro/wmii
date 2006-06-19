/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <cext.h>
#include "blitz.h"

BlitzWidget *
blitz_create_input(Drawable drawable, GC gc, BlitzFont *font)
{
	BlitzWidget *i = cext_emallocz(sizeof(BlitzWidget));
	i->drawable = drawable;
	i->gc = gc;
	i->font = font;
	i->draw = blitz_draw_input;
	return i;
}

void
blitz_draw_input(BlitzWidget *i)
{
	unsigned int x, y, w, h, shortened, len;
	static char text[2048];
	XGCValues gcv;
	blitz_draw_tile(i);


	if (!i->text)
		return;

	x = y = shortened = 0;
	w = h = 1;
	cext_strlcpy(text, i->text, sizeof(text));
	len = strlen(text);
	gcv.foreground = i->color.fg;
	gcv.background = i->color.bg;
	if(i->font->set)
		XChangeGC(__blitz.display, i->gc, GCForeground | GCBackground, &gcv);
	else {
		gcv.font = i->font->xfont->fid;
		XChangeGC(__blitz.display, i->gc, GCForeground | GCBackground | GCFont, &gcv);
	}

	h = i->font->ascent + i->font->descent;
	y = i->rect.y + i->rect.height / 2 - h / 2 + i->font->ascent;

	/* shorten text if necessary */
	while (len && (w = blitz_textwidth(i->font, text)) > i->rect.width) {
		text[len - 1] = 0;
		len--;
		shortened = 1;
	}

	if (w > i->rect.width)
		return;

	/* mark shortened info in the string */
	if (shortened) {
		if (len > 3)
			text[len - 3] = '.';
		if (len > 2)
			text[len - 2] = '.';
		if (len > 1)
			text[len - 1] = '.';
	}
	switch (i->align) {
	case EAST:
		x = i->rect.x + i->rect.width - (h / 2 + w);
		break;
	case CENTER:
		x = i->rect.x + (i->rect.width - w) / 2;
		break;
	default:
		x = i->rect.x + h / 2;
		break;
	}
	if(i->font->set)
		XmbDrawString(__blitz.display, i->drawable, i->font->set, i->gc, x, y, text, len);
	else
		XDrawString(__blitz.display, i->drawable, i->gc, x, y, text, len);
}

void
blitz_destroy_input(BlitzWidget *i)
{
	free(i);
}
