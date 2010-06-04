/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
pixelvalue(Color c) {
	return ((ulong)(c.alpha&0xff00) << 16)
	     | ((ulong)(c.red&0xff00) << 8)
	     | ((ulong)(c.green&0xff00) << 0)
	     | ((ulong)(c.blue&0xff00) >> 8);
}

bool
parsecolor(const char *name, Color *ret) {

	return XRenderParseColor(display, (char*)(uintptr_t)name, ret);
}
