/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
freefont(Font *f) {
	switch(f->type) {
	case FFontSet:
		XFreeFontSet(display, f->font.set);
		break;
	case FXft:
		XftFontClose(display, f->font.xft);
		break;
	case FX11:
		XFreeFont(display, f->font.x11);
		break;
	default:
		break;
	}
	free(f->name);
	free(f);
}
