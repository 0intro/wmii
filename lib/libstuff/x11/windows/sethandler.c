/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

static void
updatemap(Window *w) {
	void **e;

	assert(w->type == WWindow);
	assert((w->prev != nil && w->next != nil) || w->next == w->prev);

	if(w->handler == nil && w->handler_link == nil)
		map_rm(&windowmap, (ulong)w->xid);
	else {
		e = map_get(&windowmap, (ulong)w->xid, true);
		*e = w;
	}
}

Handlers*
sethandler(Window *w, Handlers *new) {
	Handlers *old;

	old = w->handler;
	w->handler = new;

	updatemap(w);
	return old;
}

static HandlersLink*	free_link;

void
pushhandler(Window *w, Handlers *new, void *aux) {
	HandlersLink *l;
	int i;

	if(free_link == nil) {
		l = emalloc(16 * sizeof *l);
		for(i=0; i < 16; i++) {
			l[i].next = free_link;
			free_link = l;
		}
	}
	l = free_link;
	free_link = l->next;

	/* TODO: Maybe: pophandler(w, new); */

	l->next = w->handler_link;
	l->handler = new;
	l->aux = aux;
	w->handler_link = l;

	updatemap(w);
}

bool
pophandler(Window *w, Handlers *old) {
	HandlersLink **lp;
	HandlersLink *l;

	for(lp=&w->handler_link; (l=*lp); lp=&l->next)
		if(l->handler == old) {
			*lp = l->next;
			l->next = free_link;
			free_link = l;
			updatemap(w);
			return true;
		}
	return false;
}

