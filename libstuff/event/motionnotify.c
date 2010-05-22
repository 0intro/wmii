/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_motionnotify(XMotionEvent *ev) {
	Window *w;

	event_xtime = ev->time;
	if((w = findwin(ev->window)))
		handle(w, motion, ev);
}
