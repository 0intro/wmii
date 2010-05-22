#include <stuff/x.h>

typedef void (*EventHandler)(XEvent*);

#define handle(w, fn, ev) \
	BLOCK(if((w)->handler->fn) (w)->handler->fn((w), ev))

extern EventHandler event_handler[LASTEvent];

