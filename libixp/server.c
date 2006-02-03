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

static unsigned char *msg[IXP_MAX_MSG];

IXPConn *ixp_server_open_conn(IXPServer *s, int fd, void (*read)(IXPConn *c),
							 void (*close)(IXPConn *c))
{
	IXPConn *c = cext_emallocz(sizeof(IXPConn));
	c->fd = fd;
	c->srv = s;
	c->read = read;
	c->close = close;
	s->conn = (IXPConn **)cext_array_attach((void **)s->conn, c,
					sizeof(IXPConn *), &s->connsz);
	return c;
}

void
ixp_server_close_conn(IXPConn *c)
{
	size_t i;
	IXPServer *s = c->srv;
	cext_array_detach((void **)s->conn, c, &s->connsz);
	if(c->map) {
		for(i = 0; (i < c->mapsz) && c->map[i]; i++)
			free(c->map[i]);
		free(c->map);
	}
	if(c->pend) {
		for(i = 0; (i < c->pendsz) && c->pend[i]; i++)
			free(c->pend[i]);
		free(c->pend);
	}
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
	free(c);
}

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
            s->conn[i]->read(s->conn[i]);
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

void ixp_server_enqueue_fcall(IXPConn *c, Fcall *fcall)
{
	c->pend = (Fcall **)cext_array_attach((void **)c->pend, fcall, sizeof(Fcall *), &c->pendsz);
}

unsigned int
ixp_server_receive_fcall(IXPConn *c, Fcall *fcall)
{
    unsigned int msize;
    char *errstr = 0;
	if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &errstr))) {
		if(c->close)
			c->close(c);
		return 0;
	}
    return ixp_msg_to_fcall(msg, IXP_MAX_MSG, fcall);
}

int
ixp_server_respond_fcall(IXPConn *c, Fcall *fcall)
{
	char *errstr;
	unsigned int msize = ixp_fcall_to_msg(fcall, msg, IXP_MAX_MSG);
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize) {
		if(c->close)
			c->close(c);
		return -1;
	}
	return 0;
}
 
int
ixp_server_respond_error(IXPConn *c, Fcall *fcall, char *errstr)
{
	unsigned int msize;
	fcall->id = RERROR;
	cext_strlcpy(fcall->errstr, errstr, sizeof(fcall->errstr));
	msize = ixp_fcall_to_msg(fcall, msg, IXP_MAX_MSG);
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize) {
		if(c->close)
			c->close(c);
		return -1;
	}
	return 0;
}
