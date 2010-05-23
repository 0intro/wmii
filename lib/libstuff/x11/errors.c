/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

extern ErrorCode ignored_xerrors[];
static bool	_trap_errors;
static long	nerrors;

int
errorhandler(Display *dpy, XErrorEvent *error) {
	ErrorCode *e;

	USED(dpy);

	if(_trap_errors)
		nerrors++;

	e = ignored_xerrors;
	if(e)
	for(; e->rcode || e->ecode; e++)
		if((e->rcode == 0 || e->rcode == error->request_code)
		&& (e->ecode == 0 || e->ecode == error->error_code))
			return 0;

	fprint(2, "%s: fatal error: Xrequest code=%d, Xerror code=%d\n",
			argv0, error->request_code, error->error_code);
	return xlib_errorhandler(display, error); /* calls exit() */
}

int
traperrors(bool enable) {
	
	sync();
	_trap_errors = enable;
	if (enable)
		nerrors = 0;
	return nerrors;
	
}
