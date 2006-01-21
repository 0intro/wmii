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
static unsigned char msg[IXP_MAX_MSG];

static IXPConn *
next_free_conn(IXPServer * s)
{
    int i;
    for(i = 0; i < IXP_MAX_CONN; i++)
        if(s->conn[i].fd < 0)
            return &s->conn[i];
    return nil;
}

static void
prepare_select(IXPServer * s)
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
ixp_server_rm_conn(IXPServer * s, IXPConn * c)
{
    if(!c->dont_close) {
        shutdown(c->fd, SHUT_RDWR);
        close(c->fd);
    }
    if(s->freeconn)
        s->freeconn(s, c);
    *c = zero_conn;
}

static IXPConn *
init_conn(IXPConn * c, int fd, int dont_close,
          void (*read) (IXPServer *, IXPConn *))
{
    *c = zero_conn;
    c->fd = fd;
	c->retry = nil;
    c->dont_close = dont_close;
    c->read = read;
    return c;
}

IXPConn *
ixp_server_add_conn(IXPServer * s, int fd, int dont_close,
                    void (*read) (IXPServer *, IXPConn *))
{
    IXPConn *c = next_free_conn(s);
    if(!c)
        return nil;
    return init_conn(c, fd, dont_close, read);
}

static void
handle_conns(IXPServer * s)
{
    int i;
    for(i = 0; i < IXP_MAX_CONN; i++) {
        if(s->conn[i].fd >= 0) {
            if((s->conn[i].retry || FD_ISSET(s->conn[i].fd, &s->rd)) && s->conn[i].read)
                /* call back read handler */
                s->conn[i].read(s, &s->conn[i]);
        }
    }
}

static void
server_client_read(IXPServer * s, IXPConn * c)
{
    unsigned int i, msize;
	int ret;
    s->errstr = 0;
	if(!c->retry) {
		if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &s->errstr))) {
			ixp_server_rm_conn(s, c);
			return;
		}
	}
	else
		memcpy(msg, c->retry, c->size);

    /*fprintf(stderr, "msize=%d\n", msize);*/
    if((msize = ixp_msg_to_fcall(msg, IXP_MAX_MSG, &s->fcall))) {
        for(i = 0; s->funcs && s->funcs[i].id; i++) {
            if(s->funcs[i].id == s->fcall.id) {
				ret = s->funcs[i].tfunc(s, c);
				if(ret == -1)
					break;
				else if(ret == -2) {
					c->size = msize;
				    c->retry = cext_emallocz(msize);
					memcpy(c->retry, msg, msize);
					return;
				}
				if(c->retry)
					free(c->retry);
				c->retry = nil;
                msize = ixp_fcall_to_msg(&s->fcall, msg, s->fcall.maxmsg);
                /*fprintf(stderr, "msize=%d\n", msize);*/
                if(ixp_send_message(c->fd, msg, msize, &s->errstr) != msize)
                    break;
                return;
            }
        }
    }
	/*fprintf(stderr, "function id=%d\n", s->fcall.id);*/
    if(!s->errstr)
        s->errstr = "function not supported";
    s->fcall.id = RERROR;
    cext_strlcpy(s->fcall.errstr, s->errstr, sizeof(s->fcall.errstr));
    msize = ixp_fcall_to_msg(&s->fcall, msg, IXP_MAX_MSG);
    if(ixp_send_message(c->fd, msg, msize, &s->errstr) != msize)
        ixp_server_rm_conn(s, c);
}

static void
server_read(IXPServer * s, IXPConn * c)
{
    int fd;
    IXPConn *new = next_free_conn(s);
    if(new && ((fd = ixp_accept_sock(c->fd)) >= 0))
        init_conn(new, fd, 0, server_client_read);
}

void
ixp_server_loop(IXPServer * s)
{
    int r;
    s->running = 1;
    s->errstr = 0;

    /* main loop */
    while(s->running) {

        prepare_select(s);

        r = select(s->maxfd + 1, &s->rd, 0, 0, 0);
        if(r == -1 && errno == EINTR)
            continue;
        if(r < 0) {
            s->errstr = "fatal select error";
            break;              /* allow cleanups in IXP using app */
        } else if(r > 0)
            handle_conns(s);
    }
}

int
ixp_server_tversion(IXPServer * s, IXPConn * c)
{
    /*fprintf(stderr, "got version %s (%s) %d (%d)\n", s->fcall.version,
            IXP_VERSION, s->fcall.maxmsg, IXP_MAX_MSG);*/
    if(strncmp(s->fcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
        s->errstr = "9P versions differ";
        return -1;
    } else if(s->fcall.maxmsg > IXP_MAX_MSG)
        s->fcall.maxmsg = IXP_MAX_MSG;
    s->fcall.id = RVERSION;
    return 0;
}

int
ixp_server_init(IXPServer * s, char *address, IXPTFunc * funcs,
                void (*freeconn) (IXPServer *, IXPConn *))
{
    int fd, i;
    s->funcs = funcs;
    s->freeconn = freeconn;
    s->errstr = 0;
    if(!address) {
        s->errstr = "no socket address provided or invalid directory";
        return -1;
    }
    if((fd = ixp_create_sock(address, &s->errstr)) < 0)
        return -1;
    for(i = 0; i < IXP_MAX_CONN; i++)
        s->conn[i] = zero_conn;
    ixp_server_add_conn(s, fd, 0, server_read);
    return 0;
}

void
ixp_server_deinit(IXPServer * s)
{
    int i;
    /* shut down server */
    for(i = 0; i < IXP_MAX_CONN; i++)
        if(s->conn[i].fd >= 0)
            ixp_server_rm_conn(s, &s->conn[i]);
}
