/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Handlers*
sethandler(Window *w, Handlers *new) {
	Handlers *old;
	void **e;

	assert(w->type == WWindow);
	assert((w->prev != nil && w->next != nil) || w->next == w->prev);

	if(new == nil)
		map_rm(&windowmap, (ulong)w->xid);
	else {
		e = map_get(&windowmap, (ulong)w->xid, true);
		*e = w;
	}
	old = w->handler;
	w->handler = new;
	return old;
}
