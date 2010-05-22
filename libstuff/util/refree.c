/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include "util.h"

void
refree(Regex *r) {

	free(r->regex);
	free(r->regc);
	r->regex = nil;
	r->regc = nil;
}
