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
		if((long)cd == -1)
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

	bsize = nstr << 1;
	buf = emalloc(bsize);
	pos = buf;
	nbuf = bsize-1;
	/* The (void*) cast is because, while the BSDs declare:
	 * size_t iconv(iconv_t, const char**, size_t*, char**, size_t*),
	 * GNU/Linux and POSIX declare:
	 * size_t iconv(iconv_t, char**, size_t*, char**, size_t*).
	 * This just happens to be safer than declaring our own
	 * prototype.
	 */
	while(iconv(cd, (void*)&str, &nstr, &pos, &nbuf) == -1)
		if(errno == E2BIG) {
			bsize <<= 1;
			nbuf = pos - buf;
			buf = erealloc(buf, bsize);
			pos = buf + nbuf;
			nbuf = bsize - nbuf - 1;
		}else
			break;
	*pos++ = '\0';
	return erealloc(buf, pos-buf);
}

char*
toutf8(const char *str) {
	return toutf8n(str, strlen(str));
}

