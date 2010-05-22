/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

const Point	ZP = {0, 0};
const Rectangle	ZR = {{0, 0}, {0, 0}};

const Window	_pointerwin = { .xid = PointerRoot };
Window*		const pointerwin = (Window*)&_pointerwin;


