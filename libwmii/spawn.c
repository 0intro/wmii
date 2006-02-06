/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wmii.h"

static void
spawn_ixp_write(char *cmd)
{
	IXPClient c;
	char *file = cmd;
	char *data = strchr(cmd, ' ');
	char *address = strdup(getenv("WMII_ADDRESS"));
    unsigned int fid = c.root_fid << 2;
	size_t len;

	if(!data || !cmd || !address)
		return;
	*data = 0;
	data++;
	len = strlen(data);
	fprintf(stderr, "spawn_ixp_write write file=%s data=%s address=%s\n", file, data, address);

    if(ixp_client_init(&c, address, getpid()) == -1) {
		free(address);
        fprintf(stderr, "libwmii: %s\n", c.errstr);
		return;
    }
	free(address);
    /* open */
    if(ixp_client_open(&c, fid, file, IXP_OWRITE) == -1) {
        fprintf(stderr, "libwmii: cannot open file '%s': %s\n", file, c.errstr);
        return;
    }
    if(ixp_client_write(&c, fid, 0, len, (void *)data) != len) {
    	fprintf(stderr, "libwmii: cannot write file: %s\n", c.errstr);
		return; 
	}
    ixp_client_close(&c, fid);
    ixp_client_deinit(&c);
}

void
wmii_spawn(void *dpy, char *cmd)
{
	static char *ixpcmd = nil;
	static size_t ixpcmdsz = 0;
	size_t len;

	if(!cmd)
		return;
	if(!strncmp(cmd, "#write ", 7)) {
	    if(!(len = strlen(&cmd[7]) + 1))
			return;
		if(len > ixpcmdsz) {
			ixpcmdsz = len + 1;
			if(ixpcmd)
				free(ixpcmd);
			ixpcmd = cext_emallocz(ixpcmdsz);
		}
		cext_strlcpy(ixpcmd, &cmd[7], len);
		spawn_ixp_write(ixpcmd);
		return;
	}

    /* the questionable double-fork is done to catch all zombies */
	if(fork() == 0) {
        if(fork() == 0) {
			setsid();
			close(ConnectionNumber(dpy));
			execlp("rc", "rc", "-c", cmd, (char *) 0);
			perror("failed");
			exit(1);
        }
        exit(0);
    }
    wait(0);
}
