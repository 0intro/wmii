/* Copyright ©2009-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <ixp.h>
#include <stuff/clientutil.h>
#include <stuff/util.h>
#include <stuff/x.h>
#include <stdio.h>

void
client_readconfig(CTuple *norm, CTuple *focus, Font **font) {

	if(norm)
		loadcolor(norm, readctl("normcolors "), nil);
	if(focus)
		loadcolor(focus, readctl("focuscolors "), nil);
	*font = loadfont(readctl("font "));
	if(!*font)
		fatal("Can't load font %q", readctl("font "));
	sscanf(readctl("fontpad "), "%d %d %d %d",
	       &(*font)->pad.min.x, &(*font)->pad.max.x,
	       &(*font)->pad.min.x, &(*font)->pad.max.y);
}

