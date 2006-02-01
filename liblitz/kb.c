/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "blitz.h"

unsigned long blitz_strtomod(char *val)
{
	unsigned long mod = 0;
	if (strstr(val, "Shift"))
		mod |= ShiftMask;
	if (strstr(val, "Control"))
		mod |= ControlMask;
	if (strstr(val, "Mod1") || strstr(val, "Alt"))
		mod |= Mod1Mask;
	if (strstr(val, "Mod2"))
		mod |= Mod2Mask;
	if (strstr(val, "Mod3"))
		mod |= Mod3Mask;
	if (strstr(val, "Mod4"))
		mod |= Mod4Mask;
	if (strstr(val, "Mod5"))
		mod |= Mod5Mask;
	return mod;
}
