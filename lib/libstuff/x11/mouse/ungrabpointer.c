/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
ungrabpointer(void) {
	XUngrabPointer(display, CurrentTime);
}
