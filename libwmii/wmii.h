/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
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

/* spawn.c */
void wmii_spawn(void *dpy, char *cmd);

/* wm.c */
int wmii_property(Display * dpy, Window w, Atom a, Atom t, long l,
                  unsigned char **prop);
void wmii_send_message(Display * dpy, Window w, Atom a, long value);
void wmii_init_lock_modifiers(Display * dpy, unsigned int *valid_mask,
                              unsigned int *num_lock_mask);
