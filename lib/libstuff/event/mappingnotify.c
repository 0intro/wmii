/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_mappingnotify(XMappingEvent *ev) {

	/* Why do you need me to tell you this? */
	XRefreshKeyboardMapping(ev);
}
