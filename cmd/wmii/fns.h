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
Bar *create_bar(Bar **b_link, char *name);
void destroy_bar(Bar **b_link, Bar*);
void draw_bar(WMScreen *s);
void draw_border(BlitzBrush*);
void resize_bar();
Bar *bar_of_name(Bar *b_link, const char *name);

/* client.c */
Client *create_client(Window, XWindowAttributes*);
void destroy_client(Client*);
void configure_client(Client*);
void prop_client(Client *c, Atom);
void kill_client(Client*);
void gravitate_client(Client*, Bool invert);
void map_client(Client*);
void unmap_client(Client*, int state);
void map_frame(Client*);
void unmap_frame(Client*);
void set_cursor(Client*, Cursor cur);
void focus_frame(Frame*, Bool restack);
void reparent_client(Client*, Window w, int x, int y);
void manage_client(Client*);
void focus(Client*, Bool restack);
void focus_client(Client*);
void resize_client(Client*, XRectangle*);
void apply_sizehints(Client*, XRectangle*, Bool floating, Bool frame, BlitzAlign sticky);
char *send_client(Frame*, char*, Bool swap);
char * message_client(Client*, char*);
void move_client(Client*, char *arg);
void size_client(Client*, char *arg);
Client *selclient();
Frame *win2frame(Window);
Client *win2client(Window);
void update_client_grab(Client*);
void apply_rules(Client*);
void apply_tags(Client*, const char*);

/* column.c */
Divide *win2div(Window);
void update_dividers();
void update_divs();
void draw_div(Divide*);
void arrange_column(Area*, Bool dirty);
void resize_column(Area*, int w);
void resize_colframe(Frame*, XRectangle*);
int str2colmode(const char *str);
Area *new_column(View*, Area *pos, uint w);

/* draw.c */
int loadcolor(Blitz *, BlitzColor *);
void draw_label(BlitzBrush *, char *text);
void draw_tile(BlitzBrush *);
void draw_rect(BlitzBrush *);

void drawbg(Display*, Drawable, GC,
		XRectangle*, BlitzColor, Bool fill, Bool border);
void drawcursor(Display*, Drawable, GC,
				int x, int y, uint h, BlitzColor);
uint textwidth(BlitzFont*, char *text);
uint textwidth_l(BlitzFont*, char *text, uint len);
void loadfont(Blitz*, BlitzFont*);
uint labelh(BlitzFont *font);
char *parse_colors(char **buf, int *buflen, BlitzColor*);

/* event.c */
void dispatch_event(XEvent*);
void check_x_event(IxpConn*);
uint flushevents(long even_mask, Bool dispatch);

/* frame.c */
Frame *create_frame(Client*, View*);
void remove_frame(Frame*);
void insert_frame(Frame *pos, Frame*, Bool before);
void resize_frame(Frame*, XRectangle*);
Bool frame_to_top(Frame *f);
void set_frame_cursor(Frame*, int x, int y);
void swap_frames(Frame*, Frame*);
int frame_delta_h();
void frame2client(Frame*, XRectangle*);
void client2frame(Frame*, XRectangle*);
int ingrabbox(Frame*, int x, int y);
void draw_frame(Frame*);
void draw_frames();
void update_frame_widget_colors(Frame*);
void check_frame_constraints(XRectangle*);

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
Bool ptinrect(int x, int y, XRectangle*);
BlitzAlign quadrant(XRectangle*, int x, int y);
Cursor cursor_of_quad(BlitzAlign);
int strtorect(XRectangle*, const char*);
BlitzAlign get_sticky(XRectangle *src, XRectangle *dst);
int r_east(XRectangle*);
int r_south(XRectangle*);

/* key.c */
void kpress(Window, ulong mod, KeyCode);
void update_keys();
void init_lock_keys();
ulong mod_key_of_str(char*);

/* mouse.c */
void mouse_resizecol(Divide*);
void do_mouse_resize(Client*, Bool opaque, BlitzAlign);
void grab_mouse(Window, ulong mod, ulong button);
void ungrab_mouse(Window, ulong mod, uint button);
BlitzAlign snap_rect(XRectangle *rects, int num, XRectangle *current,
					 BlitzAlign *mask, int snapw);
void grab_button(Window, uint button, ulong mod);

/* rule.c */
void update_rules(Rule**, const char*);
void trim(char *str, const char *chars);

/* view.c */
void arrange_view(View*);
void scale_view(View*, float w);
View *get_view(const char*);
View *create_view(const char*);
void focus_view(WMScreen*, View*);
void update_client_views(Client*, char**);
XRectangle *rects_of_view(View*, uint *num, Frame *ignore);
View *view_of_id(ushort);
void select_view(const char*);
void attach_to_view(View*, Frame*);
Client *view_selclient(View*);
char *message_view(View*, char*);
void restack_view(View*);
uchar *view_index(View*);
void destroy_view(View*);
void update_views();
uint newcolw_of_view(View*, int i);

/* wm.c */
int wmii_error_handler(Display*, XErrorEvent *error);
int win_proto(Window);
