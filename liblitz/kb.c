/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "blitz.h"

#include <cext.h>

/* free the result manually! */
char           *
blitz_modtostr(unsigned long mod)
{
	char            result[60];
	result[0] = '\0';

	if (mod & ShiftMask)
		_strlcat(result, "S-", sizeof(result));
	if (mod & ControlMask)
		_strlcat(result, "C-", sizeof(result));
	if (mod & Mod1Mask)
		_strlcat(result, "M-", sizeof(result));
	if (mod & Mod2Mask)
		_strlcat(result, "M2-", sizeof(result));
	if (mod & Mod3Mask)
		_strlcat(result, "M3-", sizeof(result));
	if (mod & Mod4Mask)
		_strlcat(result, "WIN-", sizeof(result));
	if (mod & Mod5Mask)
		_strlcat(result, "M5-", sizeof(result));
	return estrdup(result);
}

unsigned long 
blitz_strtomod(char *val)
{
	unsigned long   mod = 0;
	if (strstr(val, "S-"))
		mod |= ShiftMask;
	if (strstr(val, "C-"))
		mod |= ControlMask;
	if (strstr(val, "M-"))
		mod |= Mod1Mask;
	if (strstr(val, "M2-"))
		mod |= Mod2Mask;
	if (strstr(val, "M3-"))
		mod |= Mod3Mask;
	if (strstr(val, "WIN-"))
		mod |= Mod4Mask;
	if (strstr(val, "M5-"))
		mod |= Mod5Mask;
	return mod;
}
