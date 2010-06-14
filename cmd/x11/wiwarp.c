/* Copyight 2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stuff/x.h>
#include <stuff/util.h>
#include <fmt.h>

int
main(int argc, char *argv[]) {
	Point pt;

	ARGBEGIN{
	}ARGEND;

	initdisplay();

	if(argc) {
		if(!getint(EARGF(exit(1)), &pt.x))
			exit(1);
		if(!getint(EARGF(exit(1)), &pt.y))
			exit(1);
	}else {
		pt = querypointer(&scr.root);
		lprint(1, "%d %d\n", pt.x, pt.y);
	}

	warppointer(pt);
	XCloseDisplay(display);
}

