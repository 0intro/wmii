/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Window*
createwindow(Window *parent, Rectangle r, int depth, uint class, WinAttr *wa, int valmask) {
	return createwindow_visual(parent, r, depth, scr.visual, class, wa, valmask);
}
