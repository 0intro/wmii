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

IXPConn *
ixp_server_open_conn(IXPServer *s, int fd, void *aux,
		void (*read)(IXPConn *c), void (*close)(IXPConn *c))
{
	IXPConn *c = cext_emallocz(sizeof(IXPConn));
	c->fd = fd;
	c->aux = aux;
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
	for(tc=&s->conn; *tc && *tc != c; tc=&(*tc)->next);
	cext_assert(*tc == c);
	*tc = c->next;
	c->closed = 1;
	if(c->close)
		c->close(c);
	else
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

unsigned int
ixp_server_receive_fcall(IXPConn *c, Fcall *fcall)
{
	unsigned int msize;
	char *errstr = 0;
	if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &errstr))) {
		ixp_server_close_conn(c);
		return 0;
	}
	return ixp_msg2fcall(fcall, msg, IXP_MAX_MSG);
}

int
ixp_server_respond_fcall(IXPConn *c, Fcall *fcall)
{
	char *errstr;
	unsigned int msize = ixp_fcall2msg(msg, fcall, IXP_MAX_MSG);
	if(c->closed)
		return 0;
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize) {
		ixp_server_close_conn(c);
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
		ixp_server_close_conn(c);
	}
}
