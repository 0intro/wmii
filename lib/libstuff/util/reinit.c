/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "util.h"


void
reinit(Regex *r, char *regx) {

	refree(r);

	if(regx[0] != '\0') {
		r->regex = estrdup(regx);
		r->regc = regcomp(regx);
	}
}
