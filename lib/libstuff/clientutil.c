/* Copyright ©2009-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#define CLIENTEXTERN
#include <stdlib.h>
#include <string.h>
#include <ixp.h>
#include <stuff/clientutil.h>
#include <stuff/util.h>

char*
readctl(char *ctlname, char *key) {
	static char	ctlfile[128];
	static char 	ctl[1024];
	static char*	ectl;
	IxpCFid *fid;
	char *s, *p;
	int nkey, n;

	if(strcmp(ctlname, ctlfile)) {
		strncpy(ctlfile, ctlname, sizeof ctlfile);
		fid = ixp_open(client, ctlfile, OREAD);
		n = ixp_read(fid, ctl, sizeof ctl - 1);
		ectl = ctl + n;
		ixp_close(fid);
	}

	nkey = strlen(key);
	p = ctl - 1;
	do {
		p++;
		if(!strncmp(p, key, nkey)) {
			p += nkey;
			s = strchr(p, '\n');
			n = (s ? s : ectl) - p;
			s = freelater(emalloc(n + 1));
			s[n] = '\0';
			return memcpy(s, p, n);
		}
	} while((p = strchr(p, '\n')));
	return "";
}

void
client_init(char* address) {
	IXP_ASSERT_VERSION;

	if(address == nil)
		address = getenv("WMII_ADDRESS");
	if(address && *address)
		client = ixp_mount(address);
	else
		client = ixp_nsmount("wmii");
	if(client == nil)
		fatal("can't mount wmii filesystem: %r\n");
}
