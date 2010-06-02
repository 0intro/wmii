/*
 * Original code posted to comp.sources.x
 * Modifications by Russ Cox <rsc@swtch.com>.
 * Further modifications by Kris Maglione <maglione.k at Gmail>
 */

/*
 * Path: uunet!wyse!mikew From: mikew@wyse.wyse.com (Mike Wexler) Newsgroups:
 * comp.sources.x Subject: v02i056:  subroutine to print events in human
 * readable form, Part01/01 Message-ID: <1935@wyse.wyse.com> Date: 22 Dec 88
 * 19:28:25 GMT Organization: Wyse Technology, San Jose Lines: 1093 Approved:
 * mikew@wyse.com
 * 
 * Submitted-by: richsun!darkstar!ken Posting-number: Volume 2, Issue 56
 * Archive-name: showevent/part01
 * 
 * 
 * There are times during debugging when it would be real useful to be able to
 * print the fields of an event in a human readable form.  Too many times I
 * found myself scrounging around in section 8 of the Xlib manual looking for
 * the valid fields for the events I wanted to see, then adding printf's to
 * display the numeric values of the fields, and then scanning through X.h
 * trying to decode the cryptic detail and state fields.  After playing with
 * xev, I decided to write a couple of standard functions that I could keep
 * in a library and call on whenever I needed a little debugging verbosity.
 * The first function, GetType(), is useful for returning the string
 * representation of the type of an event.  The second function, ShowEvent(),
 * is used to display all the fields of an event in a readable format.  The
 * functions are not complicated, in fact, they are mind-numbingly boring -
 * but that's just the point nobody wants to spend the time writing functions
 * like this, they just want to have them when they need them.
 * 
 * A simple, sample program is included which does little else but to
 * demonstrate the use of these two functions.  These functions have saved me
 * many an hour during debugging and I hope you find some benefit to these.
 * If you have any comments, suggestions, improvements, or if you find any
 * blithering errors you can get it touch with me at the following location:
 * 
 * ken@richsun.UUCP
 */

#include <stdarg.h>
#include <bio.h>
#include <stuff/x.h>
#include <stuff/util.h>
#include "printevent.h"
#define Window XWindow

#define nil ((void*)0)

typedef struct Pair Pair;

struct Pair {
	int key;
	char *val;
};

static char* sep = " ";

static char *
search(Pair *lst, int key, char *(*def)(int)) {
	for(; lst->val; lst++)
		if(lst->key == key)
			return lst->val;
	return def(key);
}

static char*
unmask(Pair *list, uint val)
{
	Pair  *p;
	char *s, *end;
	int n;

	buffer[0] = '\0';
	end = buffer + sizeof buffer;
	s = buffer;

	n = 0;
	s = utfecpy(s, end, "(");
	for (p = list; p->val; p++)
		if (val & p->key) {
			if(n++)
				s = utfecpy(s, end, "|");
			s = utfecpy(s, end, p->val);
		}
	utfecpy(s, end, ")");

	return buffer;
}

static char *
strhex(int key) {
	sprint(buffer, "0x%x", key);
	return buffer;
}

static char *
strdec(int key) {
	sprint(buffer, "%d", key);
	return buffer;
}

static char *
strign(int key) {
	USED(key);

	return "?";
}

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

static void
TInt(Fmt *b, va_list *ap) {
	fmtprint(b, "%d", va_arg(*ap, int));
}

static void
TWindow(Fmt *b, va_list *ap) {
	Window w;

	w = va_arg(*ap, Window);
	fmtprint(b, "0x%ux", (uint)w);
}

static void
TData(Fmt *b, va_list *ap) {
	long *l;
	int i;

	l = va_arg(*ap, long*);
	fmtprint(b, "{");
	for (i = 0; i < 5; i++) {
		if(i > 0)
			fmtprint(b, ", ");
		fmtprint(b, "0x%08lx", l[i]);
	}
	fmtprint(b, "}");
}

/* Returns the string equivalent of a timestamp */
static void
TTime(Fmt *b, va_list *ap) {
	ldiv_t	d;
	ulong   msec;
	ulong   sec;
	ulong   min;
	ulong   hr;
	ulong   day;
	Time time;

	time = va_arg(*ap, Time);

	msec = time/1000;
	d = ldiv(msec, 60);
	msec = time-msec*1000;

	sec = d.rem;
	d = ldiv(d.quot, 60);
	min = d.rem;
	d = ldiv(d.quot, 24);
	hr = d.rem;
	day = d.quot;

#ifdef notdef
	sprintf(buffer, "%lu day%s %02lu:%02lu:%02lu.%03lu",
		day, day == 1 ? "" : "(s)", hr, min, sec, msec);
#endif

	fmtprint(b, "%ludd_%ludh_%ludm_%lud.%03luds", day, hr, min, sec, msec);
}

/* Returns the string equivalent of a boolean parameter */
static void
TBool(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{True, "True"},
		{False, "False"},
		{0, nil},
	};
	Bool key;

	key = va_arg(*ap, Bool);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a property notify state */
static void
TPropState(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{PropertyNewValue, "PropertyNewValue"},
		{PropertyDelete, "PropertyDelete"},
		{0, nil},
	};
	uint key;

	key = va_arg(*ap, uint);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a visibility notify state */
static void
TVis(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{VisibilityUnobscured, "VisibilityUnobscured"},
		{VisibilityPartiallyObscured, "VisibilityPartiallyObscured"},
		{VisibilityFullyObscured, "VisibilityFullyObscured"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static void
TModState(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{Button1Mask, "Button1Mask"},
		{Button2Mask, "Button2Mask"},
		{Button3Mask, "Button3Mask"},
		{Button4Mask, "Button4Mask"},
		{Button5Mask, "Button5Mask"},
		{ShiftMask, "ShiftMask"},
		{LockMask, "LockMask"},
		{ControlMask, "ControlMask"},
		{Mod1Mask, "Mod1Mask"},
		{Mod2Mask, "Mod2Mask"},
		{Mod3Mask, "Mod3Mask"},
		{Mod4Mask, "Mod4Mask"},
		{Mod5Mask, "Mod5Mask"},
		{0, nil},
	};
	uint state;

	state = va_arg(*ap, uint);
	fmtprint(b, "%s", unmask(list, state));
}

/* Returns the string equivalent of a mask of configure window values */
static void
TConfMask(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{CWX, "CWX"},
		{CWY, "CWY"},
		{CWWidth, "CWWidth"},
		{CWHeight, "CWHeight"},
		{CWBorderWidth, "CWBorderWidth"},
		{CWSibling, "CWSibling"},
		{CWStackMode, "CWStackMode"},
		{0, nil},
	};
	uint valuemask;

	valuemask = va_arg(*ap, uint);
	fmtprint(b, "%s", unmask(list, valuemask));
}

/* Returns the string equivalent of a motion hint */
#if 0
static void
IsHint(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyHint, "NotifyHint"},
		{0, nil},
	};
	char key;

	key = va_arg(*ap, char);
	fmtprint(b, "%s", search(list, key, strign));
}
#endif

/* Returns the string equivalent of an id or the value "None" */
static void
TIntNone(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{None, "None"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strhex));
}

/* Returns the string equivalent of a colormap state */
static void
TColMap(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{ColormapInstalled, "ColormapInstalled"},
		{ColormapUninstalled, "ColormapUninstalled"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a crossing detail */
static void
TXing(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{NotifyAncestor, "NotifyAncestor"},
		{NotifyInferior, "NotifyInferior"},
		{NotifyVirtual, "NotifyVirtual"},
		{NotifyNonlinear, "NotifyNonlinear"},
		{NotifyNonlinearVirtual, "NotifyNonlinearVirtual"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a focus change detail */
static void
TFocus(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{NotifyAncestor, "NotifyAncestor"},
		{NotifyInferior, "NotifyInferior"},
		{NotifyVirtual, "NotifyVirtual"},
		{NotifyNonlinear, "NotifyNonlinear"},
		{NotifyNonlinearVirtual, "NotifyNonlinearVirtual"},
		{NotifyPointer, "NotifyPointer"},
		{NotifyPointerRoot, "NotifyPointerRoot"},
		{NotifyDetailNone, "NotifyDetailNone"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a configure detail */
static void
TConfDetail(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{Above, "Above"},
		{Below, "Below"},
		{TopIf, "TopIf"},
		{BottomIf, "BottomIf"},
		{Opposite, "Opposite"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a grab mode */
static void
TGrabMode(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyGrab, "NotifyGrab"},
		{NotifyUngrab, "NotifyUngrab"},
		{NotifyWhileGrabbed, "NotifyWhileGrabbed"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a mapping request */
static void
TMapping(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{MappingModifier, "MappingModifier"},
		{MappingKeyboard, "MappingKeyboard"},
		{MappingPointer, "MappingPointer"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a stacking order place */
static void
TPlace(Fmt *b, va_list *ap) {
	static Pair list[] = {
		{PlaceOnTop, "PlaceOnTop"},
		{PlaceOnBottom, "PlaceOnBottom"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	fmtprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a major code */
static void
TMajor(Fmt *b, va_list *ap) {
	static char *list[] = { XMajors };
	char *s;
	uint key;

	key = va_arg(*ap, uint);
	s = "<nil>";
	if(key < nelem(list))
		s = list[key];
	fmtprint(b, "%s", s);
}

static char*
eventtype(int key) {
	static Pair list[] = {
		{ButtonPress, "ButtonPress"},
		{ButtonRelease, "ButtonRelease"},
		{CirculateNotify, "CirculateNotify"},
		{CirculateRequest, "CirculateRequest"},
		{ClientMessage, "ClientMessage"},
		{ColormapNotify, "ColormapNotify"},
		{ConfigureNotify, "ConfigureNotify"},
		{ConfigureRequest, "ConfigureRequest"},
		{CreateNotify, "CreateNotify"},
		{DestroyNotify, "DestroyNotify"},
		{EnterNotify, "EnterNotify"},
		{Expose, "Expose"},
		{FocusIn, "FocusIn"},
		{FocusOut, "FocusOut"},
		{GraphicsExpose, "GraphicsExpose"},
		{GravityNotify, "GravityNotify"},
		{KeyPress, "KeyPress"},
		{KeyRelease, "KeyRelease"},
		{KeymapNotify, "KeymapNotify"},
		{LeaveNotify, "LeaveNotify"},
		{MapNotify, "MapNotify"},
		{MapRequest, "MapRequest"},
		{MappingNotify, "MappingNotify"},
		{MotionNotify, "MotionNotify"},
		{NoExpose, "NoExpose"},
		{PropertyNotify, "PropertyNotify"},
		{ReparentNotify, "ReparentNotify"},
		{ResizeRequest, "ResizeRequest"},
		{SelectionClear, "SelectionClear"},
		{SelectionNotify, "SelectionNotify"},
		{SelectionRequest, "SelectionRequest"},
		{UnmapNotify, "UnmapNotify"},
		{VisibilityNotify, "VisibilityNotify"},
		{0, nil},
	};
	return search(list, key, strdec);
}
/* Returns the string equivalent the keycode contained in the key event */
static void
TKeycode(Fmt *b, va_list *ap) {
	KeySym keysym_str;
	XKeyEvent *ev;
	char *keysym_name;

	ev = va_arg(*ap, XKeyEvent*);

	XLookupString(ev, buffer, sizeof buffer, &keysym_str, nil);

	if (keysym_str == NoSymbol)
		keysym_name = "NoSymbol";
	else
		keysym_name = XKeysymToString(keysym_str);
	if(keysym_name == nil)
		keysym_name = "(no name)";

	fmtprint(b, "%ud (keysym 0x%x \"%s\")", (int)ev->keycode,
			(int)keysym_str, keysym_name);
}

/* Returns the string equivalent of an atom or "None" */
static void
TAtom(Fmt *b, va_list *ap) {

	fmtstrcpy(b, atomname(va_arg(*ap, Atom)));
}

#define _(m) #m, ev->m
#define TEnd nil
typedef void (*Tfn)(Fmt*, va_list*);

static int
pevent(Fmt *fmt, void *e, ...) {
	va_list ap;
	Tfn fn;
	XAnyEvent *ev;
	char *key;
	int n;

	ev = e;
	fmtprint(fmt, "%3ld %-20s ", ev->serial, eventtype(ev->type));
	if(ev->send_event)
		fmtstrcpy(fmt, "(sendevent) ");

	n = 0;
	va_start(ap, e);
	for(;;) {
		fn = va_arg(ap, Tfn);
		if(fn == TEnd)
			break;

		if(n++ != 0)
			fmtprint(fmt, "%s", sep);

		key = va_arg(ap, char*);
		fmtprint(fmt, "%s=", key);
		fn(fmt, &ap);
	}
	va_end(ap);
	return 0;
}

/*****************************************************************************/
/*** Routines to print out readable values for the field of various events ***/
/*****************************************************************************/

static int
VerbMotion(Fmt *fmt, XEvent *e) {
	XMotionEvent *ev = &e->xmotion;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TBool, _(same_screen),
		TEnd
	);
    //fprintf(stderr, "is_hint=%s%s", IsHint(ev->is_hint), sep);
}

static int
VerbButton(Fmt *fmt, XEvent *e) {
	XButtonEvent *ev = &e->xbutton;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TInt, _(button),
		TBool, _(same_screen),
		TEnd
	);
}

static int
VerbColormap(Fmt *fmt, XEvent *e) {
	XColormapEvent *ev = &e->xcolormap;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TIntNone, _(colormap),
		TBool, _(new),
		TColMap, _(state),
		TEnd
	);
}

static int
VerbCrossing(Fmt *fmt, XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TGrabMode, _(mode),
		TXing, _(detail),
		TBool, _(same_screen),
		TBool, _(focus),
		TModState, _(state),
		TEnd
	);
}

static int
VerbExpose(Fmt *fmt, XEvent *e) {
	XExposeEvent *ev = &e->xexpose;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(count),
		TEnd
	);
}

static int
VerbGraphicsExpose(Fmt *fmt, XEvent *e) {
	XGraphicsExposeEvent *ev = &e->xgraphicsexpose;

	return 	pevent(fmt, ev,
		TWindow, _(drawable),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TMajor, _(major_code),
		TInt, _(minor_code),
		TEnd
	);
}

static int
VerbNoExpose(Fmt *fmt, XEvent *e) {
	XNoExposeEvent *ev = &e->xnoexpose;

	return 	pevent(fmt, ev,
		TWindow, _(drawable),
		TMajor, _(major_code),
		TInt, _(minor_code),
		TEnd
	);
}

static int
VerbFocus(Fmt *fmt, XEvent *e) {
	XFocusChangeEvent *ev = &e->xfocus;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TGrabMode, _(mode),
		TFocus, _(detail),
		TEnd
	);
}

static int
VerbKeymap(Fmt *fmt, XEvent *e) {
	XKeymapEvent *ev = &e->xkeymap;
	int i;

	fmtprint(fmt, "window=0x%x%s", (int)ev->window, sep);
	fmtprint(fmt, "key_vector=");
	for (i = 0; i < 32; i++)
		fmtprint(fmt, "%02x", ev->key_vector[i]);
	fmtprint(fmt, "\n");
	return 0;
}

static int
VerbKey(Fmt *fmt, XEvent *e) {
	XKeyEvent *ev = &e->xkey;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TKeycode, "keycode", ev,
		TBool, _(same_screen),
		TEnd
	);
}

static int
VerbProperty(Fmt *fmt, XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TAtom, _(atom),
		TTime, _(time),
		TPropState, _(state),
		TEnd
	);
}

static int
VerbResizeRequest(Fmt *fmt, XEvent *e) {
	XResizeRequestEvent *ev = &e->xresizerequest;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TInt, _(width), TInt, _(height),
		TEnd
	);
}

static int
VerbCirculate(Fmt *fmt, XEvent *e) {
	XCirculateEvent *ev = &e->xcirculate;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static int
VerbConfigure(Fmt *fmt, XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TIntNone, _(above),
		TBool, _(override_redirect),
		TEnd
	);
}

static int
VerbCreateWindow(Fmt *fmt, XEvent *e) {
	XCreateWindowEvent *ev = &e->xcreatewindow;

	return 	pevent(fmt, ev,
		TWindow, _(parent),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TBool, _(override_redirect),
		TEnd
	);
}

static int
VerbDestroyWindow(Fmt *fmt, XEvent *e) {
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TEnd
	);
}

static int
VerbGravity(Fmt *fmt, XEvent *e) {
	XGravityEvent *ev = &e->xgravity;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TEnd
	);
}

static int
VerbMap(Fmt *fmt, XEvent *e) {
	XMapEvent *ev = &e->xmap;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(override_redirect),
		TEnd
	);
}

static int
VerbReparent(Fmt *fmt, XEvent *e) {
	XReparentEvent *ev = &e->xreparent;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TWindow, _(parent),
		TInt, _(x), TInt, _(y),
		TBool, _(override_redirect),
		TEnd
	);
}

static int
VerbUnmap(Fmt *fmt, XEvent *e) {
	XUnmapEvent *ev = &e->xunmap;

	return 	pevent(fmt, ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(from_configure),
		TEnd
	);
}

static int
VerbCirculateRequest(Fmt *fmt, XEvent *e) {
	XCirculateRequestEvent *ev = &e->xcirculaterequest;

	return 	pevent(fmt, ev,
		TWindow, _(parent),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static int
VerbConfigureRequest(Fmt *fmt, XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

	return 	pevent(fmt, ev,
		TWindow, _(parent),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TIntNone, _(above),
		TConfDetail, _(detail),
		TConfMask, _(value_mask),
		TEnd
	);
}

static int
VerbMapRequest(Fmt *fmt, XEvent *e) {
	XMapRequestEvent *ev = &e->xmaprequest;

	return 	pevent(fmt, ev,
		TWindow, _(parent),
		TWindow, _(window),
		TEnd
	);
}

static int
VerbClient(Fmt *fmt, XEvent *e) {
	XClientMessageEvent *ev = &e->xclient;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TAtom, _(message_type),
		TInt, _(format),
		TData, "data (as longs)", &ev->data,
		TEnd
	);
}

static int
VerbMapping(Fmt *fmt, XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TMapping, _(request),
		TWindow, _(first_keycode),
		TWindow, _(count),
		TEnd
	);
}

static int
VerbSelectionClear(Fmt *fmt, XEvent *e) {
	XSelectionClearEvent *ev = &e->xselectionclear;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TAtom, _(selection),
		TTime, _(time),
		TEnd
	);
}

static int
VerbSelection(Fmt *fmt, XEvent *e) {
	XSelectionEvent *ev = &e->xselection;

	return 	pevent(fmt, ev,
		TWindow, _(requestor),
		TAtom, _(selection),
		TAtom, _(target),
		TAtom, _(property),
		TTime, _(time),
		TEnd
	);
}

static int
VerbSelectionRequest(Fmt *fmt, XEvent *e) {
	XSelectionRequestEvent *ev = &e->xselectionrequest;

	return 	pevent(fmt, ev,
		TWindow, _(owner),
		TWindow, _(requestor),
		TAtom, _(selection),
		TAtom, _(target),
		TAtom, _(property),
		TTime, _(time),
		TEnd
	);
}

static int
VerbVisibility(Fmt *fmt, XEvent *e) {
	XVisibilityEvent *ev = &e->xvisibility;

	return 	pevent(fmt, ev,
		TWindow, _(window),
		TVis, _(state),
		TEnd
	);
}

/******************************************************************************/
/**************** Print the values of all fields for any event ****************/
/******************************************************************************/

typedef struct Handler Handler;
struct Handler {
	int key;
	int (*fn)(Fmt*, XEvent*);
};

int 
fmtevent(Fmt *fmt) {
	XEvent *e;
	XAnyEvent *ev;
	/*
		fprintf(stderr, "type=%s%s", eventtype(e->xany.type), sep);
		fprintf(stderr, "serial=%lu%s", ev->serial, sep);
		fprintf(stderr, "send_event=%s%s", TorF(ev->send_event), sep);
		fprintf(stderr, "display=0x%p%s", ev->display, sep);
	*/
	static Handler fns[] = {
		{MotionNotify, VerbMotion},
		{ButtonPress, VerbButton},
		{ButtonRelease, VerbButton},
		{ColormapNotify, VerbColormap},
		{EnterNotify, VerbCrossing},
		{LeaveNotify, VerbCrossing},
		{Expose, VerbExpose},
		{GraphicsExpose, VerbGraphicsExpose},
		{NoExpose, VerbNoExpose},
		{FocusIn, VerbFocus},
		{FocusOut, VerbFocus},
		{KeymapNotify, VerbKeymap},
		{KeyPress, VerbKey},
		{KeyRelease, VerbKey},
		{PropertyNotify, VerbProperty},
		{ResizeRequest, VerbResizeRequest},
		{CirculateNotify, VerbCirculate},
		{ConfigureNotify, VerbConfigure},
		{CreateNotify, VerbCreateWindow},
		{DestroyNotify, VerbDestroyWindow},
		{GravityNotify, VerbGravity},
		{MapNotify, VerbMap},
		{ReparentNotify, VerbReparent},
		{UnmapNotify, VerbUnmap},
		{CirculateRequest, VerbCirculateRequest},
		{ConfigureRequest, VerbConfigureRequest},
		{MapRequest, VerbMapRequest},
		{ClientMessage, VerbClient},
		{MappingNotify, VerbMapping},
		{SelectionClear, VerbSelectionClear},
		{SelectionNotify, VerbSelection},
		{SelectionRequest, VerbSelectionRequest},
		{VisibilityNotify, VerbVisibility},
		{0, nil},
	};
	Handler *p;

	e = va_arg(fmt->args, XEvent*);
	ev = &e->xany;

	for (p = fns; p->fn; p++)
		if (p->key == ev->type)
			return p->fn(fmt, e);
	return 1;
}

