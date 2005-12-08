/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "cext.h"
#include "ixp.h"

static u8 msg[IXP_MAX_MSG];

static int do_fcall(IXPClient * c)
{
	u32 msize = ixp_fcall_to_msg(&c->fcall, msg, IXP_MAX_MSG);
	c->errstr = 0;
	if (ixp_send_message(c->fd, msg, msize, &c->errstr) != msize)
		return FALSE;
	if (!ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &c->errstr))
		return FALSE;
	if (!(msize = ixp_msg_to_fcall(msg, IXP_MAX_MSG, &c->fcall))) {
		c->errstr = "received bad message";
		return FALSE;
	}
	if (c->fcall.id == RERROR) {
		c->errstr = c->fcall.errstr;
		return FALSE;
	}
	return TRUE;
}

int ixp_client_init(IXPClient * c, char *sockfile)
{
	if ((c->fd = ixp_connect_sock(sockfile)) < 0) {
		c->errstr = "cannot connect server";
		return FALSE;
	}
	/* version */
	c->fcall.id = TVERSION;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.maxmsg = IXP_MAX_MSG;
	cext_strlcpy(c->fcall.version, IXP_VERSION, sizeof(c->fcall.version));
	if (!do_fcall(c)) {
		ixp_client_deinit(c);
		return FALSE;
	}
	if (strncmp(c->fcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
		c->errstr = "9P versions differ";
		ixp_client_deinit(c);
		return FALSE;			/* we cannot handle this version */
	}
	c->root_fid = getpid();

	/* attach */
	c->fcall.id = TATTACH;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = c->root_fid;
	c->fcall.afid = IXP_NOFID;
	cext_strlcpy(c->fcall.uname, getenv("USER"), sizeof(c->fcall.uname));
	c->fcall.aname[0] = 0;
	if (!do_fcall(c)) {
		ixp_client_deinit(c);
		return FALSE;
	}
	c->root_qid = c->fcall.qid;
	return TRUE;
}

int ixp_client_remove(IXPClient * c, u32 newfid, char *filepath)
{
	if (!ixp_client_walk(c, newfid, filepath))
		return FALSE;
	/* remove */
	c->fcall.id = TREMOVE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	return do_fcall(c);
}

int
ixp_client_create(IXPClient * c, u32 dirfid, char *name, u32 perm, u8 mode)
{
	/* create */
	c->fcall.id = TCREATE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = dirfid;
	cext_strlcpy(c->fcall.name, name, sizeof(c->fcall.name));
	c->fcall.perm = perm;
	c->fcall.mode = mode;
	return do_fcall(c);
}

int ixp_client_walk(IXPClient * c, u32 newfid, char *filepath)
{
	/* walk */
	c->fcall.id = TWALK;
	c->fcall.fid = c->root_fid;
	c->fcall.newfid = newfid;
	if (filepath) {
		cext_strlcpy(c->fcall.name, filepath, sizeof(c->fcall.name));
		c->fcall.nwname = cext_tokenize((char **) c->fcall.wname, IXP_MAX_WELEM, c->fcall.name, '/');
	}
	return do_fcall(c);
}

int ixp_client_open(IXPClient * c, u32 newfid, char *filepath, u8 mode)
{
	if (!ixp_client_walk(c, newfid, filepath))
		return FALSE;

	/* open */
	c->fcall.id = TOPEN;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = newfid;
	c->fcall.mode = mode;
	return do_fcall(c);
}

u32
ixp_client_read(IXPClient * c, u32 fid, u64 offset, void *result,
				u32 res_len)
{
	u32 bytes =
		c->fcall.maxmsg - (sizeof(u8) + sizeof(u16) + 2 * sizeof(u32) +
						   sizeof(u64));
	/* read */
	c->fcall.id = TREAD;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	c->fcall.offset = offset;
	c->fcall.count = res_len < bytes ? res_len : bytes;
	if (!do_fcall(c))
		return 0;
	memcpy(result, c->fcall.data, c->fcall.count);
	return c->fcall.count;
}

u32
ixp_client_write(IXPClient * c, u32 fid, u64 offset, u32 count, u8 * data)
{
	if (count >
		c->fcall.maxmsg - (sizeof(u8) + sizeof(u16) + 2 * sizeof(u32) +
						   sizeof(u64))) {
		c->errstr = "message size exceeds buffer size";
		return 0;
	}
	/* write */
	c->fcall.id = TWRITE;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	c->fcall.offset = offset;
	c->fcall.count = count;
	memcpy(c->fcall.data, data, count);
	if (!do_fcall(c))
		return 0;
	return c->fcall.count;
}

int ixp_client_close(IXPClient * c, u32 fid)
{
	/* clunk */
	c->fcall.id = TCLUNK;
	c->fcall.tag = IXP_NOTAG;
	c->fcall.fid = fid;
	return do_fcall(c);
}

void ixp_client_deinit(IXPClient * c)
{
	/* session finished, now shutdown */
	if (c->fd) {
		shutdown(c->fd, SHUT_RDWR);
		close(c->fd);
	}
}
