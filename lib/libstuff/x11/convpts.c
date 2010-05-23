/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

XPoint*
convpts(Point *pt, int np) {
	XPoint *rp;
	int i;
	
	rp = emalloc(np * sizeof *rp);
	for(i = 0; i < np; i++) {
		rp[i].x = pt[i].x;
		rp[i].y = pt[i].y;
	}
	return rp;
}
