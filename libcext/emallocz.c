/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>

#include "cext.h"

void *
cext_emallocz(size_t size)
{
    void *res = calloc(1, size);

    if(!res) {
        fprintf(stderr, "fatal: could not malloc() %d bytes\n",
                (int) size);
        exit(1);
    }
    return res;
}
