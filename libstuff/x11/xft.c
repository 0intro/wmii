/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/x.h>
#include <stuff/util.h>

Xft	*xft;

#ifdef HAVE_RTLD
#include <dlfcn.h>

bool
havexft(void) {
	void *libxft;

	if(xft == nil) {
		libxft = dlopen("libXft.so", RTLD_LAZY);
		if(libxft == nil)
			return false;
		xft = emalloc(sizeof *xft);
		*(void**)(uintptr_t)&xft->drawcreate   = dlsym(libxft, "XftDrawCreate");
		*(void**)(uintptr_t)&xft->drawdestroy  = dlsym(libxft, "XftDrawDestroy");
		*(void**)(uintptr_t)&xft->fontopen     = dlsym(libxft, "XftFontOpenXlfd");
		*(void**)(uintptr_t)&xft->fontopenname = dlsym(libxft, "XftFontOpenName");
		*(void**)(uintptr_t)&xft->fontclose    = dlsym(libxft, "XftFontClose");
		*(void**)(uintptr_t)&xft->textextents  = dlsym(libxft, "XftTextExtentsUtf8");
		*(void**)(uintptr_t)&xft->drawstring   = dlsym(libxft, "XftDrawStringUtf8");
	}
	return xft && xft->drawcreate && xft->drawdestroy && xft->fontopen
		   && xft->fontopenname && xft->fontclose && xft->textextents && xft->drawstring;
}

#else
bool
havexft(void) {
	return false;
}
#endif

