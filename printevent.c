/*
 * Original code posted to comp.sources.x
 * Modifications by Russ Cox <rsc@swtch.com>.
 */

/*
Path: uunet!wyse!mikew
From: mikew@wyse.wyse.com (Mike Wexler)
Newsgroups: comp.sources.x
Subject: v02i056:  subroutine to print events in human readable form, Part01/01
Message-ID: <1935@wyse.wyse.com>
Date: 22 Dec 88 19:28:25 GMT
Organization: Wyse Technology, San Jose
Lines: 1093
Approved: mikew@wyse.com

Submitted-by: richsun!darkstar!ken
Posting-number: Volume 2, Issue 56
Archive-name: showevent/part01


There are times during debugging when it would be real useful to be able to
print the fields of an event in a human readable form.  Too many times I found 
myself scrounging around in section 8 of the Xlib manual looking for the valid 
fields for the events I wanted to see, then adding printf's to display the 
numeric values of the fields, and then scanning through X.h trying to decode
the cryptic detail and state fields.  After playing with xev, I decided to
write a couple of standard functions that I could keep in a library and call
on whenever I needed a little debugging verbosity.  The first function,
GetType(), is useful for returning the string representation of the type of
an event.  The second function, ShowEvent(), is used to display all the fields
of an event in a readable format.  The functions are not complicated, in fact,
they are mind-numbingly boring - but that's just the point nobody wants to
spend the time writing functions like this, they just want to have them when
they need them.

A simple, sample program is included which does little else but to demonstrate
the use of these two functions.  These functions have saved me many an hour 
during debugging and I hope you find some benefit to these.  If you have any
comments, suggestions, improvements, or if you find any blithering errors you 
can get it touch with me at the following location:

			ken@richsun.UUCP
*/

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xproto.h>
#include "wmii.h"
#include "printevent.h"

static char* sep = " ";

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

/* Returns the string equivalent of a boolean parameter */
static char*
TorF(int bool)
{
    switch (bool) {
    case True:
	return ("True");

    case False:
	return ("False");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a property notify state */
static char*
PropertyState(int state)
{
    switch (state) {
    case PropertyNewValue:
	return ("PropertyNewValue");

    case PropertyDelete:
	return ("PropertyDelete");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a visibility notify state */
static char*
VisibilityState(int state)
{
    switch (state) {
    case VisibilityUnobscured:
	return ("VisibilityUnobscured");

    case VisibilityPartiallyObscured:
	return ("VisibilityPartiallyObscured");

    case VisibilityFullyObscured:
	return ("VisibilityFullyObscured");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a timestamp */
static char*
ServerTime(Time time)
{
    ulong msec;
    ulong sec;
    ulong min;
    ulong hr;
    ulong day;
    static char buffer[32];

    msec = time % 1000;
    time /= 1000;
    sec = time % 60;
    time /= 60;
    min = time % 60;
    time /= 60;
    hr = time % 24;
    time /= 24;
    day = time;

if(0)
    sprintf(buffer, "%lu day%s %02lu:%02lu:%02lu.%03lu",
      day, day == 1 ? "" : "(s)", hr, min, sec, msec);

    sprintf(buffer, "%lud%luh%lum%lu.%03lds", day, hr, min, sec, msec);
    return (buffer);
}

/* Simple structure to ease the interpretation of masks */
typedef struct MaskType MaskType;
struct MaskType
{
    uint value;
    char *string;
};

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static char*
ButtonAndOrModifierState(uint state)
{
    static char buffer[256];
    static MaskType masks[] = {
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
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++)
	if (state & masks[i].value) {
	    if (first) {
		first = False;
		strcpy(buffer, masks[i].string);
	    } else {
		strcat(buffer, " | ");
		strcat(buffer, masks[i].string);
	    }
	}
    return (buffer);
}

/* Returns the string equivalent of a mask of configure window values */
static char*
ConfigureValueMask(uint valuemask)
{
    static char buffer[256];
    static MaskType masks[] = {
	{CWX, "CWX"},
	{CWY, "CWY"},
	{CWWidth, "CWWidth"},
	{CWHeight, "CWHeight"},
	{CWBorderWidth, "CWBorderWidth"},
	{CWSibling, "CWSibling"},
	{CWStackMode, "CWStackMode"},
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++)
	if (valuemask & masks[i].value) {
	    if (first) {
		first = False;
		strcpy(buffer, masks[i].string);
	    } else {
		strcat(buffer, " | ");
		strcat(buffer, masks[i].string);
	    }
	}

    return (buffer);
}

/* Returns the string equivalent of a motion hint */
static char*
IsHint(char is_hint)
{
    switch (is_hint) {
    case NotifyNormal:
	return ("NotifyNormal");

    case NotifyHint:
	return ("NotifyHint");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of an id or the value "None" */
static char*
MaybeNone(int value)
{
    static char buffer[16];

    if (value == None)
	return ("None");
    else {
	sprintf(buffer, "0x%x", value);
	return (buffer);
    }
}

/* Returns the string equivalent of a colormap state */
static char*
ColormapState(int state)
{
    switch (state) {
    case ColormapInstalled:
	return ("ColormapInstalled");

    case ColormapUninstalled:
	return ("ColormapUninstalled");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a crossing detail */
static char*
CrossingDetail(int detail)
{
    switch (detail) {
    case NotifyAncestor:
	return ("NotifyAncestor");

    case NotifyInferior:
	return ("NotifyInferior");

    case NotifyVirtual:
	return ("NotifyVirtual");

    case NotifyNonlinear:
	return ("NotifyNonlinear");

    case NotifyNonlinearVirtual:
	return ("NotifyNonlinearVirtual");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a focus change detail */
static char*
FocusChangeDetail(int detail)
{
    switch (detail) {
    case NotifyAncestor:
	return ("NotifyAncestor");

    case NotifyInferior:
	return ("NotifyInferior");

    case NotifyVirtual:
	return ("NotifyVirtual");

    case NotifyNonlinear:
	return ("NotifyNonlinear");

    case NotifyNonlinearVirtual:
	return ("NotifyNonlinearVirtual");

    case NotifyPointer:
	return ("NotifyPointer");

    case NotifyPointerRoot:
	return ("NotifyPointerRoot");

    case NotifyDetailNone:
	return ("NotifyDetailNone");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a configure detail */
static char*
ConfigureDetail(int detail)
{
    switch (detail) {
    case Above:
	return ("Above");

    case Below:
	return ("Below");

    case TopIf:
	return ("TopIf");

    case BottomIf:
	return ("BottomIf");

    case Opposite:
	return ("Opposite");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a grab mode */
static char*
GrabMode(int mode)
{
    switch (mode) {
    case NotifyNormal:
	return ("NotifyNormal");

    case NotifyGrab:
	return ("NotifyGrab");

    case NotifyUngrab:
	return ("NotifyUngrab");

    case NotifyWhileGrabbed:
	return ("NotifyWhileGrabbed");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a mapping request */
static char*
MappingRequest(int request)
{
    switch (request) {
    case MappingModifier:
	return ("MappingModifier");

    case MappingKeyboard:
	return ("MappingKeyboard");

    case MappingPointer:
	return ("MappingPointer");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a stacking order place */
static char*
Place(int place)
{
    switch (place) {
    case PlaceOnTop:
	return ("PlaceOnTop");

    case PlaceOnBottom:
	return ("PlaceOnBottom");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a major code */
static char*
MajorCode(int code)
{
    static char buffer[32];

    switch (code) {
    case X_CopyArea:
	return ("X_CopyArea");

    case X_CopyPlane:
	return ("X_CopyPlane");

    default:
	sprintf(buffer, "0x%x", code);
	return (buffer);
    }
}

/* Returns the string equivalent the keycode contained in the key event */
static char*
Keycode(XKeyEvent *ev)
{
    static char buffer[256];
    KeySym keysym_str;
    char *keysym_name;
    char string[256];

    XLookupString(ev, string, 64, &keysym_str, NULL);

    if (keysym_str == NoSymbol)
	keysym_name = "NoSymbol";
    else if (!(keysym_name = XKeysymToString(keysym_str)))
	keysym_name = "(no name)";
    sprintf(buffer, "%u (keysym 0x%x \"%s\")",
      (int)ev->keycode, (int)keysym_str, keysym_name);
    return (buffer);
}

/* Returns the string equivalent of an atom or "None"*/
static char*
AtomName(Display *dpy, Atom atom)
{
    static char buffer[256];
    char *atom_name;

    if (atom == None)
	return ("None");

    atom_name = XGetAtomName(dpy, atom);
    strncpy(buffer, atom_name, 256);
    XFree(atom_name);
    return (buffer);
}

/******************************************************************************/
/**** Routines to print out readable values for the field of various events ***/
/******************************************************************************/

static void
VerbMotion(XMotionEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "root=0x%x%s", (int)ev->root, sep);
    fprintf(stderr, "subwindow=0x%x%s", (int)ev->subwindow, sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    fprintf(stderr, "state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr, "is_hint=%s%s", IsHint(ev->is_hint), sep);
    fprintf(stderr, "same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbButton(XButtonEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "root=0x%x%s", (int)ev->root, sep);
    fprintf(stderr, "subwindow=0x%x%s", (int)ev->subwindow, sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    fprintf(stderr, "state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr, "button=%s%s", ButtonAndOrModifierState(ev->button), sep);
    fprintf(stderr, "same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbColormap(XColormapEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "colormap=%s%s", MaybeNone(ev->colormap), sep);
    fprintf(stderr, "new=%s%s", TorF(ev->new), sep);
    fprintf(stderr, "state=%s\n", ColormapState(ev->state));
}

static void
VerbCrossing(XCrossingEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "root=0x%x%s", (int)ev->root, sep);
    fprintf(stderr, "subwindow=0x%x%s", (int)ev->subwindow, sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    fprintf(stderr, "mode=%s%s", GrabMode(ev->mode), sep);
    fprintf(stderr, "detail=%s%s", CrossingDetail(ev->detail), sep);
    fprintf(stderr, "same_screen=%s%s", TorF(ev->same_screen), sep);
    fprintf(stderr, "focus=%s%s", TorF(ev->focus), sep);
    fprintf(stderr, "state=%s\n", ButtonAndOrModifierState(ev->state));
}

static void
VerbExpose(XExposeEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr, "count=%d\n", ev->count);
}

static void
VerbGraphicsExpose(XGraphicsExposeEvent *ev)
{
    fprintf(stderr, "drawable=0x%x%s", (int)ev->drawable, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr, "major_code=%s%s", MajorCode(ev->major_code), sep);
    fprintf(stderr, "minor_code=%d\n", ev->minor_code);
}

static void
VerbNoExpose(XNoExposeEvent *ev)
{
    fprintf(stderr, "drawable=0x%x%s", (int)ev->drawable, sep);
    fprintf(stderr, "major_code=%s%s", MajorCode(ev->major_code), sep);
    fprintf(stderr, "minor_code=%d\n", ev->minor_code);
}

static void
VerbFocus(XFocusChangeEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "mode=%s%s", GrabMode(ev->mode), sep);
    fprintf(stderr, "detail=%s\n", FocusChangeDetail(ev->detail));
}

static void
VerbKeymap(XKeymapEvent *ev)
{
    int i;

    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "key_vector=");
    for (i = 0; i < 32; i++)
	fprintf(stderr, "%02x", ev->key_vector[i]);
    fprintf(stderr, "\n");
}

static void
VerbKey(XKeyEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "root=0x%x%s", (int)ev->root, sep);
    if(ev->subwindow)
        fprintf(stderr, "subwindow=0x%x%s", (int)ev->subwindow, sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "[%d,%d]%s", ev->x, ev->y, sep);
    fprintf(stderr, "root=[%d,%d]%s", ev->x_root, ev->y_root, sep);
    if(ev->state)
        fprintf(stderr, "state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr, "keycode=%s%s", Keycode(ev), sep);
    if(!ev->same_screen)
        fprintf(stderr, "!same_screen");
    fprintf(stderr, "\n");
    return;

    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "root=0x%x%s", (int)ev->root, sep);
    fprintf(stderr, "subwindow=0x%x%s", (int)ev->subwindow, sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    fprintf(stderr, "state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr, "keycode=%s%s", Keycode(ev), sep);
    fprintf(stderr, "same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbProperty(XPropertyEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "atom=%s%s", AtomName(ev->display, ev->atom), sep);
    fprintf(stderr, "time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr, "state=%s\n", PropertyState(ev->state));
}

static void
VerbResizeRequest(XResizeRequestEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "width=%d height=%d\n", ev->width, ev->height);
}

static void
VerbCirculate(XCirculateEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "place=%s\n", Place(ev->place));
}

static void
VerbConfigure(XConfigureEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr, "border_width=%d%s", ev->border_width, sep);
    fprintf(stderr, "above=%s%s", MaybeNone(ev->above), sep);
    fprintf(stderr, "override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbCreateWindow(XCreateWindowEvent *ev)
{
    fprintf(stderr, "parent=0x%x%s", (int)ev->parent, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr, "border_width=%d%s", ev->border_width, sep);
    fprintf(stderr, "override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbDestroyWindow(XDestroyWindowEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x\n", (int)ev->window);
}

static void
VerbGravity(XGravityEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "x=%d y=%d\n", ev->x, ev->y);
}

static void
VerbMap(XMapEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbReparent(XReparentEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "parent=0x%x%s", (int)ev->parent, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbUnmap(XUnmapEvent *ev)
{
    fprintf(stderr, "event=0x%x%s", (int)ev->event, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "from_configure=%s\n", TorF(ev->from_configure));
}

static void
VerbCirculateRequest(XCirculateRequestEvent *ev)
{
    fprintf(stderr, "parent=0x%x%s", (int)ev->parent, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "place=%s\n", Place(ev->place));
}

static void
VerbConfigureRequest(XConfigureRequestEvent *ev)
{
    fprintf(stderr, "parent=0x%x%s", (int)ev->parent, sep);
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr, "width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr, "border_width=%d%s", ev->border_width, sep);
    fprintf(stderr, "above=%s%s", MaybeNone(ev->above), sep);
    fprintf(stderr, "detail=%s%s", ConfigureDetail(ev->detail), sep);
    fprintf(stderr, "value_mask=%s\n", ConfigureValueMask(ev->value_mask));
}

static void
VerbMapRequest(XMapRequestEvent *ev)
{
    fprintf(stderr, "parent=0x%x%s", (int)ev->parent, sep);
    fprintf(stderr, "window=0x%x\n", (int)ev->window);
}

static void
VerbClient(XClientMessageEvent *ev)
{
    int i;

    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "message_type=%s%s", AtomName(ev->display, ev->message_type), sep);
    fprintf(stderr, "format=%d\n", ev->format);
    fprintf(stderr, "data (shown as longs)=");
    for (i = 0; i < 5; i++)
	fprintf(stderr, " 0x%08lx", ev->data.l[i]);
    fprintf(stderr, "\n");
}

static void
VerbMapping(XMappingEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "request=%s%s", MappingRequest(ev->request), sep);
    fprintf(stderr, "first_keycode=0x%x%s", ev->first_keycode, sep);
    fprintf(stderr, "count=0x%x\n", ev->count);
}

static void
VerbSelectionClear(XSelectionClearEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "selection=%s%s", AtomName(ev->display, ev->selection), sep);
    fprintf(stderr, "time=%s\n", ServerTime(ev->time));
}

static void
VerbSelection(XSelectionEvent *ev)
{
    fprintf(stderr, "requestor=0x%x%s", (int)ev->requestor, sep);
    fprintf(stderr, "selection=%s%s", AtomName(ev->display, ev->selection), sep);
    fprintf(stderr, "target=%s%s", AtomName(ev->display, ev->target), sep);
    fprintf(stderr, "property=%s%s", AtomName(ev->display, ev->property), sep);
    fprintf(stderr, "time=%s\n", ServerTime(ev->time));
}

static void
VerbSelectionRequest(XSelectionRequestEvent *ev)
{
    fprintf(stderr, "owner=0x%x%s", (int)ev->owner, sep);
    fprintf(stderr, "requestor=0x%x%s", (int)ev->requestor, sep);
    fprintf(stderr, "selection=%s%s", AtomName(ev->display, ev->selection), sep);
    fprintf(stderr, "target=%s%s", AtomName(ev->display, ev->target), sep);
    fprintf(stderr, "property=%s%s", AtomName(ev->display, ev->property), sep);
    fprintf(stderr, "time=%s\n", ServerTime(ev->time));
}

static void
VerbVisibility(XVisibilityEvent *ev)
{
    fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
    fprintf(stderr, "state=%s\n", VisibilityState(ev->state));
}

/******************************************************************************/
/************ Return the string representation for type of an event ***********/
/******************************************************************************/

char *eventtype(XEvent *ev)
{
    static char buffer[20];

    switch (ev->type) {
    case KeyPress:
	return ("KeyPress");
    case KeyRelease:
	return ("KeyRelease");
    case ButtonPress:
	return ("ButtonPress");
    case ButtonRelease:
	return ("ButtonRelease");
    case MotionNotify:
	return ("MotionNotify");
    case EnterNotify:
	return ("EnterNotify");
    case LeaveNotify:
	return ("LeaveNotify");
    case FocusIn:
	return ("FocusIn");
    case FocusOut:
	return ("FocusOut");
    case KeymapNotify:
	return ("KeymapNotify");
    case Expose:
	return ("Expose");
    case GraphicsExpose:
	return ("GraphicsExpose");
    case NoExpose:
	return ("NoExpose");
    case VisibilityNotify:
	return ("VisibilityNotify");
    case CreateNotify:
	return ("CreateNotify");
    case DestroyNotify:
	return ("DestroyNotify");
    case UnmapNotify:
	return ("UnmapNotify");
    case MapNotify:
	return ("MapNotify");
    case MapRequest:
	return ("MapRequest");
    case ReparentNotify:
	return ("ReparentNotify");
    case ConfigureNotify:
	return ("ConfigureNotify");
    case ConfigureRequest:
	return ("ConfigureRequest");
    case GravityNotify:
	return ("GravityNotify");
    case ResizeRequest:
	return ("ResizeRequest");
    case CirculateNotify:
	return ("CirculateNotify");
    case CirculateRequest:
	return ("CirculateRequest");
    case PropertyNotify:
	return ("PropertyNotify");
    case SelectionClear:
	return ("SelectionClear");
    case SelectionRequest:
	return ("SelectionRequest");
    case SelectionNotify:
	return ("SelectionNotify");
    case ColormapNotify:
	return ("ColormapNotify");
    case ClientMessage:
	return ("ClientMessage");
    case MappingNotify:
	return ("MappingNotify");
    }
    sprintf(buffer, "%d", ev->type);
    return buffer;
}

/******************************************************************************/
/**************** Print the values of all fields for any event ****************/
/******************************************************************************/

void printevent(XEvent *e)
{
    XAnyEvent *ev = (void*)e;
    char *name;

    if(ev->window) {
	    XFetchName(blz.dpy, ev->window, &name);
	    if(name) {
		    fprintf(stderr, "\ttitle=%s\n", name);
		    XFree(name);
	    }
    }
    fprintf(stderr, "%3ld %-20s ", ev->serial, eventtype(e));
    if(ev->send_event)
        fprintf(stderr, "(sendevent) ");
    if(0){
        fprintf(stderr, "type=%s%s", eventtype(e), sep);
        fprintf(stderr, "serial=%lu%s", ev->serial, sep);
        fprintf(stderr, "send_event=%s%s", TorF(ev->send_event), sep);
        fprintf(stderr, "display=0x%p%s", ev->display, sep);
    }

    switch (ev->type) {
    case MotionNotify:
	VerbMotion((void*)ev);
	break;

    case ButtonPress:
    case ButtonRelease:
	VerbButton((void*)ev);
	break;

    case ColormapNotify:
	VerbColormap((void*)ev);
	break;

    case EnterNotify:
    case LeaveNotify:
	VerbCrossing((void*)ev);
	break;

    case Expose:
	VerbExpose((void*)ev);
	break;

    case GraphicsExpose:
	VerbGraphicsExpose((void*)ev);
	break;

    case NoExpose:
	VerbNoExpose((void*)ev);
	break;

    case FocusIn:
    case FocusOut:
	VerbFocus((void*)ev);
	break;

    case KeymapNotify:
	VerbKeymap((void*)ev);
	break;

    case KeyPress:
    case KeyRelease:
	VerbKey((void*)ev);
	break;

    case PropertyNotify:
	VerbProperty((void*)ev);
	break;

    case ResizeRequest:
	VerbResizeRequest((void*)ev);
	break;

    case CirculateNotify:
	VerbCirculate((void*)ev);
	break;

    case ConfigureNotify:
	VerbConfigure((void*)ev);
	break;

    case CreateNotify:
	VerbCreateWindow((void*)ev);
	break;

    case DestroyNotify:
	VerbDestroyWindow((void*)ev);
	break;

    case GravityNotify:
	VerbGravity((void*)ev);
	break;

    case MapNotify:
	VerbMap((void*)ev);
	break;

    case ReparentNotify:
	VerbReparent((void*)ev);
	break;

    case UnmapNotify:
	VerbUnmap((void*)ev);
	break;

    case CirculateRequest:
	VerbCirculateRequest((void*)ev);
	break;

    case ConfigureRequest:
	VerbConfigureRequest((void*)ev);
	break;

    case MapRequest:
	VerbMapRequest((void*)ev);
	break;

    case ClientMessage:
	VerbClient((void*)ev);
	break;

    case MappingNotify:
	VerbMapping((void*)ev);
	break;

    case SelectionClear:
	VerbSelectionClear((void*)ev);
	break;

    case SelectionNotify:
	VerbSelection((void*)ev);
	break;

    case SelectionRequest:
	VerbSelectionRequest((void*)ev);
	break;

    case VisibilityNotify:
	VerbVisibility((void*)ev);
	break;
    }
}

