/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

XWindow
getfocus(void) {
	XWindow ret;
	int revert;

	XGetInputFocus(display, &ret, &revert);
	return ret;
}
