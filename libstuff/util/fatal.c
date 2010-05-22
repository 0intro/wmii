/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <stdlib.h>
#include <fmt.h>
#include "util.h"

typedef struct VFmt VFmt;
struct VFmt {
	const char *fmt;
	va_list args;
};

#ifdef VARARGCK
# pragma varargck type "V" VFmt*
#endif

static int
Vfmt(Fmt *f) {
	VFmt *vf;
	int i;

	vf = va_arg(f->args, VFmt*);
	i = fmtvprint(f, vf->fmt, vf->args);
	return i;
}

void
fatal(const char *fmt, ...) {
	VFmt fp;

	fmtinstall('V', Vfmt);
	fmtinstall('', Vfmt);

	fp.fmt = fmt;
	va_start(fp.args, fmt);
	fprint(2, "%s: fatal: %V\n", argv0, &fp);
	va_end(fp.args);

	exit(1);
}
