/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

XftDraw*
xftdrawable(Image *img) {
	if(img->xft == nil)
		img->xft = xft->drawcreate(display, img->xid, img->visual, img->colormap);
	return img->xft;
}
