/* Copyright ©2007-2010 Kris Maglione <jg@suckless.org>
 * See LICENSE file for license details.
 */

#include "dat.h"
#include "fns.h"

static jmp_buf	errjmp[16];
static long	nerror;

void
error(char *fmt, ...) {
	char errbuf[IXP_ERRMAX];
	va_list ap;

	va_start(ap, fmt);
	vsnprint(errbuf, IXP_ERRMAX, fmt, ap);
	va_end(ap);
	ixp_errstr(errbuf, IXP_ERRMAX);

	nexterror();
}

void
nexterror(void) {
	assert(nerror > 0);
	longjmp(errjmp[--nerror], 1);
}

void
poperror(void) {
	assert(nerror > 0);
	--nerror;
}

jmp_buf*
pusherror(void) {
	assert(nerror < nelem(errjmp));
	return &errjmp[nerror++];
}

