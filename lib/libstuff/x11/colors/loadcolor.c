/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "../x11.h"

int
loadcolor(CTuple *c, const char *str, const char *end) {
	char buf[128];
	char *toks[4];

	utflcpy(buf, str, end ? min(end - str + 1, sizeof buf) : sizeof buf);
	if(3 > stokenize(toks, nelem(toks), buf, " \t\r\n"))
		return 0;

	if(!(parsecolor(toks[0], &c->fg)
	   && parsecolor(toks[1], &c->bg)
	   && parsecolor(toks[2], &c->border)))
		return 0;

	snprint(c->colstr, sizeof c->colstr, "%s %s %s", toks[0], toks[1], toks[2]);
	return toks[2] + strlen(toks[2]) - buf;
}
