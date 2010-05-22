/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
grabkeyboard(Window *w) {

	return XGrabKeyboard(display, w->xid, true /* owner events */,
		GrabModeAsync, GrabModeAsync, CurrentTime
		) == GrabSuccess;
}
