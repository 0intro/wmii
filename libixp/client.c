/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ixp.h"

int
ixp_client_do_fcall(IXPClient *c)
{
	static unsigned char msg[IXP_MAX_MSG];
	unsigned int msize = ixp_fcall2msg(msg, &c->fcall, IXP_MAX_MSG);
	c->errstr = 0;
	if(ixp_send_message(c->fd, msg, msize, &c->errstr) != msize)
		return -1;
	if(!ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &c->errstr))
		return -1;
	if(!(msize = ixp_msg2fcall(&c->fcall, msg, IXP_MAX_MSG))) {
		c->errstr = "received bad message";
		return -1;
	}
	if(c->fcall.type == RERROR) {
		c->errstr = c->fcall.ename;
		return -1;
	}
	return 0;
}

int
ixp_client_dial(IXPClient *c, char *sockfile, unsigned int rootfid)
{
	if((c->fd = ixp_connect_sock(sockfile)) < 0) {
		c->errstr = "cannot connect server";
		return -1;
	}
	c->fcall.type = TVERSION;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.msize = IXP_MAX_MSG;
	c->fcall.version = cext_estrdup(IXP_VERSION);
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->errstr);
		ixp_client_hangup(c);
		return -1;
	}
	if(strncmp(c->fcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
		fprintf(stderr, "error: %s\n", c->errstr);
		c->errstr = "9P versions differ";
		ixp_client_hangup(c);
		return -1;	/* we cannot handle this version */
	}
	c->root_fid = rootfid;

	c->fcall.type = TATTACH;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = c->root_fid;
	c->fcall.afid = IXP_NOFID;
	c->fcall.uname = cext_estrdup(getenv("USER"));
	c->fcall.aname = cext_estrdup("");
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->errstr);
		ixp_client_hangup(c);
		return -1;
	}
	c->root_qid = c->fcall.qid;
	return 0;
}

int
ixp_client_remove(IXPClient *c, unsigned int newfid, char *filepath)
{
	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;
	c->fcall.type = TREMOVE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	return ixp_client_do_fcall(c);
}

int
ixp_client_create(IXPClient *c, unsigned int dirfid, char *name,
		unsigned int perm, unsigned char mode)
{
	c->fcall.type = TCREATE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = dirfid;
	c->fcall.name = cext_estrdup(name);
	c->fcall.perm = perm;
	c->fcall.mode = mode;
	return ixp_client_do_fcall(c);
}

int
ixp_client_walk(IXPClient *c, unsigned int newfid, char *filepath)
{
	unsigned int i;
	char *wname[IXP_MAX_WELEM];
	c->fcall.type = TWALK;
	c->fcall.fid = c->root_fid;
	c->fcall.newfid = newfid;
	if(filepath) {
		c->fcall.name = cext_estrdup(filepath);
		c->fcall.nwname =
			cext_tokenize(wname, IXP_MAX_WELEM, c->fcall.name, '/');
		for(i = 0; i < c->fcall.nwname; i++)
			c->fcall.wname[i] = cext_estrdup(wname[i]);
	}
	return ixp_client_do_fcall(c);
}

int
ixp_client_stat(IXPClient *c, unsigned int newfid, char *filepath)
{
	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;

	c->fcall.type = TSTAT;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	return ixp_client_do_fcall(c);
}

int
ixp_client_open(IXPClient *c, unsigned int newfid, unsigned char mode)
{
	c->fcall.type = TOPEN;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	c->fcall.mode = mode;
	return ixp_client_do_fcall(c);
}

int
ixp_client_walkopen(IXPClient *c, unsigned int newfid, char *filepath,
		unsigned char mode)
{
	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;
	return ixp_client_open(c, newfid, mode);
}

int
ixp_client_read(IXPClient *c, unsigned int fid, unsigned long long offset,
		void *result, unsigned int res_len)
{
	unsigned int bytes = c->fcall.iounit;

	c->fcall.type = TREAD;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	c->fcall.offset = offset;
	c->fcall.count = res_len < bytes ? res_len : bytes;
	if(ixp_client_do_fcall(c) == -1)
		return -1;
	memcpy(result, c->fcall.data, c->fcall.count);
	return c->fcall.count;
}

int
ixp_client_write(IXPClient *c, unsigned int fid,
		unsigned long long offset, unsigned int count,
		unsigned char *data)
{
	if(count > c->fcall.iounit) {
		c->errstr = "iounit exceeded";
		return -1;
	}
	c->fcall.type = TWRITE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	c->fcall.offset = offset;
	c->fcall.count = count;
	c->fcall.data = (void *)data;
	if(ixp_client_do_fcall(c) == -1)
		return -1;
	return c->fcall.count;
}

int
ixp_client_close(IXPClient *c, unsigned int fid)
{
	c->fcall.type = TCLUNK;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	return ixp_client_do_fcall(c);
}

void
ixp_client_hangup(IXPClient *c)
{
	/* session finished, now shutdown */
	if(c->fd) {
		shutdown(c->fd, SHUT_RDWR);
		close(c->fd);
	}
}
