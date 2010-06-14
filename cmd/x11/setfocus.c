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
	XWindow w;

	ARGBEGIN{
	}ARGEND;

	initdisplay();

	if(!getulong(EARGF(exit(1)), &w))
		exit(1);

	XSetInputFocus(display, w, RevertToParent, CurrentTime);
	XCloseDisplay(display);
}

