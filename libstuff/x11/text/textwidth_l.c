/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

uint
textwidth_l(Font *font, char *text, uint len) {
	Rectangle r;

	r = textextents_l(font, text, len, nil);
	return Dx(r);
}
