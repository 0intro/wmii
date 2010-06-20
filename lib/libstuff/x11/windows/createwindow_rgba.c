/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Window*
createwindow_rgba(Window *parent, Rectangle r, WinAttr *wa, int valmask) {
	WinAttr attr;

	if(scr.visual32 == nil)
		return createwindow(parent, r, scr.depth, InputOutput, wa, valmask);

	attr = wa ? *wa : (WinAttr){0};
	valmask |= CWBorderPixel | CWColormap;
	attr.border_pixel = 0;
	attr.colormap = scr.colormap32;
	return createwindow_visual(parent, r, 32, scr.visual32, InputOutput, &attr, valmask);
}
