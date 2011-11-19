// cwm is licensed under the terms of the MIT License

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef XINERAMA
#include <xcb/randr.h>
#endif
#include <xcb/xcb_icccm.h>

#include "wm.h"
#include "cwm.h"
#include "config.h"

struct Mgr cwm;

void cclose(const Arg *arg) {
	/* Thanks to Michael Stapelberg (i3wm.org) */
	Client *c = cfocused();
	int i;

	if(c != NULL) {
		xcb_icccm_get_wm_protocols_reply_t protocols;
		xcb_atom_t wm_protocols = getatom("WM_PROTOCOLS");
		xcb_atom_t delete_atom = getatom("WM_DELETE_WINDOW");
		xcb_get_property_cookie_t prop_c = xcb_icccm_get_wm_protocols(cwm.connection, c->window, wm_protocols);

		if(xcb_icccm_get_wm_protocols_reply(cwm.connection, prop_c, &protocols, NULL) == 1) {
			for(i = 0; i < protocols.atoms_len; ++i) {
				if(protocols.atoms[i] == delete_atom) {
					xcb_client_message_event_t ev = { .response_type = XCB_CLIENT_MESSAGE, .format = 32, .sequence = 0, .window = c->window, .type = wm_protocols, .data.data32 = { delete_atom, XCB_CURRENT_TIME} };
					xcb_send_event(cwm.connection, 0, c->window, XCB_EVENT_MASK_NO_EVENT, (char *) &ev);
				}
			}

			xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
			xcb_flush(cwm.connection);
		}
	}
}

void ccreate(xcb_window_t w) {
	Client *c = malloc(sizeof(Client));

	if(c == NULL)
		fprintf(stderr, "ERROR\tcannot allocate memory for client\n");
	else {
		fprintf(stdout, "CREATE\tclient %d on address %p\n", w, c);

		c->window = w;
		c->monitor = NULL;
		c->pref.focus = false;

		ctom(c, cwm.mstack);

		if(cwm.pref.focusoncreate)
			cfocus(c);
	}
}

void cdestroy(Client *c) {
	if(c == NULL)
		fprintf(stderr, "ERROR\tno client to destroy\n");
	else {
		fprintf(stdout, "DESTROY\tclient %d on address %p\n", c->window, c);

		if(c != c->next) {
			c->next->prev = c->prev;
			c->prev->next = c->next;

			if(c->pref.focus)
				cfocus(c->next);

			if(c->monitor->cstack == c)
				c->monitor->cstack = c->next;
		} else
			c->monitor->cstack = NULL;

		free(c);
	}
}

void cfocus(Client *c) {
	Client *t = c->monitor->cstack;
	uint32_t val[2];

	val[0] = 0;

	do {
		t = t->next;
		val[1] = (c == t) ? XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW;

		xcb_configure_window(cwm.connection, t->window, XCB_CONFIG_WINDOW_BORDER_WIDTH | XCB_CONFIG_WINDOW_STACK_MODE, val);
	} while(c != t);

	fprintf(stdout, "INFO\tfocus client %d on address %p\n", c->window, c);
	xcb_set_input_focus(cwm.connection, XCB_INPUT_FOCUS_POINTER_ROOT, c->window, XCB_CURRENT_TIME);
	xcb_flush(cwm.connection);

	if((t = cfocused()) != NULL) {
		fprintf(stdout, "INFO\tunfocus client %d on address %p\n", t->window, t);
		t->pref.focus = false;
	}

	c->pref.focus = true;
}

Client *cfocused() {
	Client *c = NULL;
	if(cwm.mstack->cstack != NULL) {
		Client *t = cwm.mstack->cstack;

		do {
			if(t->pref.focus)
				c = t;
			t = t->next;
		} while(t != cwm.mstack->cstack && c == NULL);
	}

	return c;
}

void ckill(const Arg *arg) {
	Client *c = cfocused();
	
	if(c != NULL) {
		xcb_kill_client(cwm.connection, c->window);
		xcb_flush(cwm.connection);

		if(c != c->next)
			cfocus(c->next);

		cdestroy(c);
	}
}

void ctom(Client *c, Monitor *m) {
	if(c != NULL && m != NULL) {
		fprintf(stdout, "MOVE\tclient on %p to monitor on %p\n", c, m);

		if(c == c->next && c->monitor != NULL)
			c->monitor->cstack = NULL;

		if(m->cstack == NULL) {
			c->next = c;
			c->prev = c;
			m->cstack = c;
		} else {
			c->next = m->cstack;
			c->prev = m->cstack->prev;
			m->cstack->prev->next = c;
			m->cstack->prev = c;
		}

		c->monitor = m;
	}
}

void event_destroynotify(xcb_destroy_notify_event_t *e) {
	fprintf(stdout, "INFO\tdestroynotify from window %d\n", e->window);

	Monitor *m = cwm.mstack;
	Client *c;
	bool deleted = false;

	if(m != NULL) {
		do {
			if((c = m->cstack) != NULL) {
				do {
					if(c->window == e->window) {
						cdestroy(c);
						deleted = true;
					} else
						c = c->next;
				} while(!deleted && c != m->cstack);
			}

			m = m->next;
		} while(!deleted && m != cwm.mstack);
	}
}

void event_keypress(xcb_key_press_event_t *e) {
	unsigned int len = (sizeof(keys) / sizeof(*keys));
	unsigned int i;

	fprintf(stdout, "INFO\tkeypress on %u + %u\n", e->state, e->detail);

	for(i = 0; i < len; ++i) {
		if(keys[i].mod == e->state && keys[i].keysym == e->detail && keys[i].func)
			keys[i].func(&keys[i].arg);

		xcb_flush(cwm.connection);
	}

	xcb_send_event(cwm.connection, 1, XCB_SEND_EVENT_DEST_ITEM_FOCUS, XCB_EVENT_MASK_NO_EVENT, (char *) e);
}

void event_maprequest(xcb_map_request_event_t *e) {
	Client *c, *t = cwm.mstack->cstack;
	bool new = true;

	fprintf(stdout, "INFO\tmaprequest from window %d\n", e->window);
	xcb_map_window(cwm.connection, e->window);

	if(t != NULL) {
		do {
			if(t->window == e->window) {
				new = false;
				c = t;
			}

			t = t->next;
		} while(cwm.mstack->cstack != t);
	}

	if(new)
		ccreate(e->window);

	xcb_flush(cwm.connection);
	tile();
}

void focus(const Arg *arg) {
	Client *c = cfocused();

	if(c != NULL) {
		switch(arg->i) {
			case -1:
				c = c->prev;
				break;
			case 1:
				c = c->next;
				break;
		}

		cfocus(c);
	}
}

xcb_atom_t getatom(char *name) {
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(cwm.connection, 0, strlen(name), name);
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(cwm.connection, cookie, NULL);

	return reply->atom;
}

void grabkeys() {
	unsigned int len = (sizeof(keys) / sizeof(*keys));
	unsigned int i;

	for(i = 0; i < len; ++i) {
		fprintf(stdout, "INFO\tgrab keys (mod: %d, keysym: %d)\n", keys[i].mod, keys[i].keysym);
		xcb_grab_key(cwm.connection, 1, cwm.screen->root, keys[i].mod, keys[i].keysym, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	}

	xcb_flush(cwm.connection);
}

void mclean() {
	while(cwm.mstack != NULL) {
		mdestroy(cwm.mstack);
	}
}

void mcreate(int x, int y, int w, int h) {
	Monitor *m = malloc(sizeof(Monitor));

	if(m == NULL)
		fprintf(stderr, "ERROR\tcannot allocate memory for monitor\n");
	else {
		fprintf(stdout, "CREATE\tmonitor (pos/size: %dx%d/%dx%d) on address %p\n", x, y, w, h, m);

		if(cwm.mstack == NULL) {
			m->next = m;
			m->prev = m;
			cwm.mstack = m;
		} else {
			m->next = cwm.mstack;
			m->prev = cwm.mstack->prev;
			cwm.mstack->prev->next = m;
			cwm.mstack->prev = m;
		}

		m->cstack = NULL;
		m->pref.bottomstack = bottomstack;
		m->pref.mastersize = mastersize;
		m->pref.x = x;
		m->pref.y = y;
		m->pref.w = w;
		m->pref.h = h;
	}
}

void mdestroy(Monitor *m) {
	if(m == NULL)
		fprintf(stderr, "ERROR\tno monitor to destroy\n");
	else {
		fprintf(stdout, "DESTROY\tmonitor (pos/size: %dx%d/%dx%d) on address %p\n", m->pref.x, m->pref.y, m->pref.w, m->pref.h, m);

		while(m->cstack != NULL) {
			cdestroy(m->cstack);
		}

		if(m != m->next) {
			m->next->prev = m->prev;
			m->prev->next = m->next;

			if(m == cwm.mstack)
				cwm.mstack = m->next;
		} else
			cwm.mstack = NULL;

		free(m);
	}
}

void mscan() {
	#ifndef XINERAMA
	if(xcb_get_extension_data(cwm.connection, &xcb_randr_id)->present) {
		xcb_randr_get_screen_resources_cookie_t sr_cookie = xcb_randr_get_screen_resources(cwm.connection, cwm.screen->root);
		xcb_randr_get_screen_resources_reply_t *sr_reply = xcb_randr_get_screen_resources_reply(cwm.connection, sr_cookie, NULL);

		if(sr_reply->num_crtcs <= 1)
			mcreate(0, 0, cwm.screen->width_in_pixels, cwm.screen->height_in_pixels);
		else {
			xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_crtcs(sr_reply);
			int i;

			for(i = 0; i < sr_reply->num_crtcs; ++i) {
				xcb_randr_get_crtc_info_cookie_t rc_info_cookie = xcb_randr_get_crtc_info(cwm.connection, crtcs[i], XCB_CURRENT_TIME);
				xcb_randr_get_crtc_info_reply_t *rc_info_reply = xcb_randr_get_crtc_info_reply(cwm.connection, rc_info_cookie, NULL);

				if(xcb_randr_get_crtc_info_outputs_length(rc_info_reply))
					mcreate(rc_info_reply->x, rc_info_reply->y, rc_info_reply->width, rc_info_reply->height);
			}
		}
	}
	#else
	fprintf(stderr, "ERROR\tXINERAMA is not implemented yet\nfalling back to one screen\n");
	mcreate(0, 0, cwm.screen->width_in_pixels, cwm.screen->height_in_pixels);
	#endif
}

void quit(const Arg *arg) {
	cwm.running = false;
}

void run() {
	cwm.running = true;
	xcb_generic_event_t *ev;

	while(cwm.running) {
		ev = xcb_wait_for_event(cwm.connection);

		switch(ev->response_type & ~0x80) {
			case XCB_MAP_REQUEST:
				event_maprequest((xcb_map_request_event_t *) ev);
				break;
			case XCB_KEY_PRESS:
				event_keypress((xcb_key_press_event_t *) ev);
				break;
			case XCB_DESTROY_NOTIFY:
				event_destroynotify((xcb_destroy_notify_event_t *) ev);
				break;
			#ifdef DEBUG
			default:
				fprintf(stdout, "INFO\tunhandled event %d\n", ev);
			#endif
		}

		free(ev);
	}
}

void spawn(const Arg *arg) {
	if(fork() == 0) {
		setsid();
		execvp((char *) arg->c[0], (char **) arg->c);
	}
}

void tile() {
	Client *c = cfocused();

	if(c != NULL) {
		uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		uint32_t val[4] = { c->monitor->pref.x, c->monitor->pref.y, c->monitor->pref.w, c->monitor->pref.h };

		xcb_configure_window(cwm.connection, c->window, mask, val);
		xcb_flush(cwm.connection);
	}
}

int main(int argc, char **argv[]) {
	cwm.connection = xcb_connect(NULL, NULL);
	cwm.mstack = NULL;
	cwm.pref.focusoncreate = focusoncreate;

	if(xcb_connection_has_error(cwm.connection))
		fprintf(stderr, "ERROR\tcannot open display\n");
	else {
		setlocale(LC_CTYPE, "");

		xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(xcb_get_setup(cwm.connection));
		uint32_t val[1] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW };
		xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(cwm.connection, iterator.data->root, XCB_CW_EVENT_MASK, val);

		if(xcb_request_check(cwm.connection, cookie) != NULL)
			fprintf(stderr, "ERROR\tanother window manager is running\n");
		else {
			cwm.screen = iterator.data;
			
			mscan();
			grabkeys();
			run();
			mclean();
		}

		xcb_disconnect(cwm.connection);
	}

	return 0;
}
