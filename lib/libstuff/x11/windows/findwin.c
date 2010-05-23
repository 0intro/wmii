/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Window*
findwin(XWindow w) {
	void **e;
	
	e = map_get(&windowmap, (ulong)w, false);
	if(e)
		return *e;
	return nil;
}
