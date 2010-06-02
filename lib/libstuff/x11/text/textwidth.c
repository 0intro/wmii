/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "../x11.h"

uint
textwidth(Font *font, const char *text) {
	return textwidth_l(font, text, strlen(text));
}
