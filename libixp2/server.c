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

static IXPConn zero_conn = { -1, 0, 0, 0 };

IXPConn *
ixp_server_alloc_conn(IXPServer *s)
{
    int i;
    for(i = 0; i < IXP_MAX_CONN; i++)
        if(s->conn[i].fd < 0)
            return &s->conn[i];
    return nil;
}

static void
prepare_select(IXPServer *s)
{
    int i;
    FD_ZERO(&s->rd);
    for(i = 0; i < IXP_MAX_CONN; i++) {
        if(s->conn[i].fd >= 0) {
            if(s->maxfd < s->conn[i].fd)
                s->maxfd = s->conn[i].fd;
            if(s->conn[i].read)
                FD_SET(s->conn[i].fd, &s->rd);
        }
    }
}

void
ixp_server_free_conn(IXPServer *s, IXPConn *c)
{
    if(c->close)
		c->close(s, c);
    *c = zero_conn;
}

static void
handle_conns(IXPServer *s)
{
    int i;
    for(i = 0; i < IXP_MAX_CONN; i++) {
        if(s->conn[i].fd >= 0) {
            if(FD_ISSET(s->conn[i].fd, &s->rd) && s->conn[i].read)
                /* call back read handler */
                s->conn[i].read(s, &s->conn[i]);
        }
    }
}

char *
ixp_server_loop(IXPServer *s)
{
    int r;
    s->running = 1;

    /* main loop */
    while(s->running) {

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

void
ixp_server_init(IXPServer *s)
{
	size_t i;
    for(i = 0; i < IXP_MAX_CONN; i++)
        s->conn[i] = zero_conn;
}

void
ixp_server_deinit(IXPServer *s)
{
    int i;
    /* shut down server */
    for(i = 0; i < IXP_MAX_CONN; i++)
        if(s->conn[i].fd >= 0)
            ixp_server_free_conn(s, &s->conn[i]);
}
