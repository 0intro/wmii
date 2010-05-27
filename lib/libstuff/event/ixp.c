/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <ixp.h>
#include "event.h"

void
event_preselect(IxpServer *s) {
	USED(s);
	event_check();
}

void
event_fdready(IxpConn *c) {
	USED(c);
	event_check();
}

void
event_fdclosed(IxpConn *c) {

	c->srv->running = false;
}
