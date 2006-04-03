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

static Vector *
conn2vector(ConnVector *cv)
{
	return (Vector *) cv;
}

Vector *
ixp_map2vector(MapVector *mv)
{
	return (Vector *) mv;
}

IXPConn *ixp_server_open_conn(IXPServer *s, int fd, void (*read)(IXPConn *c),
		void (*close)(IXPConn *c))
{
	IXPConn *c = cext_emallocz(sizeof(IXPConn));
	c->fd = fd;
	c->srv = s;
	c->read = read;
	c->close = close;
	cext_vattach(conn2vector(&s->conn), c);
	return c;
}

void
ixp_server_close_conn(IXPConn *c)
{
	IXPServer *s = c->srv;
	cext_vdetach(conn2vector(&s->conn), c);
	while(c->map.size) {
		IXPMap *m =  c->map.data[0];
		cext_vdetach(ixp_map2vector(&c->map), m);
		free(m);
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
	for(i = 0; i < s->conn.size; i++) {
		if(s->maxfd < s->conn.data[i]->fd)
			s->maxfd = s->conn.data[i]->fd;
		if(s->conn.data[i]->read)
			FD_SET(s->conn.data[i]->fd, &s->rd);
	}
}

static void
handle_conns(IXPServer *s)
{
	int i;
	for(i = 0; i < s->conn.size; i++)
		if(FD_ISSET(s->conn.data[i]->fd, &s->rd) && s->conn.data[i]->read)
			/* call read handler */
			s->conn.data[i]->read(s->conn.data[i]);
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
	unsigned int i;
	for(i = 0; i < c->map.size; i++)
		if(c->map.data[i]->fid == fid)
			return c->map.data[i];
	return nil;
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
	unsigned int i;
	for(i = 0; i < s->conn.size; i++)
		if(s->conn.data[i]->close)
			s->conn.data[i]->close(s->conn.data[i]);
}
