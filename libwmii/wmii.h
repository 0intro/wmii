/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "ixp.h"
#include "blitz.h"

/* ixp.c */
#define E9pversion		"9P version not supported"
#define Enoperm 		"permission denied"
#define Enofid 			"fid not found"
#define Enofile 		"file not found"
#define Enomode 		"mode not supported"
#define Enofunc 		"function not supported"
#define Enocommand 		"command not supported"

char *wmii_ixp_version(IXPConn *c, Fcall *fcall);
char *wmii_ixp_attach(IXPConn *c, Fcall *fcall);
char *wmii_ixp_clunk(IXPConn *c, Fcall *fcall);

/* spawn.c */
void wmii_spawn(void *dpy, char *cmd);

/* wm.c */
int wmii_property(Display * dpy, Window w, Atom a, Atom t, long l, unsigned char **prop);
void wmii_send_message(Display * dpy, Window w, Atom a, long value);
void wmii_init_lock_modifiers(Display * dpy, unsigned int *valid_mask,
                              unsigned int *num_lock_mask);

extern Qid root_qid;
