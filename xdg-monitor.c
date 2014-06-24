/*****************************************************************************

 Copyright (c) 2008-2014  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, version 3 of the license.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 this program.  If not, see <http://www.gnu.org/licenses/>, or write to the
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 -----------------------------------------------------------------------------

 U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on
 behalf of the U.S. Government ("Government"), the following provisions apply
 to you.  If the Software is supplied by the Department of Defense ("DoD"), it
 is classified as "Commercial Computer Software" under paragraph 252.227-7014
 of the DoD Supplement to the Federal Acquisition Regulations ("DFARS") (or any
 successor regulations) and the Government is acquiring only the license rights
 granted herein (the license rights customarily provided to non-Government
 users).  If the Software is supplied to any unit or agency of the Government
 other than DoD, it is classified as "Restricted Computer Software" and the
 Government's rights in the Software are defined in paragraph 52.227-19 of the
 Federal Acquisition Regulations ("FAR") (or any successor regulations) or, in
 the cases of NASA, in paragraph 18.52.227-86 of the NASA Supplement to the FAR
 (or any successor regulations).

 -----------------------------------------------------------------------------

 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See http://www.openss7.com/

 *****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#ifdef _GNU_SOURCE
#include <getopt.h>
#endif
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#endif
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#ifdef DESKTOP_NOTIFICATIONS
#include <libnotify/notify.h>
#endif
#ifdef SYSTEM_TRAY_STATUS_ICON
#include <gtk/gtk.h>
#endif

#define DPRINTF(_args...) do { if (options.debug > 0) { \
		fprintf(stderr, "D: %12s: +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); } } while (0)

#define EPRINTF(_args...) do { \
		fprintf(stderr, "E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args);   } while (0)

#define OPRINTF(_args...) do { if (options.debug > 0 || options.output > 1) { \
		fprintf(stderr, "I: "); \
		fprintf(stderr, _args); } } while (0)

#define PTRACE() do { if (options.debug > 0 || options.output > 2) { \
		fprintf(stderr, "T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \

typedef enum {
	EXEC_MODE_MONITOR = 0,
	EXEC_MODE_REPLACE,
	EXEC_MODE_QUIT,
	EXEC_MODE_HELP,
	EXEC_MODE_VERSION,
	EXEC_MODE_COPYING
} ExecMode;

typedef struct {
	int debug;
	int output;
	int foreground;
	long guardtime;
	ExecMode exec_mode;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.foreground = 0,
	.guardtime = 15000,
	.exec_mode = EXEC_MODE_MONITOR,
};

static const char *StartupNotifyFields[] = {
	"LAUNCHER",
	"LAUNCHEE",
	"SEQUENCE",
	"ID",
	"NAME",
	"ICON",
	"BIN",
	"DESCRIPTION",
	"WMCLASS",
	"SILENT",
	"APPLICATION_ID",
	"DESKTOP",
	"SCREEN",
	"MONITOR",
	"TIMESTAMP",
	"PID",
	"HOSTNAME",
	"COMMAND",
	"ACTION",
	"FILE",
	"URL",
	NULL
};

typedef enum {
	FIELD_OFFSET_LAUNCHER,
	FIELD_OFFSET_LAUNCHEE,
	FIELD_OFFSET_SEQUENCE,
	FIELD_OFFSET_ID,
	FIELD_OFFSET_NAME,
	FIELD_OFFSET_ICON,
	FIELD_OFFSET_BIN,
	FIELD_OFFSET_DESCRIPTION,
	FIELD_OFFSET_WMCLASS,
	FIELD_OFFSET_SILENT,
	FIELD_OFFSET_APPLICATION_ID,
	FIELD_OFFSET_DESKTOP,
	FIELD_OFFSET_SCREEN,
	FIELD_OFFSET_MONITOR,
	FIELD_OFFSET_TIMESTAMP,
	FIELD_OFFSET_PID,
	FIELD_OFFSET_HOSTNAME,
	FIELD_OFFSET_COMMAND,
	FIELD_OFFSET_ACTION,
	FIELD_OFFSET_FILE,
	FIELD_OFFSET_URL,
} FieldOffset;

struct fields {
	char *launcher;
	char *launchee;
	char *sequence;
	char *id;
	char *name;
	char *icon;
	char *bin;
	char *description;
	char *wmclass;
	char *silent;
	char *application_id;
	char *desktop;
	char *screen;
	char *monitor;
	char *timestamp;
	char *pid;
	char *hostname;
	char *command;
	char *action;
	char *file;
	char *url;
};

typedef enum {
	StartupNotifyIdle,
	StartupNotifyNew,
	StartupNotifyChanged,
	StartupNotifyComplete,
} StartupNotifyState;

typedef struct _Client Client;

typedef struct _Sequence {
	struct _Sequence *next;
	int refs;
	StartupNotifyState state;
	Client *client;
	Bool changed;
	union {
		char *fields[sizeof(struct fields) / sizeof(char *)];
		struct fields f;
	};
	struct {
		unsigned screen;
		unsigned monitor;
		unsigned desktop;
		unsigned timestamp;
		unsigned silent;
		unsigned pid;
		unsigned sequence;
	} n;
#ifdef DESKTOP_NOTIFICATIONS
	NotifyNotification *notification;
#endif
#ifdef SYSTEM_TRAY_STATUS_ICON
	GtkStatusIcon *status;
#endif
} Sequence;

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

struct entry {
	char *Name;
	char *Comment;
	char *Icon;
	char *TryExec;
	char *Exec;
	char *Terminal;
	char *StartupNotify;
	char *StartupWMClass;
	char *SessionSetup;
};

struct entry entry = { NULL, };

typedef struct {
	int screen;			/* screen number */
	Window root;			/* root window of screen */
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;
	Window tray;			/* _NET_SYSTEM_TRAY_S%d owner */
	Client *clients;		/* clients for this screen */
	Sequence *sequences;		/* sequences for this screen */
} XdgScreen;

XdgScreen *screens;

Display *dpy = NULL;
int nscr;
XdgScreen *scr;

typedef struct {
	unsigned long state;
	Window icon_window;
} XWMState;

enum {
	XEMBED_MAPPED = (1<<0),
};

typedef struct {
	unsigned long version;
	unsigned long flags;
} XEmbedInfo;

struct _Client {
	Client *next;			/* next client in list */
	Sequence *seq;			/* associated startup sequence */
	int screen;			/* screen number */
	Window win;			/* client window */
	Window time_win;		/* client time window */
	Window transient_for;		/* transient for */
	Window leader;			/* client leader window */
	Window group;			/* client group window */
	Bool dockapp;			/* client is a dock app */
	Bool statusicon;		/* client is a status icon */
	Bool breadcrumb;		/* for traversing list */
	Bool managed;			/* managed by window manager */
	Bool new;			/* brand new */
	Time active_time;		/* last time active */
	Time focus_time;		/* last time focused */
	Time user_time;			/* last time used */
	Time map_time;			/* last time window was mapped */
	Time last_time;			/* last time something happened to this window */
	pid_t pid;			/* process id */
	char *startup_id;		/* startup id (property) */
	char **command;			/* command */
	char *name;			/* window name */
	char *hostname;			/* client machine */
	char *client_id;		/* session management id */
	char *role;			/* session management role */
	XClassHint ch;			/* class hint */
	XWMHints *wmh;			/* window manager hints */
	XWMState *wms;			/* window manager state */
	XEmbedInfo *xei;		/* _XEMBED_INFO property */
};

Time last_user_time = CurrentTime;

Atom _XA_KDE_WM_CHANGE_STATE;
Atom _XA_MANAGER;
Atom _XA_MOTIF_WM_INFO;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_STARTUP_ID;
Atom _XA_NET_STARTUP_INFO;
Atom _XA_NET_STARTUP_INFO_BEGIN;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_VIRTUAL_ROOTS;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_NET_WM_ALLOWED_ACTIONS;
Atom _XA_NET_WM_PID;
Atom _XA_NET_WM_STATE;
Atom _XA_NET_WM_STATE_FOCUSED;
Atom _XA_NET_WM_USER_TIME;
Atom _XA_NET_WM_USER_TIME_WINDOW;
Atom _XA_SM_CLIENT_ID;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_CLIENT_LEADER;
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_STATE;
Atom _XA_WM_WINDOW_ROLE;
Atom _XA_XDG_ASSIST_CMD;
Atom _XA_XDG_ASSIST_CMD_QUIT;
Atom _XA_XDG_ASSIST_CMD_REPLACE;
Atom _XA_XEMBED;
Atom _XA_XEMBED_INFO;

static void handle_MANAGER(XEvent *, Client *);
static void handle_MOTIF_WM_INFO(XEvent *, Client *);
static void handle_NET_ACTIVE_WINDOW(XEvent *, Client *);
static void handle_NET_CLIENT_LIST(XEvent *, Client *);
static void handle_NET_STARTUP_INFO_BEGIN(XEvent *, Client *);
static void handle_NET_STARTUP_INFO(XEvent *, Client *);
static void handle_NET_SUPPORTED(XEvent *, Client *);
static void handle_NET_SUPPORTING_WM_CHECK(XEvent *, Client *);
static void handle_NET_WM_STATE(XEvent *, Client *);
static void handle_NET_WM_USER_TIME(XEvent *, Client *);
static void handle_SM_CLIENT_ID(XEvent *, Client *);
static void handle_WIN_CLIENT_LIST(XEvent *, Client *);
static void handle_WINDOWMAKER_NOTICEBOARD(XEvent *, Client *);
static void handle_WIN_PROTOCOLS(XEvent *, Client *);
static void handle_WIN_SUPPORTING_WM_CHECK(XEvent *, Client *);
static void handle_WM_CLASS(XEvent *, Client *);
static void handle_WM_CLIENT_LEADER(XEvent *, Client *);
static void handle_WM_CLIENT_MACHINE(XEvent *, Client *);
static void handle_WM_COMMAND(XEvent *, Client *);
static void handle_WM_HINTS(XEvent *, Client *);
static void handle_WM_ICON_NAME(XEvent *, Client *);
static void handle_WM_ICON_SIZE(XEvent *, Client *);
static void handle_WM_NAME(XEvent *, Client *);
static void handle_WM_NORMAL_HINTS(XEvent *, Client *);
static void handle_WM_PROTOCOLS(XEvent *, Client *);
static void handle_WM_SIZE_HINTS(XEvent *, Client *);
static void handle_WM_STATE(XEvent *, Client *);
static void handle_WM_TRANSIENT_FOR(XEvent *, Client *);
static void handle_WM_WINDOW_ROLE(XEvent *, Client *);
static void handle_WM_ZOOM_HINTS(XEvent *, Client *);
static void handle_XEMBED(XEvent *, Client *);
static void handle_XEMBED_INFO(XEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	void (*handler) (XEvent *, Client *);
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global				handler					atom value		*/
	/* ----					------				-------					----------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,	NULL,					None			},
	{ "MANAGER",				&_XA_MANAGER,			&handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,		&handle_MOTIF_WM_INFO,			None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,		&handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,		&handle_NET_CLIENT_LIST,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,	NULL,					None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,	&handle_NET_STARTUP_INFO_BEGIN,		None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,		&handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,		&handle_NET_SUPPORTED,			None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,	&handle_NET_SUPPORTING_WM_CHECK,	None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,		NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,	NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		NULL,					None			},
	{ "_NET_WM_STATE_FOCUSED",		&_XA_NET_WM_STATE_FOCUSED,	NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&handle_NET_WM_STATE,			None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&handle_NET_WM_USER_TIME,		None			},
	{ "SM_CLIENT_ID",			&_XA_SM_CLIENT_ID,		&handle_SM_CLIENT_ID,			None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,		NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,		NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,		&handle_WIN_CLIENT_LIST,		None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,	&handle_WINDOWMAKER_NOTICEBOARD,	None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,		&handle_WIN_PROTOCOLS,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,	&handle_WIN_SUPPORTING_WM_CHECK,	None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,		NULL,					None			},
	{ "WM_CLASS",				NULL,				&handle_WM_CLASS,			XA_WM_CLASS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&handle_WM_CLIENT_LEADER,		None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&handle_WM_CLIENT_MACHINE,		XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&handle_WM_COMMAND,			XA_WM_COMMAND		},
	{ "WM_HINTS",				NULL,				&handle_WM_HINTS,			XA_WM_HINTS		},
	{ "WM_ICON_NAME",			NULL,				&handle_WM_ICON_NAME,			XA_WM_ICON_NAME		},
	{ "WM_ICON_SIZE",			NULL,				&handle_WM_ICON_SIZE,			XA_WM_ICON_SIZE		},
	{ "WM_NAME",				NULL,				&handle_WM_NAME,			XA_WM_NAME		},
	{ "WM_NORMAL_HINTS",			NULL,				&handle_WM_NORMAL_HINTS,		XA_WM_NORMAL_HINTS	},
	{ "WM_PROTOCOLS",			&_XA_WM_PROTOCOLS,		&handle_WM_PROTOCOLS,			None			},
	{ "WM_SIZE_HINTS",			NULL,				&handle_WM_SIZE_HINTS,			XA_WM_SIZE_HINTS	},
	{ "WM_STATE",				&_XA_WM_STATE,			&handle_WM_STATE,			None			},
	{ "WM_TRANSIENT_FOR",			NULL,				&handle_WM_TRANSIENT_FOR,		XA_WM_TRANSIENT_FOR	},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&handle_WM_WINDOW_ROLE,			None			},
	{ "WM_ZOOM_HINTS",			NULL,				&handle_WM_ZOOM_HINTS,			XA_WM_ZOOM_HINTS	},
	{ "_XDG_ASSIST_CMD_QUIT",		&_XA_XDG_ASSIST_CMD_QUIT,	NULL,					None			},
	{ "_XDG_ASSIST_CMD_REPLACE",		&_XA_XDG_ASSIST_CMD_REPLACE,	NULL,					None			},
	{ "_XDG_ASSIST_CMD",			&_XA_XDG_ASSIST_CMD,		NULL,					None			},
	{ "_XEMBED",				&_XA_XEMBED,			&handle_XEMBED,				None			},
	{ "_XEMBED_INFO",			&_XA_XEMBED_INFO,		&handle_XEMBED_INFO,			None			},
	{ NULL,					NULL,				NULL,					None			}
	/* *INDENT-ON* */
};

void
intern_atoms()
{
	int i, j, n;
	char **atom_names;
	Atom *atom_values;

	for (i = 0, n = 0; atoms[i].name; i++)
		if (atoms[i].atom)
			n++;
	atom_names = calloc(n + 1, sizeof(*atom_names));
	atom_values = calloc(n + 1, sizeof(*atom_values));
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			atom_names[j++] = atoms[i].name;
	XInternAtoms(dpy, atom_names, n, False, atom_values);
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			*atoms[i].atom = atoms[i].value = atom_values[j++];
	free(atom_names);
	free(atom_values);
}

int
handler(Display *display, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, "xdg-launch", num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

XContext ClientContext;			/* window to client context */
XContext MessageContext;		/* window to message context */

Bool
get_display()
{
	if (!dpy) {
		int s;

		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		XSetErrorHandler(handler);
		nscr = ScreenCount(dpy);
		if (!(screens = calloc(nscr, sizeof(*screens)))) {
			EPRINTF("no memory\n");
			exit(EXIT_FAILURE);
		}
		for (s = 0, scr = screens; s < nscr; s++, scr++) {
			scr->screen = s;
			scr->root = RootWindow(dpy, s);
			XSelectInput(dpy, scr->root,
				     VisibilityChangeMask | StructureNotifyMask |
				     FocusChangeMask | PropertyChangeMask);
		}
		s = DefaultScreen(dpy);
		scr = screens + s;
		intern_atoms();

		ClientContext = XUniqueContext();
		MessageContext = XUniqueContext();
	}
	return (dpy ? True : False);
}

static char *
get_text(Window win, Atom prop)
{
	XTextProperty tp = { NULL, };

	XGetTextProperty(dpy, win, &tp, prop);
	if (tp.value) {
		tp.value[tp.nitems + 1] = '\0';
		return (char *) tp.value;
	}
	return NULL;
}

static long *
get_cardinals(Window win, Atom prop, Atom type, long *n)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	long *data = NULL;

      try_harder:
	if (XGetWindowProperty(dpy, win, prop, 0L, num, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			goto try_harder;
		}
		if ((*n = nitems) > 0)
			return data;
		if (data)
			XFree(data);
	} else
		*n = -1;
	return NULL;
}

static Bool
get_cardinal(Window win, Atom prop, Atom type, long *card_ret)
{
	Bool result = False;
	long *data, n;

	if ((data = get_cardinals(win, prop, type, &n)) && n > 0) {
		*card_ret = data[0];
		result = True;
	}
	if (data)
		XFree(data);
	return result;
}

static Window *
get_windows(Window win, Atom prop, Atom type, long *n)
{
	return (Window *) get_cardinals(win, prop, type, n);
}

static Bool
get_window(Window win, Atom prop, Atom type, Window *win_ret)
{
	return get_cardinal(win, prop, type, (long *) win_ret);
}

Time *
get_times(Window win, Atom prop, Atom type, long *n)
{
	return (Time *) get_cardinals(win, prop, type, n);
}

static Bool
get_time(Window win, Atom prop, Atom type, Time *time_ret)
{
	return get_cardinal(win, prop, type, (long *) time_ret);
}

static Atom *
get_atoms(Window win, Atom prop, Atom type, long *n)
{
	return (Atom *) get_cardinals(win, prop, type, n);
}

Bool
get_atom(Window win, Atom prop, Atom type, Atom *atom_ret)
{
	return get_cardinal(win, prop, type, (long *) atom_ret);
}

XWMState *
XGetWMState(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XWMState *wms = NULL;

	if (XGetWindowProperty(d, win, _XA_WM_STATE, 0L, num, False,
			       AnyPropertyType, &real, &format, &nitems,
			       &after, (unsigned char **) &wms) == Success
	    && format != 0) {
		if (format != 32 || nitems < 2) {
			if (wms) {
				XFree(wms);
				return NULL;
			}
		}
		return wms;
	}
	return NULL;

}

XEmbedInfo *
XGetEmbedInfo(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XEmbedInfo *xei = NULL;

	if (XGetWindowProperty(d, win, _XA_XEMBED_INFO, 0L, num, False,
			       AnyPropertyType, &real, &format, &nitems,
			       &after, (unsigned char **) &xei) == Success
	    && format != 0) {
		if (format != 32 || nitems < 2) {
			if (xei) {
				XFree(xei);
				return NULL;
			}
		}
		return xei;
	}
	return NULL;
}

Bool
latertime(Time last, Time time)
{
	if (time == CurrentTime)
		return False;
	if (last == CurrentTime || (int) time - (int) last > 0)
		return True;
	return False;
}

void
pushtime(Time *last, Time time)
{
	if (latertime(*last, time))
		*last = time;
}

void
pulltime(Time *last, Time time)
{
	if (!latertime(*last, time))
		*last = time;
}

/** @brief Check for recursive window properties
  * @param atom - property name
  * @param type - property type
  * @return Window - the recursive window property or None
  */
Window
check_recursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check;

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check,
					     PropertyChangeMask | StructureNotifyMask);
			XFree(data);
			data = NULL;
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty(dpy, check, atom, 0L, 1L, False, type, &real,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success
		    && format != 0) {
			if (nitems > 0) {
				if (check != (Window) data[0]) {
					XFree(data);
					return None;
				}
			} else {
				if (data)
					XFree(data);
				return None;
			}
			XFree(data);
		} else
			return None;
	} else
		return None;
	return check;
}

/** @brief Check for non-recusive window properties (that should be recursive).
  * @param atom - property name
  * @param atom - property type
  * @return Window - the recursive window property or None
  */
Window
check_nonrecursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check = None;

	OPRINTF("non-recursive check for atom 0x%lx\n", atom);

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check,
					     PropertyChangeMask | StructureNotifyMask);
		}
		if (data)
			XFree(data);
	}
	return check;
}

/** @brief Check if an atom is in a supported atom list.
  * @param atom - list name
  * @param atom - element name
  * @return Bool - true if atom is in list
  */
static Bool
check_supported(Atom protocols, Atom supported)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	unsigned long *data = NULL;
	Bool result = False;

	OPRINTF("check for non-compliant NetWM\n");

      try_harder:
	if (XGetWindowProperty(dpy, scr->root, protocols, 0L, num, False,
			       XA_ATOM, &real, &format, &nitems, &after,
			       (unsigned char **) &data)
	    == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			goto try_harder;
		}
		if (nitems > 0) {
			unsigned long i;
			Atom *atoms;

			result = True;
			atoms = (Atom *) data;
			for (i = 0; i < nitems; i++) {
				if (atoms[i] == supported) {
					result = False;
					break;
				}
			}
		}
		if (data)
			XFree(data);
	}
	return result;
}

/** @brief Check for a non-compliant EWMH/NetWM window manager.
  *
  * There are quite a few window managers that implement part of hte EWMH/NetWM
  * specification but do not fill out _NET_SUPPORTING_WM_CHECK.  This is a big
  * annoyance.  One way to test this is whether there is a _NET_SUPPORTED on the
  * root window that does not include _NET_SUPPORTING_WM_CHECK in its atom list.
  *
  * The only window manager I know of that placed _NET_SUPPORTING_WM_CHECK in
  * the list and did not set the property on the root window was 2bwm, but is
  * has now been fixed.
  *
  * There are others that provide _NET_SUPPORTING_WM_CHECK on the root window
  * but fail to set it recursively.  When _NET_SUPPORTING_WM_CHECK is reported
  * as supported, relax the check to a non-recursive check.  (Case in point is
  * echinus(1)).
  */
static Window
check_netwm_supported()
{
	if (check_supported(_XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

/** @brief Check for an EWMH/NetWM compliant (sorta) window manager.
  */
static Window
check_netwm()
{
	int i = 0;

	do {
		scr->netwm_check = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
	} while (i++ < 2 && !scr->netwm_check);

	if (scr->netwm_check)
		XSelectInput(dpy, scr->netwm_check,
			     PropertyChangeMask | StructureNotifyMask);
	else
		scr->netwm_check = check_netwm_supported();

	return scr->netwm_check;
}

/** @brief Check for a non-compliant GNOME/WinWM window manager.
  *
  * There are quite a few window managers that implement part of the GNOME/WinWM
  * specification but do not fill in the _WIN_SUPPORTING_WM_CHECK.  This is
  * another big annoyance.  One way to test this is whether there is a
  * _WIN_PROTOCOLS on the root window that does not include
  * _WIN_SUPPORTING_WM_CHECK in its list of atoms.
  */
static Window
check_winwm_supported()
{
	if (check_supported(_XA_WIN_PROTOCOLS, _XA_WIN_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

/** @brief Check for a GNOME1/WMH/WinWM compliant window manager.
  */
static Window
check_winwm()
{
	int i = 0;

	do {
		scr->winwm_check =
		    check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
	} while (i++ < 2 && !scr->winwm_check);

	if (scr->winwm_check)
		XSelectInput(dpy, scr->winwm_check,
			     PropertyChangeMask | StructureNotifyMask);
	else
		scr->winwm_check = check_winwm_supported();

	return scr->winwm_check;
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker()
{
	int i = 0;

	do {
		scr->maker_check = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW);
	} while (i++ < 2 && !scr->maker_check);

	if (scr->maker_check)
		XSelectInput(dpy, scr->maker_check,
			     PropertyChangeMask | StructureNotifyMask);

	return scr->maker_check;
}

/** @brief Check for an OSF/Motif compliant window manager.
  */
static Window
check_motif()
{
	int i = 0;
	long *data, n = 0;

	do {
		data = get_cardinals(scr->root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n);
	} while (i++ < 2 && !data);

	if (data && n >= 2)
		scr->motif_check = data[1];
	if (scr->motif_check)
		XSelectInput(dpy, scr->motif_check,
			     PropertyChangeMask | StructureNotifyMask);

	return scr->motif_check;
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	char buf[32];
	Atom atom;

	snprintf(buf, 32, "WM_S%d", scr->screen);
	if ((atom = XInternAtom(dpy, buf, True)))
		scr->icccm_check = XGetSelectionOwner(dpy, atom);

	if (scr->icccm_check)
		XSelectInput(dpy, scr->icccm_check,
			     PropertyChangeMask | StructureNotifyMask);

	return scr->icccm_check;
}

/** @brief Check whether an ICCCM window manager is present.
  *
  * This pretty much assumes that any ICCCM window manager will select for
  * SubstructureRedirectMask on the root window.
  */
static Window
check_redir()
{
	XWindowAttributes wa;

	OPRINTF("checking direction for screen %d\n", scr->screen);

	scr->redir_check = None;
	if (XGetWindowAttributes(dpy, scr->root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			scr->redir_check = scr->root;
	return scr->redir_check;
}

/** @brief Find window manager and compliance for the current screen.
  */
static Bool
check_window_manager()
{
	Bool have_wm = False;

	OPRINTF("checking wm compliance for screen %d\n", scr->screen);

	OPRINTF("checking redirection\n");
	if (check_redir()) {
		have_wm = True;
		OPRINTF("redirection on window 0x%lx\n", scr->redir_check);
	}
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm()) {
		have_wm = True;
		OPRINTF("ICCCM 2.0 window 0x%lx\n", scr->icccm_check);
	}
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif()) {
		have_wm = True;
		OPRINTF("OSF/Motif window 0x%lx\n", scr->motif_check);
	}
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker()) {
		have_wm = True;
		OPRINTF("WindowMaker window 0x%lx\n", scr->maker_check);
	}
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm()) {
		have_wm = True;
		OPRINTF("GNOME/WMH window 0x%lx\n", scr->winwm_check);
	}
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm()) {
		have_wm = True;
		OPRINTF("NetWM/EWMH window 0x%lx\n", scr->netwm_check);
	}

	return have_wm;
}

static void
handle_wmchange(XEvent *e, Client *c)
{
	if (!e || e->type == PropertyNotify)
		if (!check_window_manager())
			check_window_manager();
}

static void
handle_WINDOWMAKER_NOTICEBOARD(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

static void
handle_MOTIF_WM_INFO(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

static void
handle_NET_SUPPORTING_WM_CHECK(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

static void
handle_NET_SUPPORTED(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

static void
handle_WIN_SUPPORTING_WM_CHECK(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

static void
handle_WIN_PROTOCOLS(XEvent *e, Client *c)
{
	handle_wmchange(e, c);
}

Bool
set_screen_of_root(Window sroot)
{
	int s;

	for (s = 0; s < nscr; s++) {
		if (screens[s].root == sroot) {
			scr = screens + s;
			return True;
		}
	}
	EPRINTF("Could not find screen for root 0x%lx!\n", sroot);
	return False;
}

/* @brief find the frame of a client window.
 *
 * This only really works for reparenting window managers (which are the most
 * common).  It watches out for virual roots.
 */
Window
get_frame(Window win)
{
	Window frame = win, fparent, froot = None, *fchildren, *vroots, vroot = None;
	unsigned int du;
	long i, n = 0;

	vroots = get_windows(scr->root, _XA_NET_VIRTUAL_ROOTS, XA_WINDOW, &n);
	get_window(scr->root, _XA__SWM_VROOT, XA_WINDOW, &vroot);

	for (fparent = frame; fparent != froot; frame = fparent) {
		if (!XQueryTree(dpy, frame, &froot, &fparent, &fchildren, &du)) {
			frame = None;
			goto done;
		}
		if (fchildren)
			XFree(fchildren);
		/* When the parent is a virtual root, we have the frame. */
		for (i = 0; i < n; i++)
			if (fparent == vroots[i])
				goto done;
		if (vroot && fparent == vroot)
			goto done;
	}
      done:
	if (vroots)
		XFree(vroots);
	return frame;
}

Window
get_focus_frame()
{
	Window focus;
	int di;

	XGetInputFocus(dpy, &focus, &di);

	if (focus == None || focus == PointerRoot)
		return None;

	return get_frame(focus);
}

Bool
find_focus_screen()
{
	Window frame, froot;
	int di;
	unsigned int du;

	if (!(frame = get_focus_frame()))
		return False;

	if (!XGetGeometry(dpy, frame, &froot, &di, &di, &du, &du, &du, &du))
		return False;

	return set_screen_of_root(froot);
}

Bool
find_pointer_screen()
{
	Window proot = None, dw;
	int di;
	unsigned int du;

	if (XQueryPointer(dpy, scr->root, &proot, &dw, &di, &di, &di, &di, &du))
		return True;
	return set_screen_of_root(proot);
}

Bool
find_window_screen(Window w)
{
	Window wroot, dw, *dwp;
	unsigned int du;

	if (!XQueryTree(dpy, w, &wroot, &dw, &dwp, &du))
		return False;
	if (dwp)
		XFree(dwp);

	return set_screen_of_root(wroot);
}

Client *
find_client(Window w)
{
	Client *c = NULL;

	if (XFindContext(dpy, w, ClientContext, (XPointer *) &c))
		find_window_screen(w);
	else
		scr = screens + c->screen;
	return (c);
}

/*
 * Check whether the window manager needs assitance: return True when it needs assistance
 * and False otherwise.
 */
Bool
need_assistance()
{
	if (!check_netwm()) {
		DPRINTF("Failed NetWM check!\n");
		return True;
	}
	if (!check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
		DPRINTF("_NET_STARTUP_ID not supported\n");
		return True;
	}
	return False;
}

/** @brief look up the file from APPID.
  *
  * We search in the applications directories first (i.e.
  * /usr/share/applications) and then in the autostart directories (e.g.
  * /etc/xdg/autostart);
  *
  */
char *
lookup_file(char *name)
{
	char *path = NULL, *appid;
	struct stat st;

	appid = calloc(1024, sizeof(*appid));
	strncat(appid, name, 1023);

	if (strstr(appid, ".desktop") != appid + strlen(appid) - 8)
		strncat(appid, ".desktop", 1024);
	if ((*appid != '/') && (*appid != '.')) {
		/* need to look in applications subdirectory of XDG_DATA_HOME and then
		   each of the subdirectories in XDG_DATA_DIRS */
		char *home, *copy, *dirs;

		if (getenv("XDG_DATA_DIRS") && *getenv("XDG_DATA_DIRS") != '\0')
			dirs = getenv("XDG_DATA_DIRS");
		else
			dirs = "/usr/local/share:/usr/share";
		home = calloc(PATH_MAX, sizeof(*home));
		if (getenv("XDG_DATA_HOME") && *getenv("XDG_DATA_HOME") != '\0')
			strcpy(home, getenv("XDG_DATA_HOME"));
		else {
			if (getenv("HOME"))
				strcpy(home, getenv("HOME"));
			else
				strcpy(home, "~");
			strcat(home, "/.local/share");
		}
		strcat(home, ":");
		strcat(home, dirs);
		copy = strdup(home);
		for (dirs = copy; !path && strlen(dirs);) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = 0;
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, 4096);
			strcat(path, "/applications/");
			strcat(path, appid);
			if (stat(path, &st)) {
				free(path);
				path = NULL;
				continue;
			}
			free(copy);
			free(home);
			free(appid);
			return (path);
		}
		free(copy);
		/* next look in fallback subdirectory of XDG_DATA_HOME and then each of
		   the subdirectories in XDG_DATA_DIRS */
		copy = strdup(home);
		for (dirs = copy; !path && strlen(dirs);) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = 0;
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, 4096);
			strcat(path, "/fallback/");
			strcat(path, appid);
			if (stat(path, &st)) {
				free(path);
				path = NULL;
				continue;
			}
			free(copy);
			free(home);
			free(appid);
			return (path);
		}
		free(copy);
		/* next look in autostart subdirectory of XDG_CONFIG_HOME and then each
		   of the subdirectories in XDG_CONFIG_DIRS */
		if (getenv("XDG_CONFIG_DIRS") && *getenv("XDG_CONFIG_DIRS") != '\0')
			dirs = getenv("XDG_CONFIG_DIRS");
		else
			dirs = "/etc/xdg";
		if (getenv("XDG_CONFIG_HOME") && *getenv("XDG_CONFIG_HOME") != '\0')
			strcpy(home, getenv("XDG_CONFIG_HOME"));
		else {
			if (getenv("HOME"))
				strcpy(home, getenv("HOME"));
			else
				strcpy(home, "~");
			strcat(home, "/.config");
		}
		strcat(home, ":");
		strcat(home, dirs);
		copy = strdup(home);
		for (dirs = copy; !path && strlen(dirs);) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = 0;
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, 4096);
			strcat(path, "/autostart/");
			strcat(path, appid);
			if (stat(path, &st)) {
				free(path);
				path = NULL;
				continue;
			}
			free(copy);
			free(home);
			free(appid);
			return (path);
		}
		free(copy);
		free(home);
	} else {
		path = strdup(appid);
		if (stat(path, &st)) {
			free(path);
			path = NULL;
		}
	}
	free(appid);
	return path;
}

void
apply_quotes(char **str, char *q)
{
	char *p;

	for (p = *str; *p; p++) ;
	while (*q) {
		if (*q == ' ' || *q == '"' || *q == '\\')
			*p++ = '\\';
		*p++ = *q++;
	}
	*p = '\0';
	*str = p;
}

Bool running;

void
send_msg(char *msg)
{
	XEvent xev;
	int l;
	Window from;
	char *p;

	DPRINTF("Message is: '%s'\n", msg);

	from = XCreateSimpleWindow(dpy, scr->root, 0, 0, 1, 1, 0, ParentRelative,
				   ParentRelative);

	xev.type = ClientMessage;
	xev.xclient.message_type = _XA_NET_STARTUP_INFO_BEGIN;
	xev.xclient.display = dpy;
	xev.xclient.window = from;
	xev.xclient.format = 8;

	l = strlen((p = msg)) + 1;

	while (l > 0) {
		strncpy(xev.xclient.data.b, p, 20);
		p += 20;
		l -= 20;
		XSendEvent(dpy, scr->root, False, PropertyChangeMask, &xev);
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}

	XDestroyWindow(dpy, from);
}

struct {
	char *label;
	FieldOffset offset;
} labels[] = {
	/* *INDENT-OFF* */
	{ " NAME=",		FIELD_OFFSET_NAME		},
	{ " ICON=",		FIELD_OFFSET_ICON		},
	{ " BIN=",		FIELD_OFFSET_BIN		},
	{ " DESCRIPTION=",	FIELD_OFFSET_DESCRIPTION	},
	{ " WMCLASS=",		FIELD_OFFSET_WMCLASS		},
	{ " SILENT=",		FIELD_OFFSET_SILENT		},
	{ " APPLICATION_ID=",	FIELD_OFFSET_APPLICATION_ID	},
	{ " DESKTOP=",		FIELD_OFFSET_DESKTOP		},
	{ " SCREEN=",		FIELD_OFFSET_SCREEN		},
	{ " MONITOR=",		FIELD_OFFSET_MONITOR		},
	{ " TIMESTAMP=",	FIELD_OFFSET_TIMESTAMP		},
	{ " PID=",		FIELD_OFFSET_PID		},
	{ " HOSTNAME=",		FIELD_OFFSET_HOSTNAME		},
	{ " COMMAND=",		FIELD_OFFSET_COMMAND		},
        { " ACTION=",           FIELD_OFFSET_ACTION		},
	{ NULL,			FIELD_OFFSET_ID			}
	/* *INDENT-ON* */
};

void
add_field(Sequence *seq, char **p, char *label, FieldOffset offset)
{
	char *value;

	if ((value = seq->fields[offset])) {
		strcat(*p, label);
		apply_quotes(p, value);
	}
}

void
add_fields(Sequence *seq, char *msg)
{
	int i;

	for (i = 0; labels[i].label; i++)
		add_field(seq, &msg, labels[i].label, labels[i].offset);
}

void
send_new(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "new:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyNew;
}

void
send_change(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "change:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyChanged;
}

void
send_remove(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "remove:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyComplete;
}

char *
get_proc_file(pid_t pid, char *name, size_t *size)
{
	struct stat st;
	char *file, *buf;
	FILE *f;
	size_t read, total;

	file = calloc(64, sizeof(*file));
	snprintf(file, 64, "/proc/%d/%s", pid, name);

	if (stat(file, &st)) {
		free(file);
		*size = 0;
		return NULL;
	}
	buf = calloc(st.st_size, sizeof(*buf));
	if (!(f = fopen(file, "rb"))) {
		free(file);
		free(buf);
		*size = 0;
		return NULL;
	}
	free(file);
	/* read entire file into buffer */
	for (total = 0; total < st.st_size; total += read)
		if ((read = fread(buf + total, 1, st.st_size - total, f)))
			if (total < st.st_size) {
				free(buf);
				fclose(f);
				*size = 0;
				return NULL;
			}
	fclose(f);
	*size = st.st_size;
	return buf;
}

char *
get_proc_startup_id(pid_t pid)
{
	char *buf, *pos, *end;
	size_t size;

	if (!(buf = get_proc_file(pid, "environ", &size)))
		return NULL;

	for (pos = buf, end = buf + size; pos < end; pos += strlen(pos) + 1) {
		if (strstr(pos, "DESKTOP_STARTUP_ID=") == pos) {
			pos += strlen("DESKTOP_STARTUP_ID=");
			pos = strdup(pos);
			free(buf);
			return pos;
		}
	}
	free(buf);
	return NULL;
}

char *
get_proc_comm(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "comm", &size);
}

char *
get_proc_exec(pid_t pid)
{
	struct stat st;
	char *file, *buf;
	ssize_t size;

	file = calloc(64, sizeof(*file));
	snprintf(file, 64, "/proc/%d/exe", pid);
	if (stat(file, &st)) {
		free(file);
		return NULL;
	}
	buf = calloc(256, sizeof(*buf));
	if ((size = readlink(file, buf, 256)) < 0 || size > 256 - 1) {
		free(file);
		free(buf);
		return NULL;
	}
	buf[size + 1] = '\0';
	return buf;
}

char *
get_proc_argv0(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "cmdline", &size);
}

static Bool
test_client(Client *c)
{
	Sequence *seq;
	pid_t pid;
	char *str;

	if (!(seq = c->seq))
		return False;
	if (c->startup_id) {
		if (!strcmp(c->startup_id, seq->f.id))
			return True;
		else
			return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcmp(seq->f.hostname, c->hostname)))
		pid = 0;
	if (pid && (str = get_proc_startup_id(pid))) {
		if (strcmp(seq->f.id, str))
			return False;
		else
			return True;
		free(str);
	}
	/* correct wmclass */
	if (seq->f.wmclass) {
		if (c->ch.res_name && !strcmp(seq->f.wmclass, c->ch.res_name))
			return True;
		if (c->ch.res_class && !strcmp(seq->f.wmclass, c->ch.res_class))
			return True;
	}
	/* same process id */
	if (pid && atoi(seq->f.pid) == pid)
		return True;
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == strtoul(seq->f.timestamp, NULL, 0))
		return True;
	/* correct executable */
	if (pid && seq->f.bin) {
		if ((str = get_proc_comm(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
	}
	/* NOTE: we use the PID to look in: /proc/[pid]/cmdline for argv[]
	   /proc/[pid]/environ for env[] /proc/[pid]/comm basename of executable
	   /proc/[pid]/exe symbolic link to executable */
	return False;
}

static Sequence *ref_sequence(Sequence *seq);

/** @brief find the startup sequence for a client
  * @param c - client to lookup startup sequence for
  * @return Sequence * - the found sequence or NULL if not found
  */
static Sequence *
find_startup_seq(Client *c)
{
	Sequence *seq;

	if ((seq = c->seq))
		return seq;

	/* search by startup id */
	if (c->startup_id) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			if (!seq->f.id) {
				EPRINTF("sequence with null id!\n");
				continue;
			}
			if (!strcmp(c->startup_id, seq->f.id))
				break;
		}
		if (!seq) {
			DPRINTF("cannot find startup id '%s'!\n", c->startup_id);
			return (seq);
		}
		goto found_it;
	}

	/* search by wmclass */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name && !strcmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class && !strcmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq)
			goto found_it;
	}

	/* search by binary */
	if (c->command) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *binary;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((binary = seq->f.bin)) {
				if (c->command[0] && !strcmp(binary, c->command[0]))
					break;
			}

		}
		if (seq)
			goto found_it;
	}

	/* search by wmclass (this time case insensitive) */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name
				    && !strcasecmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class
				    && !strcasecmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq)
			goto found_it;
	}

	/* search by pid and hostname */
	if (c->pid && c->hostname) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *hostname;
			pid_t pid;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if (!(pid = seq->n.pid) || !(hostname = seq->f.hostname))
				continue;
			if (c->pid == pid && !strcmp(c->hostname, hostname))
				break;
		}
		if (seq)
			goto found_it;
	}
	DPRINTF("could not find startup ID for client\n");
	return NULL;
      found_it:
	seq->client = c;
	c->seq = ref_sequence(seq);
	return (seq);
}

void
setup_client(Client *c)
{
	/* use /proc/[pid]/cmdline to set up WM_COMMAND if not present */
}

/** @brief detect whether a client is a dockapp
  */
static Bool
is_dockapp(Client *c)
{
	/* In an attempt to get around GTK+ >= 2.4.0 limitations, some GTK+ dock apps
	   simply set the res_class to "DockApp". */
	if (c->ch.res_class && !strcmp(c->ch.res_class, "DockApp"))
		return True;
	if (!c->wmh)
		return False;
	/* Many libxpm based dockapps use xlib to set the state hint correctly. */
	if ((c->wmh->flags & StateHint) && c->wmh->initial_state == WithdrawnState)
		return True;
	/* Many dockapps that were based on GTK+ < 2.4.0 are having their initial_state
	   changed to NormalState by GTK+ >= 2.4.0, so when the other flags are set,
	   accept anyway (note that IconPositionHint is optional). */
	if ((c->wmh->flags & ~IconPositionHint) ==
	    (WindowGroupHint | StateHint | IconWindowHint))
		return True;
	return False;
}

/** @brief detect whether a client is a status icon
  */
Bool
is_statusicon(Client *c)
{
	if (c->xei)
		return True;
	return False;
}

/** @brief update client from X server
  * @param c - the client to update
  *
  * Updates the client from information and properties maintained by the X
  * server.  Care should be taken that information that shoule be obtained from
  * a group window is obtained from that window first and then overwritten by
  * any information contained in the specific window.
  */
static void
update_client(Client *c)
{
	long card;
	int count = 0;

	if (c->wmh)
		XFree(c->wmh);
	c->group = None;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
	}

	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);

	XFetchName(dpy, c->win, &c->name);
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);
	}

	if (c->hostname)
		XFree(c->hostname);
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);

	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);

	free(c->startup_id);
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);

	card = 0;
	if (!get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card) && c->group)
		get_cardinal(c->group, _XA_NET_WM_PID, XA_CARDINAL, &card);
	c->pid = card;

	find_startup_seq(c);
	if (test_client(c))
		setup_client(c);

	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win, ExposureMask | VisibilityChangeMask |
		     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
	if (c->group) {
		XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->group, ExposureMask | VisibilityChangeMask |
			     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
	}
	c->new = False;
}

static Client *
add_client(Window win)
{
	Client *c;

	c = calloc(1, sizeof(*c));
	c->win = win;
	c->next = scr->clients;
	scr->clients = c;
	update_client(c);
	c->new = True;
	return (c);
}

static void
remove_client(Client *c)
{
	if (c->ch.res_name)
		XFree(c->ch.res_name);
	if (c->ch.res_class)
		XFree(c->ch.res_class);
	if (c->wmh)
		XFree(c->wmh);
	if (c->name)
		XFree(c->name);
	if (c->startup_id)
		XFree(c->startup_id);
	if (c->command)
		XFreeStringList(c->command);
	if (c->hostname)
		XFree(c->hostname);
	if (c->role)
		XFree(c->role);
	XDeleteContext(dpy, c->win, ClientContext);
	if (c->time_win)
		XDeleteContext(dpy, c->time_win, ClientContext);
	if (c->wms)
		XFree(c->wms);
	if (c->xei)
		XFree(c->xei);
	free(c);
}

void
del_client(Client *r)
{
	Client *c, **cp;

	for (cp = &scr->clients, c = *cp; c && c != r; cp = &c->next, c = *cp) ;
	if (c == r)
		*cp = c->next;
	remove_client(r);
}

/** @brief update client from startup notification sequence
  * @param seq - the sequence that has changed state
  *
  * Update the client associated with a startup notification sequence.
  */
static void
update_startup_client(Sequence *seq)
{
	Client *c;

	if (!(c = seq->client))
		/* Note that we do not want to go searching for clients because any
		   client that we would find at this point could get a false positive
		   against an client that existed before the startup notification
		   sequence.  We could use creation timestamps to filter out the false
		   positives, but that is for later. */
		return;
	/* TODO: things to update are: _NET_WM_PID, WM_CLIENT_MACHINE, ...  Note that
	   _NET_WM_STARTUP_ID should already be set. */
}

static void
convert_sequence_fields(Sequence *seq)
{
	if (seq->f.screen)
		seq->n.screen = atoi(seq->f.screen);
	if (seq->f.monitor)
		seq->n.monitor = atoi(seq->f.monitor);
	if (seq->f.desktop)
		seq->n.desktop = atoi(seq->f.desktop);
	if (seq->f.timestamp)
		seq->n.timestamp = atoi(seq->f.timestamp);
	if (seq->f.silent)
		seq->n.silent = atoi(seq->f.silent);
	if (seq->f.pid)
		seq->n.pid = atoi(seq->f.pid);
	if (seq->f.sequence)
		seq->n.sequence = atoi(seq->f.sequence);
}

static void
free_sequence_fields(Sequence *seq)
{
	int i;

	for (i = 0; i < sizeof(seq->f) / sizeof(char *); i++) {
		free(seq->fields[i]);
		seq->fields[i] = NULL;
	}
}

static void
copy_sequence_fields(Sequence *old, Sequence *new)
{
	int i;

	for (i = 0; i < sizeof(old->f) / sizeof(char *); i++) {
		if (new->fields[i]) {
			free(old->fields[i]);
			old->fields[i] = new->fields[i];
		}
	}
	convert_sequence_fields(old);
}

static Sequence *
find_seq_by_id(char *id)
{
	Sequence *seq;

	for (seq = scr->sequences; seq; seq = seq->next)
		if (!strcmp(seq->f.id, id))
			break;
	return (seq);
}

static void
add_sequence(Sequence *seq)
{
	seq->refs = 1;
	seq->client = NULL;
	seq->next = scr->sequences;
	scr->sequences = seq;
}

static Sequence *
unref_sequence(Sequence *seq)
{
	if (seq) {
		if (--seq->refs <= 0) {
			Sequence *s, **prev;

			for (prev = &scr->sequences, s = *prev; s && s != seq;
			     prev = &s->next, s = *prev) ;
			if (s) {
				*prev = s->next;
				s->next = NULL;
			}
			free_sequence_fields(seq);
			free(seq);
			return (NULL);
		}
	}
	return (seq);
}

static Sequence *
ref_sequence(Sequence *seq)
{
	if (seq)
		++seq->refs;
	return (seq);
}

static Sequence *
remove_sequence(Sequence *del)
{
	Sequence *seq, **prev;

	for (prev = &scr->sequences, seq = *prev; seq && seq != del;
	     prev = &seq->next, seq = *prev) ;
	if (seq) {
		*prev = seq->next;
		seq->next = NULL;
		unref_sequence(seq);
	} else
		EPRINTF("could not find sequence\n");
	return (seq);
}

static void
process_startup_msg(Message *m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q, *copy, *b;
	const char **f;
	int i;
	int escaped, quoted;

	if (!strncmp(p, "new:", 4)) {
		cmd.state = StartupNotifyNew;
	} else if (!strncmp(p, "change:", 7)) {
		cmd.state = StartupNotifyChanged;
	} else if (!strncmp(p, "remove:", 7)) {
		cmd.state = StartupNotifyComplete;
	} else {
		free(m->data);
		free(m);
		return;
	}
	p = strchr(p, ':') + 1;
	while (*p != '\0') {
		while (*p == ' ')
			p++;
		k = p;
		if (!(v = strchr(k, '='))) {
			free_sequence_fields(&cmd);
			DPRINTF("mangled message\n");
			return;
		} else {
			*v++ = '\0';
			p = q = v;
			escaped = quoted = 0;
			for (;;) {
				if (!escaped) {
					if (*p == '"') {
						p++;
						quoted ^= 1;
						continue;
					} else if (*p == '\\') {
						p++;
						escaped = 1;
						continue;
					} else if (*p == '\0' || (*p == ' ' && !quoted)) {
						if (quoted) {
							free_sequence_fields(&cmd);
							DPRINTF("mangled message\n");
							return;
						}
						if (*p == ' ')
							p++;
						*q = '\0';
						break;
					}
				}
				*q++ = *p++;
				escaped = 0;
			}
			for (i = 0, f = StartupNotifyFields; f[i] != NULL; i++)
				if (strcmp(f[i], k) == 0)
					break;
			if (f[i] != NULL)
				cmd.fields[i] = strdup(v);
		}
	}
	free(m->data);
	free(m);
	if (!cmd.f.id) {
		free_sequence_fields(&cmd);
		DPRINTF("message with no ID= field\n");
		return;
	}
	/* get information from ID */
	do {
		p = q = copy = strdup(cmd.f.id);
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launcher)
			cmd.f.launcher = strdup(q);
		q = p;
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launchee)
			cmd.f.launchee = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.pid)
			cmd.f.pid = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.sequence)
			cmd.f.sequence = strdup(q);
		q = p;
		if (!(p = strstr(q, "_TIME")))
			break;
		*p++ = '\0';
		if (!cmd.f.hostname)
			cmd.f.hostname = strdup(q);
		q = p + 4;
		if (!cmd.f.timestamp)
			cmd.f.timestamp = strdup(q);
	} while (0);
	free(copy);
	/* get timestamp from ID if necessary */
	if (!cmd.f.timestamp && (p = strstr(cmd.f.id, "_TIME")) != NULL)
		cmd.f.timestamp = strdup(p + 5);
	convert_sequence_fields(&cmd);
	if (!(seq = find_seq_by_id(cmd.f.id))) {
		if (cmd.state != StartupNotifyNew) {
			free_sequence_fields(&cmd);
			DPRINTF("message out of sequence\n");
			return;
		}
		if (!(seq = calloc(1, sizeof(*seq)))) {
			free_sequence_fields(&cmd);
			DPRINTF("no memory\n");
			return;
		}
		*seq = cmd;
		add_sequence(seq);
		return;
	}
	switch (seq->state) {
	case StartupNotifyIdle:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
			seq->state = StartupNotifyNew;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyNew:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyChanged:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyComplete:
		/* remove sequence */
		break;
	}
	/* remove sequence */
	remove_sequence(seq);
}

static void
handle_NET_STARTUP_INFO_BEGIN(XEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	if (!e || e->type != ClientMessage)
		return;
	from = e->xclient.window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m) {
		m = calloc(1, sizeof(*m));
		XSaveContext(dpy, from, MessageContext, (XPointer) m);
	}
	free(m->data);
	m->origin = from;
	m->data = calloc(21, sizeof(*m->data));
	m->len = 0;
	/* unpack data */
	len = strnlen(e->xclient.data.b, 20);
	strncat(m->data, e->xclient.data.b, 20);
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

static void
handle_NET_STARTUP_INFO(XEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	if (!e || e->type != ClientMessage)
		return;
	from = e->xclient.window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m)
		return;
	m->data = realloc(m->data, m->len + 21);
	/* unpack data */
	len = strnlen(e->xclient.data.b, 20);
	strncat(m->data, e->xclient.data.b, 20);
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

void
clean_msgs(Window w)
{
	Message *m = NULL;

	if (XFindContext(dpy, w, MessageContext, (XPointer *) &m))
		return;

	XDeleteContext(dpy, w, MessageContext);
	free(m->data);
	free(m);
}

/** @brief perform actions necessary for a newly managed client
  * @param c - the client
  * @param t - the time that the client was managed or CurrentTime when unknown
  *
  * The client has just become managed, so try to associated it with a startup
  * notfication sequence if it has not already been associated.  If the client
  * can be associated with a startup notification sequence, then terminate the
  * sequence when it has WMCLASS set, or when SILENT is set (indicating no
  * startup notification is pending).  Otherwise, wait for the sequence to
  * complete on its own.
  *
  * If we cannot associated a sequence at this point, the launcher may be
  * defective and only sent the startup notification after the client has
  * already launched, so leave the client around for later checks.  We can
  * compare the map_time of the client to the timestamp of a later startup
  * notification to determine whether the startup notification should have been
  * sent before the client mapped.
  */
static void
managed_client(Client *c, Time t)
{
	Sequence *seq = NULL;

	c->managed = True;
	pulltime(&c->map_time, t ? : c->last_time);
	if (!(seq = c->seq) && !(seq = find_startup_seq(c)))
		return;
	switch (seq->state) {
	case StartupNotifyIdle:
	case StartupNotifyComplete:
		break;
	case StartupNotifyNew:
	case StartupNotifyChanged:
		if (!seq->f.wmclass && !seq->n.silent)
			/* We are expecting that the client will generate startup
			   notification completion on its own, so let the timers run and
			   wait for completion. */
			return;
		/* We are not expecting that the client will generate startup
		   notification completion on its own.  Either we generate the completion 
		   or wait for a supporting window manager to do so. */
		send_remove(seq);
		break;
	}
	/* FIXME: we can remove the startup notification and the client, but perhaps we
	   will just wait for the client to be unmanaged or destroyed. */
}

static void
unmanaged_client(Client *c)
{
	c->managed = False;
	/* FIXME: we can remove the client and any associated startup notification. */
}

static void
handle_SM_CLIENT_ID(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	if (!(c->client_id = get_text(c->win, _XA_SM_CLIENT_ID)) && c->leader)
		c->client_id = get_text(c->leader, _XA_SM_CLIENT_ID);
	if (!(c->client_id) && c->group)
		c->client_id = get_text(c->group, _XA_SM_CLIENT_ID);
}

static void
handle_WM_CLASS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);
	c->dockapp = is_dockapp(c);
}

static void
handle_WM_CLIENT_LEADER(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->leader = None;
	if (e && e->xproperty.state == PropertyDelete)
		return;
	get_window(c->win, _XA_WM_CLIENT_LEADER, AnyPropertyType, &c->leader);
}

static void
handle_WM_CLIENT_MACHINE(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->hostname) {
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->leader)
		c->hostname = get_text(c->leader, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->transient_for)
		c->hostname = get_text(c->transient_for, XA_WM_CLIENT_MACHINE);
}

static void
handle_WM_COMMAND(XEvent *e, Client *c)
{
	int count = 0;

	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);
	if (!c->command && c->leader)
		XGetCommand(dpy, c->leader, &c->command, &count);
	if (!c->command && c->transient_for)
		XGetCommand(dpy, c->transient_for, &c->command, &count);

}

static void
handle_WM_HINTS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wmh) {
		XFree(c->wmh);
		c->wmh = NULL;
		c->group = None;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
		if (c->group) {
			XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->group,
				     ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask |
				     PropertyChangeMask);
		}
		c->dockapp = is_dockapp(c);
	}
}

static void
handle_WM_ICON_NAME(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_ICON_SIZE(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_NAME(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_NORMAL_HINTS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_PROTOCOLS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_SIZE_HINTS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
handle_WM_STATE(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wms) {
		XFree(c->wms);
		c->wms = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete) {
		unmanaged_client(c);
		return;
	}
	if (!(c->wms = XGetWMState(dpy, c->win))) {
		/* XXX: about the only time this should happed would be if the window
		   was destroyed out from under us: check that.  We should get a
		   DestroyNotify soon anyway. */
		return;
	}
	if (c->managed) {
		switch (c->wms->state) {
		case WithdrawnState:
			unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* This is merely a state transition between active states. */
			break;
		}
	} else {
		switch (c->wms->state) {
		case WithdrawnState:
			/* The window manager placed a WM_STATE of WithdrawnState on the
			   window which probably means that it is a dock app that was
			   just managed, but only for WindowMaker work-alikes.
			   Otherwise, per ICCCM, placing withdrawn state on the window
			   means that it is unmanaged. */
			if ((c->dockapp = is_dockapp(c)) && scr->maker_check)
				managed_client(c, e->xproperty.time);
			else
				unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* The window manager place a WM_STATE of other than
			   WithdrawnState on the window which means that it was just
			   managed per ICCCM. */
			managed_client(c, e->xproperty.time);
			break;
		}
	}
}

static void
handle_WM_TRANSIENT_FOR(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->transient_for = None;
	if (e && e->xproperty.state == PropertyDelete)
		return;
	get_window(c->win, XA_WM_TRANSIENT_FOR, AnyPropertyType, &c->transient_for);
}

static void
handle_WM_WINDOW_ROLE(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->role) {
		XFree(c->role);
		c->role = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete)
		return;
	c->role = get_text(c->win, _XA_WM_WINDOW_ROLE);
}

static void
handle_WM_ZOOM_HINTS(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	/* we don't actually process zoom hints */
}

enum {
	XEMBED_EMBEDDED_NOTIFY,
	XEMBED_WINDOW_ACTIVATE,
	XEMBED_WINDOW_DEACTIVATE,
	XEMBED_REQUEST_FOCUS,
	XEMBED_FOCUS_IN,
	XEMBED_FOCUS_OUT,
	XEMBED_FOCUS_NEXT,
	XEMBED_FOCUS_PREV,
	XEMBED_GRAB_KEY,
	XEMBED_UNGRAB_KEY,
	XEMBED_MODALITY_ON,
	XEMBED_MODALITY_OFF,
	XEMBED_REGISTER_ACCELERATOR,
	XEMBED_UNREGISTER_ACCELERATOR,
	XEMBED_ACTIVATE_ACCELERATOR,
};

/* detail for XEMBED_FOCUS_IN: */
enum {
	XEMBED_FOCUS_CURRENT,
	XEMBED_FOCUS_FIRST,
	XEMBED_FOCUS_LAST,
};

enum {
	XEMBED_MODIFIER_SHIFT = (1 << 0),
	XEMBED_MODIFIER_CONTROL = (1 << 1),
	XEMBED_MODIFIER_ALT = (1 << 2),
	XEMBED_MODIFIER_SUPER = (1 << 3),
	XEMBED_MODIFIER_HYPER = (1 << 4),
};

enum {
	XEMBED_ACCELERATOR_OVERLOADED = (1<<0),
};

/* Note:
 *	xclient.data.l[0] = timestamp
 *	xclient.data.l[1] = major opcode
 *	xclient.data.l[2] = detail code
 *	xclient.data.l[3] = data1
 *	xclient.data.l[4] = data2
 *
 * Sent to target window with no event mask and propagation false.
 */

static void
handle_XEMBED(XEvent *e, Client *c)
{
	if (!c || !e || e->type != ClientMessage)
		return;
	if (c->managed)
		return;
	switch (e->xclient.data.l[1]) {
	case XEMBED_EMBEDDED_NOTIFY:
		/* Sent from the embedder to the client on embedding, after reparenting
		   and mapping the client's X window.  A client that receives this
		   message knows that its window was embedded by an XEmbed site and not
		   simply reparented by a window manager. The embedder's window handle is 
		   in data1.  data2 is the protocol version in use. */
		managed_client(c, e->xclient.data.l[0]);
		return;
	case XEMBED_WINDOW_ACTIVATE:
	case XEMBED_WINDOW_DEACTIVATE:
		/* Sent from embedder to client when the window becomes active or
		   inactive (keyboard focus) */
		break;
	case XEMBED_REQUEST_FOCUS:
		/* Sent from client to embedder to request keyboard focus. */
		break;
	case XEMBED_FOCUS_IN:
	case XEMBED_FOCUS_OUT:
		/* Sent from embedder to client when it gets or loses focus. */
		break;
	case XEMBED_FOCUS_NEXT:
	case XEMBED_FOCUS_PREV:
		/* Sent from the client to embedder at the end or start of tab chain. */
		break;
	case XEMBED_GRAB_KEY:
	case XEMBED_UNGRAB_KEY:
		/* Obsolete */
		break;
	case XEMBED_MODALITY_ON:
	case XEMBED_MODALITY_OFF:
		/* Sent from embedder to client when shadowed or released by a modal
		   dialog. */
		break;
	case XEMBED_REGISTER_ACCELERATOR:
	case XEMBED_UNREGISTER_ACCELERATOR:
	case XEMBED_ACTIVATE_ACCELERATOR:
		/* Sent from client to embedder to register an accelerator. 'detail' is
		   the accelerator_id.  For register, data1 is the X key symbol, and
		   data2 is a bitfield of modifiers. */
		break;
	}
	/* XXX: there is a question whether we should manage the client here or not.
	   These other messages indicate that we have probably missed a management event
	   somehow, so mark it managed anyway. */
	managed_client(c, e->xclient.data.l[0]);
}

static void
handle_XEMBED_INFO(XEvent *e, Client *c)
{
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	if (e && e->xproperty.state == PropertyDelete) {
		c->statusicon = False;
		return;
	}
	if ((c->xei = XGetEmbedInfo(dpy, c->win))) {
		c->dockapp = False;
		c->statusicon = True;
		return;
	}

}

static void
handle_NET_WM_STATE(XEvent *e, Client *c)
{
	Atom *atoms = NULL;
	long i, n;

	if ((atoms = get_atoms(e->xany.window, _XA_NET_WM_STATE, AnyPropertyType, &n))) {
		if (!c)
			c = add_client(e->xany.window);
		for (i = 0; i < n; i++)
			if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
				c->focus_time = e->xproperty.time;
				break;
			}
		XFree(atoms);
	}
}

/** @brief track the active window
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the _NET_ACTIVE_WINDOW property on the root window set
  * by EWMH/NetWM compliant window managers to indicate the active window.  We
  * keep track of the last time that each client window was active.
  */
static void
handle_NET_ACTIVE_WINDOW(XEvent *e, Client *c)
{
	Window active = None;

	if (get_window(scr->root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW, &active) && active) {
		if (XFindContext(dpy, active, ClientContext, (XPointer *) &c) != Success)
			c = add_client(active);
		if (e)
			c->active_time = e->xproperty.time;
	}
}

static void
handle_NET_WM_USER_TIME(XEvent *e, Client *c)
{
	if (c && get_time(e->xany.window, _XA_NET_WM_USER_TIME, XA_CARDINAL, &c->user_time))
		pushtime(&last_user_time, c->user_time);
}

static void
handle_CLIENT_LIST(XEvent *e, Atom atom, Atom type)
{
	Window *list;
	long i, n;
	Client *c, **cp;

	if ((list = get_windows(scr->root, atom, type, &n))) {
		for (c = scr->clients; c; c->breadcrumb = False, c->new = False, c = c->next) ;
		for (i = 0, c = NULL; i < n; c = NULL, i++) {
			if (XFindContext(dpy, list[i], ClientContext, (XPointer *) &c) !=
			    Success)
				c = add_client(list[i]);
			c->breadcrumb = True;
		}
		XFree(list);
		for (cp = &scr->clients, c = *cp; c;) {
			if (!c->breadcrumb) {
				*cp = c->next;
				remove_client(c);
				c = *cp;
			} else {
				cp = &c->next;
				c = *cp;
			}
		}
	}
}

static void
handle_NET_CLIENT_LIST(XEvent *e, Client *c)
{
	handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static void
handle_WIN_CLIENT_LIST(XEvent *e, Client *c)
{
	handle_CLIENT_LIST(e, _XA_WIN_CLIENT_LIST, XA_CARDINAL);
}

static void
handle_MANAGER(XEvent *e, Client *c)
{
	char buf[64] = { 0, };
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", scr->screen);
	sel = XInternAtom(dpy, buf, False);
	if ((win = XGetSelectionOwner(dpy, sel))) {
		if ((win != scr->tray)) {
			XSelectInput(dpy, win,
				     StructureNotifyMask | SubstructureNotifyMask |
				     PropertyChangeMask);
			DPRINTF("system tray changed from 0x%08lx to 0x%08lx\n",
				scr->tray, win);
			scr->tray = win;
		}
	} else if (scr->tray) {
		DPRINTF("system tray removed from 0x%08lx\n", scr->tray);
		scr->tray = None;
	}
}

void
handle_atom(XEvent *e, Client *c, Atom atom)
{
	struct atoms *a;

	for (a = atoms; a->atom; a++) {
		if (a->value == atom) {
			if (a->handler)
				a->handler(e, c);
			break;
		}
	}
}

/** @brief handle monitoring events
  * @param e - X event to handle
  *
  * If the window mwnager has a client list, we can chekc for newly mapped
  * window by additions to the client list.  We can calculate the user time by
  * tracking _NET_WM_USER_TMIE and _NET_WM_TIME_WINDOW on all clients.  If the
  * window manager supports _NET_WM_STATE_FOCUSED ro at least
  * _NET_ACTIVE_WINDOW, couples with FocusIn and FocusOut events, we should be
  * able to track the last focused window at the time that the app started (not
  * all apps support _NET_WM_USER_TIME).
  */
void
handle_event(XEvent *e)
{
	Client *c;

	switch (e->type) {
	case PropertyNotify:
		if ((c = find_client(e->xproperty.window)))
			pushtime(&c->last_time, e->xproperty.time);
		handle_atom(e, c, e->xproperty.atom);
		break;
	case FocusIn:
	case FocusOut:
		if ((c = find_client(e->xfocus.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now... */
			managed_client(c, CurrentTime);
		break;
	case Expose:
		if ((c = find_client(e->xexpose.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now... */
			managed_client(c, CurrentTime);
		break;
	case VisibilityNotify:
		if ((c = find_client(e->xvisibility.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now. */
			managed_client(c, CurrentTime);
		break;
	case CreateNotify:
		/* only interested in top-level windows that are not override redirect */
		if (e->xcreatewindow.override_redirect)
			break;
		if (!(c = find_client(e->xcreatewindow.window)))
			if (e->xcreatewindow.parent == scr->root)
				c = add_client(e->xcreatewindow.window);
		break;
	case DestroyNotify:
		if ((c = find_client(e->xdestroywindow.window)))
			del_client(c);
		clean_msgs(e->xdestroywindow.window);
		break;
	case MapNotify:
		if (e->xmap.override_redirect)
			break;
		if ((c = find_client(e->xmap.window)) && !c->managed)
			/* Not sure we should do this for anything but dockapps here...
			   Window managers may map normal windows before setting the
			   WM_STATE property, and system trays invariably map window
			   before sending the _XEMBED message. */
			if ((c->dockapp = is_dockapp(c)))
				/* We missed a management event, so treat the window as
				   managed now */
				managed_client(c, CurrentTime);
		break;
	case UnmapNotify:
		/* we can't tell much from a simple unmap event */
		break;
	case ReparentNotify:
		/* any top-level window that is reparented by the window manager should
		   have WM_STATE set eventually (either before or after the reparenting), 
		   or receive an _XEMBED client message it they are a status icon.  The
		   exception is dock apps: some window managers reparent the dock app and 
		   never set WM_STATE on it (even though WindowMaker does). */
		if (e->xreparent.override_redirect)
			break;
		if ((c = find_client(e->xreparent.window)) && !c->managed) {
			if (e->xreparent.parent == scr->root)
				/* reparented the wrong way */
				break;
			if ((c->statusicon = is_statusicon(c))) {
				/* This is a status icon. */
				if (scr->tray)
					/* Status icons will receive an _XEMBED message
					   from the system tray when managed. */
					break;
				/* It is questionable whether status icons will be
				   reparented at all when there is no system tray... */
				EPRINTF("status icon 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else if (!(c->dockapp = is_dockapp(c))) {
				/* This is a normal window. */
				if (scr->redir_check)
					/* We will receive WM_STATE property change when
					   managed (if there is a window manager
					   present). */
					break;
				/* It is questionable whether regular windows will be
				   reparented at all when there is no window manager... */
				EPRINTF("normal window 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else {
				/* This is a dock app. */
				if (scr->maker_check)
					/* We will receive a WM_STATE property change
					   when managed when a WindowMaker work-alike is
					   present. */
					break;
				/* Non-WindowMaker window managers reparent dock apps
				   without any further indication that the dock app has
				   been managed. */
				managed_client(c, CurrentTime);
			}
		}
		break;
	case ConfigureNotify:
		break;
	case ClientMessage:
		c = find_client(e->xclient.window);
		handle_atom(e, c, e->xclient.message_type);
		break;
	case MappingNotify:
	default:
		break;
	}
}

#ifdef SYSTEM_TRAY_STATUS_ICON

void
icon_activate_cb(GtkStatusIcon *status, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;
}

gboolean
icon_button_press_cb(GtkStatusIcon *status, GdkEvent *event, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

gboolean
icon_button_release_cb(GtkStatusIcon *status, GdkEvent *event, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

gboolean
icon_popup_menu_cb(GtkStatusIcon *status, guint button, guint time, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

gboolean
icon_query_tooltip_cb(GtkStatusIcon *status, gint x, gint y, gboolean
		      keyboard_mode, GtkTooltip *tooltip, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return TRUE;		/* show tooltip */
}

/** @brief process scroll event callback
  * @param status - the GtkStatusIcon emitting the event
  * @param event - the event that cause the callback
  * @param data - client data, the Sequence pointer
  *
  * The scroll-event signal is emitted when a button in the 4 to 7 range is
  * pressed.  Wheel mice are usually configured to generate button press events
  * for buttons 4 and 5 when the wheel is turned.
  */
gboolean
icon_scroll_event_cb(GtkStatusIcon *status, GdkEvent *event, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

/** @brief process size changed event callback
  * @param status - the GtkStatusIcon emmitting the event
  * @param size - the size (in pixels) of the icon
  * @param data - client data, the Sequence pointer
  *
  * Gets emitted when the size available for the image changes, e.g. because the
  * notification area got resized.
  */
gboolean
icon_size_changed_cb(GtkStatusIcon *status, gint size, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* did we scale the icon? otherwise GTK+ will scale */
}

/** @brief create a status icon for the startup sequence
  * @param seq - the sequence for which to create the status icon
  */
GtkStatusIcon *
create_statusicon(Sequence *seq)
{
	GtkStatusIcon *status;

	if ((status = seq->status))
		return (status);
	if (!(status = gtk_status_icon_new())) {
		DPRINTF("could not create status icon\n");
		return (status);
	}

	/* If the sequence has an icon name, use that to set the status icon, otherwise
	   use an application icon from stock. */
	if (seq->f.icon) {
		gtk_status_icon_set_from_icon_name(status, seq->f.icon);
	} else {
		gtk_status_icon_set_from_stock(status, "application");	/* FIXME */
	}

	{
		GdkDisplay *display;
		GdkScreen *screen;

		/* The sequence always belongs to a particular screen, so set the screen
		   for the status icon. */
		display = gdk_display_get_default();
		if (seq->f.screen) {
			screen = gdk_display_get_screen(display, seq->n.screen);
		} else {
			screen = gdk_display_get_default_screen(display);
		}
		gtk_status_icon_set_screen(status, screen);
	}
	{
		char *markup = NULL;

		/* Set the tooltip text as markup */
		if (markup)
			gtk_status_icon_set_tooltip_markup(status, markup);
	}
	{
		const char *title;

		/* Set the statis icon title */
		if (!(title = seq->f.description))
			if (!(title = seq->f.name))
				title = "Startup Notification";
		gtk_status_icon_set_title(status, title);
	}
	{
		const char *name;

		/* Set the status icon name */
		if (!(name = seq->f.name))
			name = NAME;
		gtk_status_icon_set_name(status, name);
	}
	g_signal_connect(G_OBJECT(status), "activate",
			 G_CALLBACK(icon_activate_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "button-press-event",
			 G_CALLBACK(icon_button_press_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "button-release-event",
			 G_CALLBACK(icon_button_release_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "popup-menu",
			 G_CALLBACK(icon_popup_menu_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "query_tooltip",
			 G_CALLBACK(icon_query_tooltip_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "scroll-event",
			 G_CALLBACK(icon_scroll_event_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "size-changed",
			 G_CALLBACK(icon_size_changed_cb), (gpointer) seq);

	gtk_status_icon_set_visible(status, TRUE);
	return (seq->status = status);
}

#endif				/* SYSTEM_TRAY_STATUS_ICON */

#ifdef DESKTOP_NOTIFICATIONS

/** @brief update notifications
  */
void
update_notification(Sequence *seq)
{
	/* FIXME */
}

gboolean
check_notify(void)
{
	if (!notify_is_initted())
		notify_init(NAME);
	return notify_is_initted()? notify_get_server_info(NULL, NULL, NULL,
							   NULL) : False;
}

/** @brief process a desktop notification action callback
  * @param notify - the notification object
  * @param action - the action ("kill" or "continue")
  * @param data - pointer to startup notification sequence
  */
static void
notify_action_callback(NotifyNotification *notify, char *action, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	if (action) {
		if (!strcmp(action, "kill")) {
		} else if (!strcmp(action, "continue")) {
		}
	}
}

static void
notify_closed_callback(NotifyNotification *notify, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;
	gint reason;

	reason = notify_notification_get_closed_reason(notify);
	(void) reason;
	/* FIXME: continue waiting */
	/* But.... might want to do two different actions based on whether there was a
	   timeout or the notification was explicitly closed by the user. If there was an 
	   explicit close, we should probably continue waiting without further
	   notifications, otherwise if it was a timeout we might only continue waiting
	   for another cycle. */
	g_object_unref(G_OBJECT(notify));
}

static gboolean
notify_update_callback(gpointer data)
{
	Sequence *seq = (typeof(seq)) data;
	NotifyNotification *notify;

	if (!(notify = seq->notification)) {
		unref_sequence(seq);
		return FALSE;	/* remove timeout source */
	}
	update_notification(seq);
	return TRUE;		/* continue timeout source */
}

static void
put_sequence(gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	unref_sequence(seq);
}

static Bool
seq_can_kill(Sequence *seq)
{
	/* FIXME */
	return False;
}

/** @brief create a desktop notification
  * @param seq - startup notification sequence
  *
  * We create a desktop notification whenever a startup notification sequence
  * persists for longer than the guard time without being completed.  Two
  * actions can be taken: kill the process (if the process id is known and the
  * host machine is the same as our host, or an XID is known that can be
  * XKill'ed), or continue waiting.  The secnod action is to continue waiting
  * and is the default if the desktop notification times out.
  */
NotifyNotification *
create_notification(Sequence *seq)
{
	NotifyNotification *notify;
	char *summary, *body;

	summary = "Application taking too long to start.";
	/* TODO: create a nice body message describing which application is taking too
	   long, how much time has elapsed since startup, and possibly other information. 
	 */
	body = NULL;
	/* This is the icon theme icon name or filename.  Filenames should be avoided.
	   This information comes from the ICON= field of the startup notification or can 
	   be derived from the APPLICATION_ID= field of the startup notification, or
	   could be a default application image. */
	/* Note that if we put some information in the body (such as expired time) we
	   might want to set a 1-second timeout and call notify_notification_update() on
	   a one second basis. */
	if (!(notify = seq->notification)) {
		if (!(notify = notify_notification_new(summary, body, seq->f.icon))) {
			DPRINTF("could not create notification\n");
			return (NULL);
		}
	} else {
		notify_notification_update(notify, summary, body, seq->f.icon);
		notify_notification_clear_hints(notify);
		notify_notification_clear_actions(notify);
	}
	notify_notification_set_timeout(notify, NOTIFY_EXPIRES_DEFAULT);
	notify_notification_set_category(notify, "some_category");	/* FIXME */
	notify_notification_set_urgency(notify, NOTIFY_URGENCY_NORMAL);
//      notify_notification_set_image_from_pixbuf(notify, seq->pixbuf);
	notify_notification_set_hint(notify, "some_hint", NULL);	/* FIXME */
	if (seq_can_kill(seq)) {
		notify_notification_add_action(notify, "kill", "Kill",
					       NOTIFY_ACTION_CALLBACK
					       (notify_action_callback),
					       (gpointer) ref_sequence(seq),
					       put_sequence);
		notify_notification_add_action(notify, "continue", "Continue",
					       NOTIFY_ACTION_CALLBACK
					       (notify_action_callback),
					       (gpointer) ref_sequence(seq),
					       put_sequence);
	}
	g_signal_connect(G_OBJECT(notify), "closed", G_CALLBACK(notify_closed_callback),
			 (gpointer) seq);
#define UPDATE_INTERVAL 2
	g_timeout_add_seconds(UPDATE_INTERVAL, notify_update_callback, (gpointer) seq);
	if (!notify_notification_show(notify, NULL)) {
		DPRINTF("could not show notification\n");
		g_object_unref(G_OBJECT(notify));
		seq->notification = NULL;
		return (NULL);
	}
	seq->notification = notify;
	return (notify);
}

#endif				/* DESKTOP_NOTIFICATIONS */

/** @brief set up for monitoring
  *
  * We want to establish the client lists, dock app lists and system tray lists
  * before we first enter the event loop (whether in daemon node or not) and
  * then syncronize to the X server so that we do not consider window that were
  * mapped before a startup notification as being part of a startup notification
  * completion.
  */
void
setup_to_monitor()
{
#if 0
	handle_NET_CLIENT_LIST(NULL, NULL);
	handle_WIN_CLIENT_LIST(NULL, NULL);
	handle_NET_ACTIVE_WINDOW(NULL, NULL);
#endif
}

int signum;

void
sighandler(int sig)
{
	signum = sig;
}

#if defined DESKTOP_NOTIFICATIONS || defined SYSTEM_TRAY_STATUS_ICON
#undef HAVE_GLIB_EVENT_LOOP
#define HAVE_GLIB_EVENT_LOOP 1
#endif

#if defined SYSTEM_TRAY_STATUS_ICON
#undef HAVE_GTK
#define HAVE_GTK 1
#endif

#ifdef HAVE_GLIB_EVENT_LOOP
gboolean
on_xfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		XEvent ev;

		while (XPending(dpy) && running) {
			XNextEvent(dpy, &ev);
			handle_event(&ev);
		}
	}
	return TRUE;		/* keep event source */

}
#endif				/* HAVE_GLIB_EVENT_LOOP */

/** @brief monitor startup and assist the window manager.
  */
void
do_monitor()
{
	pid_t pid;

	setup_to_monitor();
	XSync(dpy, False);
	if ((pid = fork()) < 0) {
		EPRINTF("%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	} else if (pid != 0) {
		/* parent returns with new connection */
		XCloseDisplay(dpy);
		return;
	}
	setsid();		/* become a session leader */
	/* close files */
	fclose(stdin);
	/* fork once more for SVR4 */
	if ((pid = fork()) < 0) {
		EPRINTF("%s\n", strerror(errno));
		exit(EXIT_SUCCESS);
	} else if (pid != 0) {
		/* parent exits */
		exit(EXIT_SUCCESS);
	}
	umask(0);
	/* continue on monitoring */
#ifdef HAVE_GLIB_EVENT_LOOP
	{
		int xfd;
		GIOChannel *chan;
		gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
		guint srce;

		xfd = ConnectionNumber(dpy);
		chan = g_io_channel_unix_new(xfd);
		srce = g_io_add_watch(chan, mask, on_xfd_watch, NULL);
		(void) srce;

#ifdef HAVE_GTK
		gtk_main();
#else				/* HAVE_GTK */
		{
			GMainLoop *loop = g_main_loop_new(NULL, FALSE);

			g_main_loop_run(loop);
		}
#endif				/* HAVE_GTK */
	}
#else				/* HAVE_GLIB_EVENT_LOOP */
	{
		int xfd;
		XEvent ev;

		signal(SIGHUP, sighandler);
		signal(SIGINT, sighandler);
		signal(SIGTERM, sighandler);
		signal(SIGQUIT, sighandler);

		/* main event loop */
		running = True;
		XSync(dpy, False);
		xfd = ConnectionNumber(dpy);
		while (running) {
			struct pollfd pfd = { xfd, POLLIN | POLLHUP | POLLERR, 0 };

			if (signum)
				exit(EXIT_SUCCESS);

			if (poll(&pfd, 1, -1) == -1) {
				switch (errno) {
				case EINTR:
				case EAGAIN:
				case ERESTART:
					continue;
				}
				EPRINTF("poll: %s\n", strerror(errno));
				fflush(stderr);
				exit(EXIT_FAILURE);
			}
			if (pfd.revents & (POLLNVAL | POLLHUP | POLLERR)) {
				EPRINTF("poll: error\n");
				fflush(stderr);
				exit(EXIT_FAILURE);
			}
			if (pfd.revents & (POLLIN)) {
				while (XPending(dpy) && running) {
					XNextEvent(dpy, &ev);
					handle_event(&ev);
				}
			}
		}
	}
#endif				/* HAVE_GLIB_EVENT_LOOP */
}

static void
copying(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2008-2014  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>\n\
Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>\n\
\n\
All Rights Reserved.\n\
--------------------------------------------------------------------------------\n\
This program is free software: you can  redistribute it  and/or modify  it under\n\
the terms of the  GNU Affero  General  Public  License  as published by the Free\n\
Software Foundation, version 3 of the license.\n\
\n\
This program is distributed in the hope that it will  be useful, but WITHOUT ANY\n\
WARRANTY; without even  the implied warranty of MERCHANTABILITY or FITNESS FOR A\n\
PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.\n\
\n\
You should have received a copy of the  GNU Affero General Public License  along\n\
with this program.   If not, see <http://www.gnu.org/licenses/>, or write to the\n\
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\
--------------------------------------------------------------------------------\n\
U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on behalf\n\
of the U.S. Government (\"Government\"), the following provisions apply to you. If\n\
the Software is supplied by the Department of Defense (\"DoD\"), it is classified\n\
as \"Commercial  Computer  Software\"  under  paragraph  252.227-7014  of the  DoD\n\
Supplement  to the  Federal Acquisition Regulations  (\"DFARS\") (or any successor\n\
regulations) and the  Government  is acquiring  only the  license rights granted\n\
herein (the license rights customarily provided to non-Government users). If the\n\
Software is supplied to any unit or agency of the Government  other than DoD, it\n\
is  classified as  \"Restricted Computer Software\" and the Government's rights in\n\
the Software  are defined  in  paragraph 52.227-19  of the  Federal  Acquisition\n\
Regulations (\"FAR\")  (or any successor regulations) or, in the cases of NASA, in\n\
paragraph  18.52.227-86 of  the  NASA  Supplement  to the FAR (or any  successor\n\
regulations).\n\
--------------------------------------------------------------------------------\n\
", NAME " " VERSION);
}

static void
version(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014  Monavacon Limited.\n\
Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008  OpenSS7 Corporation.\n\
Copyright (c) 1997, 1998, 1999, 2000, 2001  Brian F. G. Bidulock.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Distributed by OpenSS7 under GNU Affero General Public License Version 3,\n\
with conditions, incorporated herein by reference.\n\
\n\
See `%1$s --copying' for copying permissions.\n\
", NAME, PACKAGE, VERSION);
}

static void
usage(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command Options:\n\
    [-m, --monitor]\n\
        run as monitoring process, default if no other command\n\
        option is specified\n\
    -r, --replace\n\
        restart %1$s by replacing the existing running process\n\
        with the current one\n\
    -q, --quit\n\
        ask a running %1$s process to stop\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General Options:\n\
    -f, --foreground\n\
        run in the foreground for debugging (so that standard error\n\
        goes to the terminal)\n\
    -g, --guard-time TIMEOUT\n\
        amount of time, TIMEOUT, in milliseconds to wait for a\n\
        desktop application to launch [default: 15000]\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: 0]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: 1]\n\
        this option may be repeated.\n\
", argv[0]);
}

int
main(int argc, char *argv[])
{
	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"monitor",	no_argument,		NULL, 'm'},
			{"replace",	no_argument,		NULL, 'r'},
			{"quit",	no_argument,		NULL, 'q'},
			{"foreground",	no_argument,		NULL, 'f'},
			{"guard-time",	required_argument,	NULL, 'g'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "mrqfg:D::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getop(argc, argv, "mrqfg:DvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'm':	/* -m, --monitor */
			if (options.exec_mode != EXEC_MODE_MONITOR)
				goto bad_command;
			options.exec_mode = EXEC_MODE_MONITOR;
			break;
		case 'r':	/* -r, --replace */
			if (options.exec_mode != EXEC_MODE_MONITOR
			    && options.exec_mode != EXEC_MODE_REPLACE)
				goto bad_command;
			options.exec_mode = EXEC_MODE_REPLACE;
			break;
		case 'q':	/* -q, --quit */
			if (options.exec_mode != EXEC_MODE_MONITOR
			    && options.exec_mode != EXEC_MODE_QUIT)
				goto bad_command;
			options.exec_mode = EXEC_MODE_QUIT;
			break;
		case 'f':	/* -f, --foreground */
			options.foreground = 1;
			break;
		case 'g':	/* -g, --guard-time TIMEOUT */
			if (!optarg)
				options.guardtime = 15000;
			else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.guardtime = val;
			}
			break;

		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n",
					argv[0]);
			if (!optarg) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n",
					argv[0]);
			if (!optarg) {
				options.output++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(EXIT_SUCCESS);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n",
					argv[0]);
			version(argc, argv);
			exit(EXIT_SUCCESS);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n",
					argv[0]);
			copying(argc, argv);
			exit(EXIT_SUCCESS);
		case '?':
		default:
		      bad_option:
			optind--;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '",
						argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument",
						argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		      bad_command:
			fprintf(stderr, "%s: only one command option allowed\n", argv[0]);
			goto bad_usage;
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind < argc)
		goto bad_nonopt;
	get_display();
	do_monitor();
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker: