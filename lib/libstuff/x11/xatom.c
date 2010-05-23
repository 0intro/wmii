/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

Atom
xatom(char *name) {
	void **e;
	
	e = hash_get(&atommap, name, true);
	if(*e == nil)
		*e = (void*)XInternAtom(display, name, false);
	return (Atom)*e;
}
