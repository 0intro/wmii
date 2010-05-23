#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#define CLIENTEXTERN
#include <string.h>
#include <ixp.h>
#include <stuff/clientutil.h>
#include <stuff/util.h>

static IxpCFid*	ctlfid;
static char 	ctl[1024];
static char*	ectl;

char*
readctl(char *key) {
	char *s, *p;
	int nkey, n;

	if(ctlfid == nil) {
		ctlfid = ixp_open(client, "ctl", OREAD);
		n = ixp_read(ctlfid, ctl, 1023);
		ectl = ctl + n;
		ixp_close(ctlfid);
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
			return strncpy(s, p, n);
		}
	} while((p = strchr(p, '\n')));
	return "";
}

void
client_init(char* address) {
	if(address && *address)
		client = ixp_mount(address);
	else
		client = ixp_nsmount("wmii");
	if(client == nil)
		fatal("can't mount: %r\n");
}

