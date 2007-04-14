/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

/* wm.c */
char *message_root(char *message);

/* area.c */
Area *create_area(View *v, Area *pos, uint w);
void destroy_area(Area *a);
Area *area_of_id(View *t, ushort id);
void focus_area(Area *a);
char *select_area(Area *a, char *arg);
void send_to_area(Area *to, Frame *f);
void attach_to_area(Area *a, Frame *f, Bool send);
void detach_from_area(Frame *f);
Client *area_selclient(Area *a);

/* bar.c */
Bar *create_bar(Bar **b_link, char *name);
void destroy_bar(Bar **b_link, Bar *b);
void draw_bar(WMScreen *s);
void draw_border(BlitzBrush *b);
void resize_bar();
Bar *bar_of_name(Bar *b_link, const char *name);

/* client.c */
Client *create_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void prop_client(Client *c, Atom a);
void kill_client(Client *c);
void gravitate_client(Client *c, Bool invert);
void map_client(Client *c);
void unmap_client(Client *c, int state);
void map_frame(Client *c);
void unmap_frame(Client *c);
void set_cursor(Client *c, Cursor cur);
void focus_frame(Frame *f, Bool restack);
void reparent_client(Client *c, Window w, int x, int y);
void manage_client(Client *c);
void focus(Client *c, Bool restack);
void focus_client(Client *c);
void resize_client(Client *c, XRectangle *r);
void apply_sizehints(Client *c, XRectangle *r, Bool floating, Bool frame, BlitzAlign sticky);
char *send_client(Frame *f, char *arg, Bool swap);
char * message_client(Client *c, char *message);
void move_client(Client *c, char *arg);
void size_client(Client *c, char *arg);
Client *selclient();
Frame *win2frame(Window w);
Client *win2client(Window w);
void update_client_grab(Client *c);
void apply_rules(Client *c);
void apply_tags(Client *c, const char *tags);

/* column.c */
Divide *win2div(Window w);
void update_dividers();
void update_divs();
void draw_div(Divide *d);
void arrange_column(Area *a, Bool dirty);
void resize_column(Frame *f, XRectangle *r);
int str2colmode(const char *str);
Area *new_column(View *v, Area *pos, uint w);

/* draw.c */
int loadcolor(Blitz *blitz, BlitzColor *c);
void draw_label(BlitzBrush *b, char *text);
void draw_tile(BlitzBrush *b);
void draw_rect(BlitzBrush *b);

void drawbg(Display *dpy, Drawable drawable, GC gc,
		XRectangle *rect, BlitzColor c, Bool fill, Bool border);
void drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, uint h, BlitzColor c);
uint textwidth(BlitzFont *font, char *text);
uint textwidth_l(BlitzFont *font, char *text, uint len);
void loadfont(Blitz *blitz, BlitzFont *font);
uint labelh(BlitzFont *font);
char *parse_colors(char **buf, int *buflen, BlitzColor *col);

/* event.c */
void dispatch_event(XEvent *e);
void check_x_event(IxpConn *c);
uint flushevents(long even_mask, Bool dispatch);

/* frame.c */
Frame *create_frame(Client *c, View *v);
void remove_frame(Frame *f);
void insert_frame(Frame *pos, Frame *f, Bool before);
void resize_frame(Frame *f, XRectangle *r);
Bool frame_to_top(Frame *f);
void set_frame_cursor(Frame *f, int x, int y);
void swap_frames(Frame *fa, Frame *fb);
int frame_delta_h();
void frame2client(Frame *f, XRectangle *r);
void client2frame(Frame *f, XRectangle *r);
int ingrabbox(Frame *f, int x, int y);
void draw_frame(Frame *f);
void draw_frames();
void update_frame_widget_colors(Frame *f);
void check_frame_constraints(XRectangle *rect);

/* fs.c */
void fs_attach(Ixp9Req *r);
void fs_clunk(Ixp9Req *r);
void fs_create(Ixp9Req *r);
void fs_flush(Ixp9Req *r);
void fs_freefid(Fid *f);
void fs_open(Ixp9Req *r);
void fs_read(Ixp9Req *r);
void fs_remove(Ixp9Req *r);
void fs_stat(Ixp9Req *r);
void fs_walk(Ixp9Req *r);
void fs_write(Ixp9Req *r);
void write_event(char *format, ...);

/* geom.c */
Bool ptinrect(int x, int y, XRectangle * r);
BlitzAlign quadrant(XRectangle *rect, int x, int y);
Cursor cursor_of_quad(BlitzAlign align);
int strtorect(XRectangle *r, const char *val);
BlitzAlign get_sticky(XRectangle *src, XRectangle *dst);
int r_east(XRectangle *r);
int r_south(XRectangle *r);

/* key.c */
void kpress(Window w, ulong mod, KeyCode keycode);
void update_keys();
void init_lock_keys();
ulong mod_key_of_str(char *val);

/* mouse.c */
void do_mouse_resize(Client *c, Bool grabbox, BlitzAlign align);
void grab_mouse(Window w, ulong mod, ulong button);
void ungrab_mouse(Window w, ulong mod, uint button);
BlitzAlign snap_rect(XRectangle *rects, int num, XRectangle *current,
					 BlitzAlign *mask, int snap);
void grab_button(Window w, uint button, ulong mod);

/* rule.c */
void update_rules(Rule **rule, const char *data);
void trim(char *str, const char *chars);

/* view.c */
void arrange_view(View *v);
void scale_view(View *v, float w);
View *get_view(const char *name);
View *create_view(const char *name);
void focus_view(WMScreen *s, View *v);
void update_client_views(Client *c, char **tags);
XRectangle *rects_of_view(View *v, uint *num, Frame *ignore);
View *view_of_id(ushort id);
void select_view(const char *arg);
void attach_to_view(View *v, Frame *f);
Client *view_selclient(View *v);
char *message_view(View *v, char *message);
void restack_view(View *v);
uchar *view_index(View *v);
void destroy_view(View *v);
void update_views();
uint newcolw_of_view(View *v, int i);

/* wm.c */
int wmii_error_handler(Display *dpy, XErrorEvent *error);
int win_proto(Window w);
