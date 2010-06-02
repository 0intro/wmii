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
ewmh_getstrut(Window *w, Rectangle struts[4]) {
	long *strut;
	ulong n;

	memset(struts, 0, sizeof struts);

	n = getprop_long(w, Net("WM_STRUT_PARTIAL"), "CARDINAL",
		0L, &strut, Last);
	if(n != Last) {
		free(strut);
		n = getprop_long(w, Net("WM_STRUT"), "CARDINAL",
			0L, &strut, 4L);
		if(n != 4) {
			free(strut);
			return;
		}
		strut = erealloc(strut, Last * sizeof *strut);
		strut[LeftMin] = strut[RightMin] = 0;
		strut[LeftMax] = strut[RightMax] = INT_MAX;
		strut[TopMin] = strut[BottomMin] = 0;
		strut[TopMax] = strut[BottomMax] = INT_MAX;
	}
	struts[Left] =   Rect(0,                strut[LeftMin],  strut[Left],      strut[LeftMax]);
	struts[Right] =  Rect(-strut[Right],    strut[RightMin], 0,                strut[RightMax]);
	struts[Top] =    Rect(strut[TopMin],    0,               strut[TopMax],    strut[Top]);
	struts[Bottom] = Rect(strut[BottomMin], -strut[Bottom],  strut[BottomMax], 0);
	free(strut);
}

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

