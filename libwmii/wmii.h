/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "ixp.h"
#include "blitz.h"

#define MAX_ID			  20
#define MAX_BUF			 128

typedef struct Action Action;

struct Action {
	char *name;
	void (*func) (void *obj, char *);
};

/* ixputil.c */
File *wmii_create_ixpfile(IXPServer * s, char *key, char *val);
void wmii_get_ixppath(File * f, char *path, size_t size);
void wmii_move_ixpfile(File * f, File * to_parent);
IXPServer *wmii_setup_server(char *sockfile);

/* spawn.c */
void spawn(void *dpy, char *cmd);

/* util.c */
void swap(void **p1, void **p2);

/* wm.c */
int
property(Display * dpy, Window w, Atom a, Atom t, long l,
		 unsigned char **prop);
void win_prop(Display * dpy, Window w, Atom a, char *res, int len);
void send_message(Display * dpy, Window w, Atom a, long value);
void
init_lock_modifiers(Display * dpy, unsigned int *valid_mask,
					unsigned int *num_lock_mask);
