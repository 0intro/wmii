/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "wm.h"

Layout *
match_layout(char *name)
{
    Layout *l;
    for(l = layouts; l; l = l->next)
        if(!strncmp(name, l->name, strlen(l->name)))
            return l;
    return nil;
}
