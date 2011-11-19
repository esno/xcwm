#ifndef _CWM_H_
#define _CWM_H_

static void cclose(const Arg *arg);
static void ccreate(xcb_window_t w);
static void cdestroy(Client *c);
static void cfocus(Client *c);
static Client *cfocused();
static void ckill(const Arg *arg);
static void ctom(Client *c, Monitor *m);
static void event_destroynotify(xcb_destroy_notify_event_t *e);
static void event_keypress(xcb_key_press_event_t *e);
static void event_maprequest(xcb_map_request_event_t *e);
static void focus(const Arg *arg);
static xcb_atom_t getatom(char *name);
static void grabkeys();
static void mclean();
static void mcreate(int x, int y, int w, int h);
static void mdestroy(Monitor *m);
static void mscan();
static void quit(const Arg *arg);
static void run();
static void spawn(const Arg *arg);
static void tile();

#endif /* _CWM_H_ */
