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


Fcall **
ixp_server_attach_fcall(Fcall *f, Fcall **array, size_t *size)
{
	size_t i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(sizeof(Fcall *) * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		Fcall **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(sizeof(Fcall *) * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = f;
	return array;
}

void
ixp_server_detach_fcall(Fcall *f, Fcall **array)
{
	size_t i;
	for(i = 0; array[i] != f; i++);
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}

IXPMap **
ixp_server_attach_map(IXPMap *m, IXPMap **array, size_t *size)
{
	size_t i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(sizeof(IXPMap *) * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		IXPMap **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(sizeof(IXPMap *) * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = m;
	return array;
}

void
ixp_server_detach_map(IXPMap *m, IXPMap **array)
{
	size_t i;
	for(i = 0; array[i] != m; i++);
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}


IXPMap *
ixp_server_fid2map(IXPReq *r, unsigned int fid)
{
	size_t i;
	for(i = 0; (i < r->mapsz) && r->map[i]; i++)
		if(r->map[i]->fid == fid)
			return r->map[i];
	return nil;
}

void
ixp_server_close_conn(IXPServer *s, IXPConn *c)
{
	IXPReq *r = c->aux;

	if(r) {
		size_t i;
		if(r->map) {
			for(i = 0; (i < r->mapsz) && r->map[i]; i++)
				free(r->map[i]);
			free(r->map);
		}
		if(r->async) {
			for(i = 0; (i < r->asyncsz) && r->async[i]; i++)
				free(r->async[i]);
			free(r->async);
			free(r);
		}
	}
	c->aux = nil;
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
}
