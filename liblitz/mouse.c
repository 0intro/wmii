/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blitz.h"

/* free the result manually! */
char *blitz_buttontostr(unsigned int button)
{
	char result[8];
	result[0] = 0;
	snprintf(result, 8, "Button%ud", button - Button1);
	return cext_estrdup(result);
}

unsigned int blitz_strtobutton(char *val)
{
	unsigned int res = 0;
	if (val && strlen(val) > 6 && !strncmp(val, "Button", 6))
		res = blitz_strtonum(&val[6], 1, 5) + Button1;
	return res;
}
