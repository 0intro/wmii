/* Public Domain --Kris Maglione */
#include "dat.h"
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <string.h>
#include "fns.h"

char*
toutf8n(const char *str, size_t nstr) {
	static iconv_t cd;
	static bool haveiconv;
	char *buf, *pos;
	size_t nbuf, bsize;

	if(cd == nil) {
		cd = iconv_open("UTF-8", nl_langinfo(CODESET));
		if((int)cd == -1)
			warning("Can't convert from local character encoding to UTF-8");
		else
			haveiconv = true;
	}
	if(!haveiconv) {
		buf = emalloc(nstr+1);
		memcpy(buf, str, nstr);
		buf[nstr+1] = '\0';
		return buf;
	}

	iconv(cd, nil, nil, nil, nil);

	bsize = nstr * 1.25 + 4;
	buf = emalloc(bsize);
	pos = buf;
	nbuf = bsize-1;
	while(iconv(cd, (void*)&str, &nstr, &pos, &nbuf) == -1)
		if(errno == E2BIG) {
			bsize *= 1.25 + 4;
			nbuf = pos - buf;
			buf = erealloc(buf, bsize);
			pos = buf + nbuf;
			nbuf = bsize - nbuf - 1;
		}else
			break;
	*pos = '\0';
	return buf;
}

char*
toutf8(const char *str) {
	return toutf8n(str, strlen(str));
}

