/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "ixp.h"

#include <cext.h>

static Connection zero_conn = { 0 };
static int user_fd = -1;

void set_error(IXPServer * s, char *errstr)
{
	if (s->errstr)
		free(s->errstr);
	if (errstr)
		s->errstr = strdup(errstr);
	else
		s->errstr = 0;
}

File *fd_to_file(IXPServer * s, int fd)
{
	int cidx = fd / MAX_CONN;
	int fidx = fd - (cidx * MAX_CONN);

	return s->conn[cidx].files[fidx];
}

static void handle_ixp_create(Connection * c)
{
	c->s->create(c->s, ((char *) c->data) + sizeof(ReqHeader));
	free(c->data);
	c->data = c->s->errstr ?
		rerror_message(c->s->errstr, &c->len) : rcreate_message(&c->len);
	c->remain = c->len;
}

static void handle_ixp_open(Connection * c)
{
	int i;

	/* seek next free slot */
	for (i = 0; (i < MAX_OPEN_FILES) && c->files[i]; i++);
	if (i == MAX_OPEN_FILES) {
		fprintf(stderr, "%s",
				"ixp: server: maximum of open files, try again later.\n");
		free(c->data);
		c->data =
			rerror_message("maximum open files reached, close files first",
						   &c->len);
		c->remain = c->len;
		return;
	}
	c->files[i] = c->s->open(c->s, ((char *) c->data) + sizeof(ReqHeader));
	c->seen[i] = MAX_SEEN_SHUTDOWN;
	free(c->data);
	c->data = c->s->errstr ?
		rerror_message(c->s->errstr,
					   &c->len) : ropen_message(i + MAX_CONN * c->index,
												&c->len);
	c->remain = c->len;
}

static void handle_ixp_read(Connection * c, ReqHeader * h)
{
	void *data = 0;
	size_t out_len;

	data = cext_emallocz(h->buf_len);
	out_len = c->s->read(c->s, h->fd, h->offset, data, h->buf_len);
	free(c->data);
	if (c->s->errstr)
		c->data = rerror_message(c->s->errstr, &c->len);
	else
		c->data = rread_message(data, out_len, &c->len);
	c->remain = c->len;
	free(data);
}

static void handle_ixp_write(Connection * c, ReqHeader * h)
{
	c->s->write(c->s, h->fd, h->offset,
				((char *) c->data) + sizeof(ReqHeader), h->buf_len);
	free(c->data);
	if (c->s->errstr)
		c->data = rerror_message(c->s->errstr, &c->len);
	else
		c->data = rwrite_message(&c->len);
	c->remain = c->len;
}

static void handle_ixp_close(Connection * c, ReqHeader * h)
{
	int fidx = h->fd - (c->index * MAX_CONN);

	c->s->close(c->s, h->fd);
	c->files[fidx] = 0;
	free(c->data);
	if (c->s->errstr)
		c->data = rerror_message(c->s->errstr, &c->len);
	else
		c->data = rclose_message(&c->len);
	c->remain = c->len;
}

static void handle_ixp_remove(Connection * c)
{
	c->s->remove(c->s, ((char *) c->data) + sizeof(ReqHeader));
	free(c->data);
	c->data = c->s->errstr ?
		rerror_message(c->s->errstr, &c->len) : rremove_message(&c->len);
	c->remain = c->len;
}

static void check_ixp_request(Connection * c)
{
	ReqHeader h;
	/* check pending request */
	if (c->s->errstr)
		set_error(c->s, 0);
	memcpy(&h, c->data, sizeof(ReqHeader));
	switch (h.req) {
	case TCREATE:
		handle_ixp_create(c);
		break;
	case TREMOVE:
		handle_ixp_remove(c);
		break;
	case TOPEN:
		handle_ixp_open(c);
		break;
	case TCLUNK:
		handle_ixp_close(c, &h);
		break;
	case TREAD:
		handle_ixp_read(c, &h);
		break;
	case TWRITE:
		handle_ixp_write(c, &h);
		break;
	default:
		fprintf(stderr, "%s", "ixp: server: invalid request\n");
		free(c->data);
		c->len = c->remain = 0;
		break;
	}
}

static void update_conns(IXPServer * s)
{
	int i;

	FD_ZERO(&s->rd);
	FD_ZERO(&s->wr);
	for (i = 0; i < MAX_CONN; i++) {
		if (s->conn[i].fd >= 0) {
			s->nfds = _MAX(s->nfds, s->conn[i].fd);
			if (s->conn[i].read && !s->conn[i].mode
				&& (!s->conn[i].len || s->conn[i].remain)) {
				FD_SET(s->conn[i].fd, &s->rd);
			}
			if (s->conn[i].write && s->conn[i].mode && s->conn[i].remain) {
				FD_SET(s->conn[i].fd, &s->wr);
			}
		}
	}
}

static void close_conn(Connection * c)
{
	int i;
	/* shutdown connection and cleanup open files */
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
	c->fd = -1;
	c->mode = 0;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (c->files[i])
			c->files[i] = 0;
	}
}

static void read_conn(Connection * c)
{
	size_t r;

	if (!c->header) {
		r = read(c->fd, &c->len, sizeof(size_t));
		if (r != sizeof(size_t)) {
			close_conn(c);
			return;
		}
		c->remain = c->len;
		c->data = cext_emallocz(c->len);
		c->header = 1;
	}
	r = read(c->fd, ((char *) c->data) + c->len - c->remain, c->remain);
	if (r < 1) {
		close_conn(c);
		return;
	}
	c->remain -= r;

	if (c->remain == 0) {
		/* check IXP request */
		c->mode = 1;			/* next mode is response */
		check_ixp_request(c);
		c->header = 0;
	}
}

static void write_conn(Connection * c)
{
	size_t r;

	if (!c->header) {
		r = write(c->fd, &c->len, sizeof(size_t));
		if (r != sizeof(size_t)) {
			close_conn(c);
		}
		c->header = 1;
	}
	r = write(c->fd, ((char *) c->data) + c->len - c->remain, c->remain);
	if (r < 1) {
		close_conn(c);
		return;
	}
	c->remain -= r;

	if (c->remain == 0) {
		c->len = 0;
		c->mode = 0;
		c->header = 0;
	}
}

static void new_conn(Connection * c)
{
	int r, i;
	socklen_t l;
	struct sockaddr_un name = { 0 };

	l = sizeof(name);
	if ((r = accept(c->fd, (struct sockaddr *) &name, &l)) < 0) {
		perror("ixp: server: cannot accept connection");
		return;
	}
	if (c->s->runlevel == SHUTDOWN) {
		fprintf(stderr, "%s",
				"ixp: server: connection refused, server is shutting down.\n");
		close(r);
		return;
	}
	for (i = 0; i < MAX_CONN; i++) {
		if (c->s->conn[i].fd == -1) {	/* free connection */
			c->s->conn[i] = zero_conn;
			c->s->conn[i].s = c->s;
			c->s->conn[i].index = i;
			c->s->conn[i].fd = r;
			c->s->conn[i].read = read_conn;
			c->s->conn[i].write = write_conn;
			break;
		}
	}

	if (i == MAX_CONN) {
		fprintf(stderr, "%s",
				"ixp: server: connection refused, try again later.\n");
		close(r);
	}
}


static int check_open_files(Connection * c)
{
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (c->files[i] && c->seen[i]) {
			c->seen[i]--;
			return 1;
		}
	}
	return 0;
}

static void handle_socks(IXPServer * s)
{
	int i, now = 1;
	for (i = 0; i < MAX_CONN; i++) {
		if (s->conn[i].fd >= 0) {
			if (FD_ISSET(s->conn[i].fd, &s->rd) && s->conn[i].read) {
				/* call back read handler */
				s->conn[i].read(&s->conn[i]);
			} else if (FD_ISSET(s->conn[i].fd, &s->wr) && s->conn[i].write) {
				/* call back write handler */
				s->conn[i].write(&s->conn[i]);
			}
			/*
			 * don't shutdown, if there're remaining bits or if
			 * still responses are sent or still opened files
			 */
			if ((s->runlevel == SHUTDOWN)
				&& (check_open_files(&s->conn[i])
					|| (s->conn[i].remain > 0)
					|| s->conn[i].mode))
				now = 0;
		}
	}
	if ((s->runlevel == SHUTDOWN) && now)
		s->runlevel = HALT;		/* real stop */
}

IXPServer *init_server(char *sockfile, void (*cleanup) (void))
{
	int i;
	struct sockaddr_un addr = { 0 };
	int yes = 1;
	socklen_t su_len;
	IXPServer *s;

	/* init */
	s = (IXPServer *) cext_emallocz(sizeof(IXPServer));
	s->sockfile = sockfile;
	s->root = (File *) cext_emallocz(sizeof(File));
	s->runlevel = HALT;			/* initially server is not running */
	s->create = ixp_create;
	s->remove = ixp_remove;
	s->open = ixp_open;
	s->close = ixp_close;
	s->read = ixp_read;
	s->write = ixp_write;
	s->root->name = strdup("");
	for (i = 0; i < MAX_CONN; i++) {
		s->conn[i].s = s;
		s->conn[i].fd = -1;
		s->conn[i].index = i;
	}

	signal(SIGPIPE, SIG_IGN);
	if ((s->conn[0].fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("ixp: server: socket");
		free(s);
		return 0;
	}
	if (setsockopt(s->conn[0].fd, SOL_SOCKET, SO_REUSEADDR,
				   (char *) &yes, sizeof(yes)) < 0) {
		perror("ixp: server: setsockopt");
		close(s->conn[0].fd);
		free(s);
		return 0;
	}
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockfile, sizeof(addr.sun_path));
	su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

	if (bind(s->conn[0].fd, (struct sockaddr *) &addr, su_len) < 0) {
		perror("ixp: server: cannot bind socket");
		close(s->conn[0].fd);
		free(s);
		return 0;
	}
	chmod(sockfile, S_IRWXU);

	if (listen(s->conn[0].fd, MAX_CONN) < 0) {
		perror("ixp: server: cannot listen on socket");
		close(s->conn[0].fd);
		free(s);
		return 0;
	}
	s->conn[0].read = new_conn;

	/* register to cleanup function, to unlink sockfile */
	if (cleanup)
		atexit(cleanup);

	return s;
}

void
run_server_with_fd_support(IXPServer * s, int fd,
						   void (*fd_read) (Connection *),
						   void (*fd_write) (Connection *))
{
	s->conn[1] = zero_conn;
	s->conn[1].index = 1;
	s->conn[1].s = s;
	s->conn[1].fd = user_fd = fd;
	s->conn[1].read = fd_read;
	s->conn[1].write = fd_write;
	run_server(s);
}

void run_server(IXPServer * s)
{
	int r, i;
	s->runlevel = RUNNING;

	/* main loop */
	while (s->runlevel != HALT) {

		update_conns(s);

		r = select(s->nfds + 1, &s->rd, &s->wr, 0, 0);
		if (r == -1 && errno == EINTR)
			continue;
		if (r < 0) {
			perror("ixp: server: select");
			break;				/* allow cleanups in IXP using app */
		} else if (r > 0) {
			handle_socks(s);
		}
	}
	/* shut down server */
	for (i = MAX_CONN - 1; i >= 0; i--) {
		if (s->conn[i].fd >= 0 && s->conn[i].fd != user_fd) {
			close(s->conn[i].fd);
		}
	}
}

void deinit_server(IXPServer * s)
{
	unlink(s->sockfile);
	ixp_remove(s, "/");
	free(s);
}
