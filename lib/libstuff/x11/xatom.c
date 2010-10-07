/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

Atom
xatom(const char *name) {
	void **e, **f;

	e = hash_get(&atommap, name, true);
	if(*e == nil) {
		*e = (void*)XInternAtom(display, name, false);
		f = map_get(&atomnamemap, (ulong)*e, true);
		if(*f == nil)
			*f = (void*)(uintptr_t)name;
	}
	return (Atom)*e;
}

char*
atomname(ulong atom) {
	void **e;

	e = map_get(&atomnamemap, atom, true);
	if(*e == nil) {
		*e = XGetAtomName(display, atom);
		if(*e == nil) {
			map_rm(&atomnamemap, atom);
			return nil;
		}
		*hash_get(&atommap, *e, true) = (void*)atom;
	}
	return *e;
}

