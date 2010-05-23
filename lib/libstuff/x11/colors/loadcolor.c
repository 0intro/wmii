/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "../x11.h"

bool
loadcolor(CTuple *c, char *str) {
	char buf[24];

	utflcpy(buf, str, sizeof buf);
	memcpy(c->colstr, str, sizeof c->colstr);

	buf[7] = buf[15] = buf[23] = '\0';
	return namedcolor(buf, &c->fg)
	    && namedcolor(buf+8, &c->bg)
	    && namedcolor(buf+16, &c->border);
}
