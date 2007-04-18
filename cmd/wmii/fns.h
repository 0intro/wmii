/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

/* wm.c */
char *message_root(char *message);

/* area.c */
Area *create_area(View*, Area *pos, uint w);
void destroy_area(Area*);
Area *area_of_id(View*, ushort id);
void focus_area(Area*);
char *select_area(Area*, char *arg);
void send_to_area(Area*, Frame*);
void attach_to_area(Area*, Frame*, Bool send);
void detach_from_area(Frame*);
Client *area_selclient(Area*);

/* bar.c */
void initbar(WMScreen *s);
Bar *create_bar(Bar **b_link, char *name);
void destroy_bar(Bar **b_link, Bar*);
void draw_bar(WMScreen *s);
void resize_bar();
Bar *bar_of_name(Bar *b_link, const char *name);

/* client.c */
Client *create_client(XWindow, XWindowAttributes*);
void destroy_client(Client*);
void configure_client(Client*);
void prop_client(Client *c, Atom);
void kill_client(Client*);
void gravitate_client(Client*, Bool invert);
void map_client(Client*);
void unmap_client(Client*, int state);
int map_frame(Client*);
int unmap_frame(Client*);
void set_cursor(Client*, Cursor cur);
void focus_frame(Frame*, Bool restack);
void reparent_client(Client*, Window*, Point);
void manage_client(Client*);
void focus(Client*, Bool restack);
void focus_client(Client*);
void resize_client(Client*, Rectangle*);
void apply_sizehints(Client*, Rectangle*, Bool floating, Bool frame, Align sticky);
char *send_client(Frame*, char*, Bool swap);
char * message_client(Client*, char*);
void move_client(Client*, char *arg);
void size_client(Client*, char *arg);
Client *selclient();
Client *win2client(XWindow);
void apply_rules(Client*);
void apply_tags(Client*, const char*);

/* column.c */
void update_divs();
void draw_div(Divide*);
void setdiv(Divide*, int x);
void arrange_column(Area*, Bool dirty);
void resize_column(Area*, int w);
void resize_colframe(Frame*, Rectangle*);
int str2colmode(const char *str);
Area *new_column(View*, Area *pos, uint w);

/* event.c */
void dispatch_event(XEvent*);
void check_x_event(IxpConn*);
uint flushevents(long even_mask, Bool dispatch);

/* frame.c */
Frame *create_frame(Client*, View*);
void remove_frame(Frame*);
void insert_frame(Frame *pos, Frame*, Bool before);
void resize_frame(Frame*, Rectangle);
Bool frame_to_top(Frame *f);
void set_frame_cursor(Frame*, Point);
void swap_frames(Frame*, Frame*);
int frame_delta_h();
Rectangle frame2client(Frame*, Rectangle);
Rectangle client2frame(Frame*, Rectangle);
int ingrabbox(Frame*, int x, int y);
void draw_frame(Frame*);
void draw_frames();
void update_frame_widget_colors(Frame*);
Rectangle constrain(Rectangle);

/* fs.c */
void fs_attach(Ixp9Req*);
void fs_clunk(Ixp9Req*);
void fs_create(Ixp9Req*);
void fs_flush(Ixp9Req*);
void fs_freefid(Fid*);
void fs_open(Ixp9Req*);
void fs_read(Ixp9Req*);
void fs_remove(Ixp9Req*);
void fs_stat(Ixp9Req*);
void fs_walk(Ixp9Req*);
void fs_write(Ixp9Req*);
void write_event(char*, ...);

/* geom.c */
Bool ptinrect(Point, Rectangle);
Align quadrant(Rectangle, Point);
Cursor cursor_of_quad(Align);
Align get_sticky(Rectangle src, Rectangle dst);

/* key.c */
void kpress(XWindow, ulong mod, KeyCode);
void update_keys();
void init_lock_keys();
ulong mod_key_of_str(char*);

/* mouse.c */
void mouse_resizecol(Divide*);
void do_mouse_resize(Client*, Bool opaque, Align);
void grab_mouse(XWindow, ulong mod, ulong button);
void ungrab_mouse(XWindow, ulong mod, uint button);
Align snap_rect(Rectangle *rects, int num, Rectangle *current,
					 Align *mask, int snapw);
void grab_button(XWindow, uint button, ulong mod);

/* rule.c */
void update_rules(Rule**, const char*);
void trim(char *str, const char *chars);

/* view.c */
void arrange_view(View*);
void scale_view(View*, int w);
View *get_view(const char*);
View *create_view(const char*);
void focus_view(WMScreen*, View*);
void update_client_views(Client*, char**);
Rectangle *rects_of_view(View*, uint *num, Frame *ignore);
View *view_of_id(ushort);
void select_view(const char*);
void attach_to_view(View*, Frame*);
Client *view_selclient(View*);
char *message_view(View*, char*);
void restack_view(View*);
uchar *view_index(View*);
void destroy_view(View*);
void update_views();
uint newcolw(View*, int i);

/* wm.c */
int win_proto(Window);

/* x11.c */
XRectangle XRect(Rectangle);
int eqrect(Rectangle, Rectangle);
Point addpt(Point, Point);
Point subpt(Point, Point);
Point divpt(Point, Point);
Rectangle insetrect(Rectangle, int);
Rectangle rectaddpt(Rectangle, Point);
Rectangle rectsubpt(Rectangle, Point);
void initdisplay();
Image * allocimage(int w, int h, int depth);
void freeimage(Image *);
Window *createwindow(Window *parent, Rectangle, int depth, uint class, WinAttr*, int valuemask);
void destroywindow(Window*);
void setwinattr(Window*, WinAttr*, int valmask);
void reshapewin(Window*, Rectangle);
void movewin(Window*, Point);
int mapwin(Window*);
int unmapwin(Window*);
void lowerwin(Window*);
void raisewin(Window*);
Handlers* sethandler(Window*, Handlers*);
Window* findwin(XWindow);
uint winprotocols(Window*);
void setshapemask(Window *dst, Image *src, Point);
void border(Image *dst, Rectangle, int w, ulong col);
void fill(Image *dst, Rectangle, ulong col);
void drawpoly(Image *dst, Point *pt, int np, int cap, int w, ulong col);
void fillpoly(Image *dst, Point *pt, int np, ulong col);
void drawline(Image *dst, Point p1, Point p2, int cap, int w, ulong col);
void drawstring(Image *dst, Font *font, Rectangle, Align align, char *text, ulong col);
void copyimage(Image *dst, Rectangle, Image *src, Point p);
Bool namedcolor(char *name, ulong*);
Bool loadcolor(CTuple*, char*);
Font * loadfont(char*);
void freefont(Font*);
uint textwidth_l(Font*, char*, uint len);
uint textwidth(Font*, char*);
uint labelh(Font*);
Atom xatom(char*);
Point querypointer(Window*);
void warppointer(Point);
Point translate(Window*, Window*, Point);
int grabpointer(Window*, Window *confine, Cursor cur, int mask);
void ungrabpointer();

/* utf.c */
int	chartorune(Rune*, char*);
int	fullrune(char*, int n);
int	runelen(long);
int	runenlen(Rune*, int nrune);
Rune*	runestrcat(Rune*, Rune*);
Rune*	runestrchr(Rune*, Rune);
int	runestrcmp(Rune*, Rune*);
Rune*	runestrcpy(Rune*, Rune*);
Rune*	runestrdup(Rune*) ;
Rune*	runestrecpy(Rune*, Rune *end, Rune*);
long	runestrlen(Rune*);
Rune*	runestrncat(Rune*, Rune*, long);
int	runestrncmp(Rune*, Rune*, long);
Rune*	runestrncpy(Rune*, Rune*, long);
Rune*	runestrrchr(Rune*, Rune);
Rune*	runestrstr(Rune*, Rune*);
int	runetochar(char*, Rune *rune);
Rune	totitlerune(Rune);
char*	utfecpy(char*, char *end, char*);
int	utflen(char*);
int	utfnlen(char*, long);
char*	utfrrune(char*, long);
char*	utfrune(char*, long);
char*	utfutf(char*, char*);
char*	toutf8n(const char*, int);
char*	toutf8(const char*);
