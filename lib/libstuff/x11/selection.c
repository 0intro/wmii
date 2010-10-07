/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

static Handlers handlers;

typedef struct Data	Data;

struct Data {
	long	selection;
	void	(*callback)(void*, char*);
	void*	aux;
};

static bool
_getselection(Window *w, long selection, char *type) {
	XConvertSelection(display, selection, xatom(type),
			  selection, w->xid, CurrentTime);
	return true;
}

void
getselection(char *selection, void (*callback)(void*, char*), void *aux) {
	Window *w;
	Data *d;

	d = emallocz(sizeof *d);
	d->selection = xatom(selection);
	d->callback = callback;
	d->aux = aux;

	w = createwindow(&scr.root, Rect(0, 0, 1, 1), 0, InputOnly, nil, 0);
	w->aux = d;
	sethandler(w, &handlers);

	_getselection(w, d->selection, "UTF8_STRING");
}

static bool
selection_event(Window *w, void *aux, XSelectionEvent *ev) {
	Data *d;
	char **ret;

	d = aux;
	if(ev->property == None && ev->target != xatom("STRING"))
		return _getselection(w, d->selection, "STRING");
	else if(ev->property == None)
		d->callback(d->aux, nil);
	else {
		getprop_textlist(w, atomname(ev->property), &ret);
		delproperty(w, atomname(ev->property));
		d->callback(d->aux, ret ? *ret : nil);
		free(ret);
	}
	destroywindow(w);
	return false;
}

static Handlers handlers = {
	.selection = selection_event,
};

