/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "util.h"
#include <stuff/x11.h>


char**
comm(int cols, char **toka, char **tokb) {
	Vector_ptr vec;
	char **ret;
	int cmp;

	vector_pinit(&vec);
	while(*toka || *tokb) {
		if(!*toka)
			cmp = 1;
		else if(!*tokb)
			cmp = -1;
		else
			cmp = strcmp(*toka, *tokb);
		if(cmp < 0) {
			if(cols & CLeft)
				vector_ppush(&vec, *toka);
			toka++;
		}else if(cmp > 0) {
			if(cols & CRight)
				vector_ppush(&vec, *tokb);
			tokb++;
		}else {
			if(cols & CCenter)
				vector_ppush(&vec, *toka);
			toka++;
			tokb++;
		}
	}
	vector_ppush(&vec, nil);
	ret = strlistdup((char**)vec.ary);
	free(vec.ary);
	return ret;
}
