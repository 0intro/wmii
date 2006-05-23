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
	if(c->fcall.id == RERROR) {
		c->errstr = c->fcall.errstr;
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
	c->fcall.id = TVERSION;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.maxmsg = IXP_MAX_MSG;
	cext_strlcpy(c->fcall.version, IXP_VERSION, sizeof(c->fcall.version));
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->fcall.errstr);
		ixp_client_hangup(c);
		return -1;
	}
	if(strncmp(c->fcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
		fprintf(stderr, "error: %s\n", c->fcall.errstr);
		c->errstr = "9P versions differ";
		ixp_client_hangup(c);
		return -1;	/* we cannot handle this version */
	}
	c->root_fid = rootfid;

	c->fcall.id = TATTACH;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = c->root_fid;
	c->fcall.afid = IXP_NOFID;
	cext_strlcpy(c->fcall.uname, getenv("USER"), sizeof(c->fcall.uname));
	c->fcall.aname[0] = 0;
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->fcall.errstr);
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
	c->fcall.id = TREMOVE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	return ixp_client_do_fcall(c);
}

int
ixp_client_create(IXPClient *c, unsigned int dirfid, char *name,
		unsigned int perm, unsigned char mode)
{
	c->fcall.id = TCREATE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = dirfid;
	cext_strlcpy(c->fcall.name, name, sizeof(c->fcall.name));
	c->fcall.perm = perm;
	c->fcall.mode = mode;
	return ixp_client_do_fcall(c);
}

int
ixp_client_walk(IXPClient *c, unsigned int newfid, char *filepath)
{
	unsigned int i;
	char *wname[IXP_MAX_WELEM];
	c->fcall.id = TWALK;
	c->fcall.fid = c->root_fid;
	c->fcall.newfid = newfid;
	if(filepath) {
		cext_strlcpy(c->fcall.name, filepath, sizeof(c->fcall.name));
		c->fcall.nwname =
			cext_tokenize(wname, IXP_MAX_WELEM, c->fcall.name, '/');
		for(i = 0; i < c->fcall.nwname; i++)
			cext_strlcpy(c->fcall.wname[i], wname[i], sizeof(c->fcall.wname[i]));
	}
	return ixp_client_do_fcall(c);
}

int
ixp_client_stat(IXPClient *c, unsigned int newfid, char *filepath)
{
	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;

	c->fcall.id = TSTAT;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	return ixp_client_do_fcall(c);
}

int
ixp_client_open(IXPClient *c, unsigned int newfid, unsigned char mode)
{
	c->fcall.id = TOPEN;
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

	c->fcall.id = TREAD;
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
	c->fcall.id = TWRITE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	c->fcall.offset = offset;
	c->fcall.count = count;
	memcpy(c->fcall.data, data, count);
	if(ixp_client_do_fcall(c) == -1)
		return -1;
	return c->fcall.count;
}

int
ixp_client_close(IXPClient *c, unsigned int fid)
{
	c->fcall.id = TCLUNK;
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
