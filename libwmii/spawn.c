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
	static IXPClient c;
	char *file = cmd;
	char *data = strchr(cmd, ' ');
	char *address = getenv("WMII_ADDRESS");
    unsigned int fid = c.root_fid << 2;
	size_t len;

	if(!data || !cmd || !address)
		return;
	*data = 0;
	data++;
	len = strlen(data);

    if(ixp_client_init(&c, address) == -1) {
        fprintf(stderr, "libwmii: %s\n", c.errstr);
		return;
    }
    /* open */
    if(ixp_client_open(&c, fid, file, IXP_OWRITE) == -1) {
        fprintf(stderr, "libwmii: cannot open file '%s': %s\n", file, c.errstr);
        return;
    }
    if(ixp_client_write(&c, fid, 0, len, (void *)data) != len) {
    	fprintf(stderr, "wmiir: cannot write file: %s\n", c.errstr);
		return; 
	}
    ixp_client_close(&c, fid);
    ixp_client_deinit(&c);
}

void
wmii_spawn(void *dpy, char *cmd)
{
    /* the questionable double-fork is done to catch all zombies */
	if(!cmd)
		return;
	if(!strncmp(cmd, "#write ", 7))
		spawn_ixp_write(&cmd[7]);
	else if(fork() == 0) {
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
