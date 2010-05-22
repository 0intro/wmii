#include <stuff/base.h>
#include <stuff/x11.h>
#include <fmt.h>

extern Visual*	render_visual;

extern void	init_screens(void);

/* printevent.c */
int	fmtevent(Fmt*);

/* xext.c */
void	randr_event(XEvent*);
bool	render_argb_p(Visual*);
void	xext_event(XEvent*);
void	xext_init(void);
Rectangle*	xinerama_screens(int*);

void	event_check(void);
void	event_dispatch(XEvent*);
uint	event_flush(long, bool dispatch);
uint	event_flushenter(void);
void	event_loop(void);
void	event_updatextime(void);

void	event_buttonpress(XButtonPressedEvent*);
void	event_buttonrelease(XButtonPressedEvent*);
void	event_clientmessage(XClientMessageEvent*);
void	event_configurenotify(XConfigureEvent*);
void	event_configurerequest(XConfigureRequestEvent*);
void	event_destroynotify(XDestroyWindowEvent*);
void	event_enternotify(XCrossingEvent*);
void	event_expose(XExposeEvent*);
void	event_focusin(XFocusChangeEvent*);
void	event_focusout(XFocusChangeEvent*);
void	event_keypress(XKeyEvent*);
void	event_leavenotify(XCrossingEvent*);
void	event_mapnotify(XMapEvent*);
void	event_maprequest(XMapRequestEvent*);
void	event_mappingnotify(XMappingEvent*);
void	event_motionnotify(XMotionEvent*);
void	event_propertynotify(XPropertyEvent*);
void	event_unmapnotify(XUnmapEvent*);

extern long	event_xtime;
extern bool	event_looprunning;
extern void	(*event_debug)(XEvent*);

