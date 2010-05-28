/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"
#include <assert.h>

Window*
findwin(XWindow xw) {
	Window *w;
	void **e;
	
	e = map_get(&windowmap, (ulong)xw, false);
	if(e) {
		w = *e;
		assert(w->xid == xw);
		return w;
	}
	return nil;
}
