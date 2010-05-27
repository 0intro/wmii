/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <limits.h>
#include <string.h>
#include "fns.h"

enum {
	Left, Right, Top, Bottom,
	LeftMin, LeftMax,
	RightMin, RightMax,
	TopMin, TopMax,
	BottomMin, BottomMax,
	Last
};

void
ewmh_setstrut(Window *w, Rectangle struts[4]) {
	long strut[Last];
	int i;

	strut[LeftMin] = struts[Left].min.y;
	strut[Left] = struts[Left].max.x;
	strut[LeftMax] = struts[Left].max.y;

	strut[RightMin] = struts[Right].min.y;
	strut[Right] = -struts[Right].min.x;
	strut[RightMax] = struts[Right].max.y;

	strut[TopMin] = struts[Top].min.x;
	strut[Top] = struts[Top].max.y;
	strut[TopMax] = struts[Top].max.x;

	strut[BottomMin] = struts[Bottom].min.x;
	strut[Bottom] = -struts[Bottom].min.y;
	strut[BottomMax] = struts[Bottom].max.x;

	for(i=0; i<Last; i++)
		if(strut[i] < 0)
			strut[i] = 0;

	changeprop_long(w, Net("WM_STRUT_PARTIAL"), "CARDINAL", strut, nelem(strut));
}

