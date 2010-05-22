/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Rectangle
textextents_l(Font *font, char *text, uint len, int *offset) {
	Rectangle rect;
	XRectangle r;
	XGlyphInfo i;
	int unused;

	if(!offset)
		offset = &unused;

	switch(font->type) {
	case FFontSet:
		*offset = Xutf8TextExtents(font->font.set, text, len, &r, nil);
		return Rect(r.x, -r.y - r.height, r.x + r.width, -r.y);
	case FXft:
		XftTextExtentsUtf8(display, font->font.xft, (uchar*)text, len, &i);
		*offset = i.xOff;
		return Rect(-i.x, i.y - i.height, -i.x + i.width, i.y);
	case FX11:
		rect = ZR;
		rect.max.x = XTextWidth(font->font.x11, text, len);
		rect.max.y = font->ascent;
		*offset = rect.max.x;
		return rect;
	default:
		die("Invalid font type");
		return ZR; /* shut up ken */
	}
}
