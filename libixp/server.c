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

static unsigned char *msg[IXP_MAX_MSG];

IXPConn *ixp_server_open_conn(IXPServer *s, int fd, void (*read)(IXPConn *c),
		void (*close)(IXPConn *c))
{
	IXPConn *c = cext_emallocz(sizeof(IXPConn));
	c->fd = fd;
	c->srv = s;
	c->read = read;
	c->close = close;
	c->next = s->conn;
	s->conn = c;
	return c;
}

void
ixp_server_close_conn(IXPConn *c)
{
	IXPServer *s = c->srv;
	IXPConn **tc;
	IXPMap *m;
	for(tc=&s->conn; *tc && *tc != c; tc=&(*tc)->next);
	cext_assert(*tc == c);
	*tc = c->next;
	while((m = c->map)) {
		c->map = m->next;
		free(m);
	}
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
	free(c);
}

static void
prepare_select(IXPServer *s)
{
	IXPConn **c;
	FD_ZERO(&s->rd);
	for(c=&s->conn; *c; *c && (c=&(*c)->next)) {
		if(s->maxfd < (*c)->fd)
			s->maxfd = (*c)->fd;
		if((*c)->read)
			FD_SET((*c)->fd, &s->rd);
	}
}

static void
handle_conns(IXPServer *s)
{
	IXPConn **c;
	for(c=&s->conn; *c; *c && (c=&(*c)->next))
		if(FD_ISSET((*c)->fd, &s->rd) && (*c)->read)
			/* call read handler */
			(*c)->read(*c);
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

IXPMap *
ixp_server_fid2map(IXPConn *c, unsigned int fid)
{
	IXPMap *m;
	for(m=c->map; m && m->fid != fid; m=m->next);
	return m;
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
	return ixp_msg2fcall(fcall, msg, IXP_MAX_MSG);
}

int
ixp_server_respond_fcall(IXPConn *c, Fcall *fcall)
{
	char *errstr;
	unsigned int msize = ixp_fcall2msg(msg, fcall, IXP_MAX_MSG);
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
	msize = ixp_fcall2msg(msg, fcall, IXP_MAX_MSG);
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize) {
		if(c->close)
			c->close(c);
		return -1;
	}
	return 0;
}

void
ixp_server_close(IXPServer *s)
{
	IXPConn *c, *next;
	for(c=s->conn; c; c=next) {
		next=c->next;
		if(c->close)
			c->close(c);
	}
}
