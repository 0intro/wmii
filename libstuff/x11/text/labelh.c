/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

uint
labelh(Font *font) {
	return max(font->height + font->descent + font->pad.min.y + font->pad.max.y, 1);
}
