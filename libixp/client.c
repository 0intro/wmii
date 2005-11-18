/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ixp.h"

#include <cext.h>

static IXPClient zero_client = {0};
static size_t   offsets[MAX_CONN * MAX_OPEN_FILES][2];	/* set of possible fd's */

static void 
check_error(IXPClient * c, void *msg)
{
	ResHeader       h;

	if (c->errstr)
		free(c->errstr);
	c->errstr = 0;

	memcpy(&h, msg, sizeof(ResHeader));
	if (h.res == RERROR)
		c->errstr = strdup((char *) msg + sizeof(ResHeader));
}

static void 
handle_dead_server(IXPClient * c)
{
	if (c->errstr)
		free(c->errstr);
	c->errstr = strdup(DEAD_SERVER);
	if (c->fd) {
		shutdown(c->fd, SHUT_RDWR);
		close(c->fd);
	}
	c->fd = -1;
}

static void    *
poll_server(IXPClient * c, void *request, size_t req_len,
	    size_t * out_len)
{
	size_t          num = 0;
	void           *result = 0;
	int             r, header = 0;

	if (c->errstr)
		free(c->errstr);
	c->errstr = 0;

	/* first send request */
	while (num < req_len) {
		FD_ZERO(&c->wr);
		FD_SET(c->fd, &c->wr);
		r = select(c->fd + 1, 0, &c->wr, 0, 0);
		if (r == -1 && errno == EINTR)
			continue;
		if (r < 0) {
			perror("ixp: client: select");
			if (result)
				free(result);
			handle_dead_server(c);
			return 0;
		} else if (r > 0) {
			if (!header) {
				/* write header first */
				r = write(c->fd, &req_len, sizeof(size_t));
				if (r != sizeof(size_t)) {
					if (result)
						free(result);
					handle_dead_server(c);
					return 0;
				}
				header = 1;
			}
			r = write(c->fd, ((char *) request) + num, req_len - num);
			if (r < 1) {
				perror("ixp: client: write");
				if (result)
					free(result);
				handle_dead_server(c);
				return 0;
			}
			num += r;
		}
	}
	free(request);		/* cleanup */

	num = 0;
	header = 0;
	/* now wait for response */
	do {
		FD_ZERO(&c->rd);
		FD_SET(c->fd, &c->rd);
		r = select(c->fd + 1, &c->rd, 0, 0, 0);
		if (r == -1 && errno == EINTR)
			continue;
		if (r < 0) {
			perror("ixp: client: select");
			if (result)
				free(result);
			handle_dead_server(c);
			return 0;
		} else if (r > 0) {
			if (!header) {
				r = read(c->fd, out_len, sizeof(size_t));
				if (r != sizeof(size_t)) {
					if (result)
						free(result);
					handle_dead_server(c);
					return 0;
				}
				result = emalloc(*out_len);
				header = 1;
			}
			r = read(c->fd, ((char *) result) + num, *out_len - num);
			if (r < 1) {
				perror("ixp: client: read");
				if (result)
					free(result);
				handle_dead_server(c);
				return 0;
			}
			num += r;
		}
	}
	while (num != *out_len);

	/* error checking */
	if (result)
		check_error(c, result);
	if (c->errstr) {
		free(result);
		result = 0;
	}
	return result;
}

static void 
cixp_create(IXPClient * c, char *path)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = tcreate_message(path, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return;
	memcpy(&h, result, sizeof(ResHeader));
	free(result);
}

static int 
cixp_open(IXPClient * c, char *path)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = topen_message(path, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return -1;
	memcpy(&h, result, sizeof(ResHeader));
	free(result);
	offsets[h.fd][0] = offsets[h.fd][1] = 0;
	return h.fd;
}

static          size_t
cixp_read(IXPClient * c, int fd, void *out_buf, size_t out_buf_len)
{
	size_t          len;

	len = seek_read(c, fd, offsets[fd][0], out_buf, out_buf_len);
	if (c->errstr)
		return 0;
	if (len == out_buf_len)
		offsets[fd][0] += len;
	return len;
}

size_t
seek_read(IXPClient * c, int fd, size_t offset,
	  void *out_buf, size_t out_buf_len)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = tread_message(fd, offset, out_buf_len, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return -1;
	memcpy(&h, result, sizeof(ResHeader));
	memcpy(out_buf, ((char *) result) + sizeof(ResHeader), h.buf_len);
	free(result);
	return h.buf_len;
}

static void 
cixp_write(IXPClient * c, int fd, void *content, size_t in_len)
{
	seek_write(c, fd, offsets[fd][1], content, in_len);
	if (!c->errstr)
		offsets[fd][1] += in_len;
}

void
seek_write(IXPClient * c, int fd, size_t offset, void *content,
	   size_t in_len)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = twrite_message(fd, offset, content, in_len, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return;
	memcpy(&h, result, sizeof(ResHeader));
	free(result);
}

static void 
cixp_close(IXPClient * c, int fd)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = tclose_message(fd, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return;
	memcpy(&h, result, sizeof(ResHeader));
	free(result);
	offsets[fd][0] = offsets[fd][1] = 0;
}

static void 
cixp_remove(IXPClient * c, char *path)
{
	ResHeader       h;
	size_t          req_len, res_len;
	void           *req = tremove_message(path, &req_len);
	void           *result = poll_server(c, req, req_len, &res_len);

	if (!result)
		return;
	memcpy(&h, result, sizeof(ResHeader));
	free(result);
}

IXPClient      *
init_client(char *sockfile)
{
	struct sockaddr_un addr = {0};
	socklen_t       su_len;

	/* init */
	IXPClient      *c = (IXPClient *) emalloc(sizeof(IXPClient));
	*c = zero_client;
	c->create = cixp_create;
	c->open = cixp_open;
	c->read = cixp_read;
	c->write = cixp_write;
	c->close = cixp_close;
	c->remove = cixp_remove;

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockfile, sizeof(addr.sun_path));
	su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

	if ((c->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		free(c);
		return 0;
	}
	if (connect(c->fd, (struct sockaddr *) & addr, su_len)) {
		close(c->fd);
		free(c);
		return 0;
	}
	return c;
}

void 
deinit_client(IXPClient * c)
{
	if (c->fd) {
		shutdown(c->fd, SHUT_RDWR);
		close(c->fd);
	}
	c->fd = -1;
	free(c);
}
