/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ixp.h"
#include "cext.h"

static void
prepare_select(IXPServer *s)
{
    int i;
    FD_ZERO(&s->rd);
    for(i = 0; (i < s->connsz) && s->conn[i]; i++) {
    	if(s->maxfd < s->conn[i]->fd)
        	s->maxfd = s->conn[i]->fd;
        if(s->conn[i]->read)
        	FD_SET(s->conn[i]->fd, &s->rd);
    }
}

static void
handle_conns(IXPServer *s)
{
    int i;
    for(i = 0; (i < s->connsz) && s->conn[i]; i++)
    	if(FD_ISSET(s->conn[i]->fd, &s->rd) && s->conn[i]->read)
        	/* call read handler */
            s->conn[i]->read(s, s->conn[i]);
}

char *
ixp_server_loop(IXPServer *s)
{
    int r;
    s->running = 1;

    /* main loop */
    while(s->running && s->conn) {

        prepare_select(s);

        r = select(s->maxfd + 1, &s->rd, 0, 0, 0);
        if(r == -1 && errno == EINTR)
            continue;
        if(r < 0)
            return "fatal select error";
        else if(r > 0)
            handle_conns(s);
    }
	return nil;
}

IXPMap *
ixp_server_fid2map(IXPConn *c, unsigned int fid)
{
	size_t i;
	for(i = 0; (i < c->mapsz) && c->map[i]; i++)
		if(c->map[i]->fid == fid)
			return c->map[i];
	return nil;
}
