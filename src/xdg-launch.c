/*****************************************************************************

 Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>
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

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

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
#include <wordexp.h>

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
#ifdef RECENTLY_USED
#include <glib.h>
#include <gio/gio.h>
#endif
// #undef RECENTLY_USED_XBEL    /* FIXME */
#ifdef RECENTLY_USED_XBEL
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
					} } while (0)

const char *program = NAME;

char **eargv = NULL;
int eargc = 0;

typedef struct {
	int debug;
	int output;
	char *appid;
	char *launcher;
	char *launchee;
	char *sequence;
	char *hostname;
	int monitor;
	int screen;
	int desktop;
	Time timestamp;
	char *name;
	char *icon;
	char *binary;
	char *description;
	char *wmclass;
	char *silent;
	char *exec;
	char *file;
	char *url;
	pid_t pid;
	char *id;
	Bool keyboard;
	Bool pointer;
	char *action;
	Bool xsession;
	Bool autostart;
	char *path;
	char *uri;
	char *rawcmd;
	char *runhist;
	char *recapps;
	char *recently;
	char *recent;
	int keep;
	Bool info;
	Bool toolwait;
	int timeout;
	int mappings;
	Bool withdrawn;
	Bool printpid;
	Bool printwid;
	Bool noprop;
	Bool manager;
	Bool tray;
	Bool pager;
	Bool composite;
	Bool assist;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.appid = NULL,
	.launcher = NULL,
	.launchee = NULL,
	.sequence = NULL,
	.hostname = NULL,
	.monitor = -1,
	.screen = -1,
	.desktop = -1,
	.timestamp = 0,
	.name = NULL,
	.icon = NULL,
	.binary = NULL,
	.description = NULL,
	.wmclass = NULL,
	.silent = NULL,
	.exec = NULL,
	.file = NULL,
	.url = NULL,
	.pid = 0,
	.id = NULL,
	.keyboard = False,
	.pointer = False,
	.action = NULL,
	.xsession = False,
	.autostart = False,
	.path = NULL,
	.uri = NULL,
	.rawcmd = NULL,
	.runhist = NULL,
	.recapps = NULL,
	.recently = NULL,
	.recent = NULL,
	.keep = 0,
	.info = False,
	.toolwait = False,
	.timeout = 0,
	.mappings = 0,
	.withdrawn = False,
	.printpid = False,
	.printwid = False,
	.noprop = False,
	.manager = False,
	.tray = False,
	.pager = False,
	.composite = False,
	.assist = False,
};

Options defaults = {
	.appid = "[APPID]",
	.launcher = NAME,
	.launchee = "[APPID]",
	.sequence = "0",
	.hostname = "gethostname()",
	.monitor = -1,
	.screen = -1,
	.desktop = -1,
	.timestamp = 0,
	.name = "[Name=]",
	.icon = "[Icon=]",
	.binary = "[TryExec=|Exec=]",
	.description = "[Comment=]",
	.wmclass = "[StartupWMClass=]",
	.silent = "0",
	.file = "",
	.url = "",
	.pid = 0,
	.id = "$DESKTOP_STARTUP_ID",
	.keyboard = False,
	.pointer = False,
	.action = "none",
	.xsession = False,
	.autostart = False,
	.path = NULL,
	.uri = NULL,
	.runhist = "~/.config/xde/run-history",
	.recapps = "~/.config/xde/recent-applications",
	.recently = "~/.local/share/recently-used",
	.recent = NULL,
	.keep = 10,
	.info = False,
	.toolwait = False,
	.timeout = 15,
	.mappings = 1,
	.withdrawn = False,
	.printpid = False,
	.printwid = False,
	.noprop = False,
	.manager = False,
	.tray = False,
	.pager = False,
	.composite = False,
	.assist = False,
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

struct fields fields = { NULL, };

typedef enum {
	StartupNotifyIdle,
	StartupNotifyNew,
	StartupNotifyChanged,
	StartupNotifyComplete,
} StartupNotifyState;

typedef struct _Client Client;

typedef struct _Sequence {
	struct _Sequence *next;
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
} Sequence;

Sequence *sequences;

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

#ifdef STARTUP_NOTIFICATION
typedef struct Notify Notify;

struct Notify {
	Notify *next;
	SnStartupSequence *seq;
	Bool assigned;
};
#endif

static const char *DesktopEntryFields[] = {
	"Type",
	"Name",
	"Comment",
	"Icon",
	"TryExec",
	"Exec",
	"Terminal",
	"StartupNotify",
	"StartupWMClass",
	"SessionSetup",
	"Categories",
	"MimeType",
	"AsRoot",
	"AutostartPhase",
	NULL
};

struct entry {
	char *Type;
	char *Name;
	char *Comment;
	char *Icon;
	char *TryExec;
	char *Exec;
	char *Terminal;
	char *StartupNotify;
	char *StartupWMClass;
        char *SessionSetup;
	char *Categories;
	char *MimeType;
	char *AsRoot;
	char *AutostartPhase;
};

struct entry entry = { NULL, };

Display *dpy = NULL;
int screen;
Window root;

#ifdef STARTUP_NOTIFICATION
SnDisplay *sn_dpy;
SnMonitorContext *sn_ctx;
#endif

struct _Client {
	Client *next;
	int screen;
	Window win;			/* the client window */
	Window time_win;		/* the time window */
	Window group;			/* the group window */
	Window icon_win;		/* the icon window */
	int state;			/* WM_STATE */
	Bool dockapp;			/* this client is a dockapp */
	Bool breadcrumb;
	Bool new;
	Bool managed;			/* set when client managed */
	Bool counted;			/* set when client counted */
	Time active_time;
	Time focus_time;
	Time user_time;
	pid_t pid;
	char *startup_id;
	char **command;			/* words in WM_COMMAND */
	int count;			/* count of words in WM_COMMAND */
	char *name;
	char *hostname;
	XClassHint ch;
	XWMHints *wmh;
#ifdef STARTUP_NOTIFICATION
	SnStartupSequence *seq;
#endif
};

Client *clients = NULL;

typedef struct {
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;
	Window stray_owner;		/* _NET_WM_SYSTEM_TRAY_S%d owner */
	Window pager_owner;		/* _NET_DESKTOP_LAYOUT_S%d owner */
	Window compm_owner;		/* _NET_WM_CM_S%d owner */
	Window shelp_owner;		/* _XDG_ASSIST_S%d owner */
} WindowManager;

WindowManager wm;

Time last_user_time = CurrentTime;
Time launch_time = CurrentTime;

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
Atom _XA_NET_WM_DESKTOP;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_STATE;
Atom _XA_WM_CLASS;
Atom _XA_NET_SYSTEM_TRAY_ORIENTATION;
Atom _XA_NET_SYSTEM_TRAY_VISUAL;
Atom _XA_NET_DESKTOP_LAYOUT;
Atom _XA_XDG_ASSIST_CMD;
Atom _XA_XDG_ASSIST_CMD_QUIT;
Atom _XA_XDG_ASSIST_CMD_REPLACE;

static Bool handle_MANAGER(XEvent *, Client *);
static Bool handle_MOTIF_WM_INFO(XEvent *, Client *);
static Bool handle_NET_ACTIVE_WINDOW(XEvent *, Client *);
static Bool handle_NET_CLIENT_LIST(XEvent *, Client *);
static Bool handle_NET_STARTUP_INFO_BEGIN(XEvent *, Client *);
static Bool handle_NET_STARTUP_INFO(XEvent *, Client *);
static Bool handle_NET_SUPPORTED(XEvent *, Client *);
static Bool handle_NET_SUPPORTING_WM_CHECK(XEvent *, Client *);
static Bool handle_NET_WM_STATE(XEvent *, Client *);
static Bool handle_NET_WM_USER_TIME(XEvent *, Client *);
static Bool handle_NET_WM_DESKTOP(XEvent *, Client *);
static Bool handle_WIN_CLIENT_LIST(XEvent *, Client *);
static Bool handle_WINDOWMAKER_NOTICEBOARD(XEvent *, Client *);
static Bool handle_WIN_PROTOCOLS(XEvent *, Client *);
static Bool handle_WIN_SUPPORTING_WM_CHECK(XEvent *, Client *);
static Bool handle_WIN_WORKSPACE(XEvent *, Client *);
static Bool handle_WM_CLIENT_MACHINE(XEvent *, Client *);
static Bool handle_WM_COMMAND(XEvent *, Client *);
static Bool handle_WM_HINTS(XEvent *, Client *);
static Bool handle_WM_STATE(XEvent *, Client *);
static Bool handle_WM_CLASS(XEvent *, Client *);
static Bool handle_NET_SYSTEM_TRAY_ORIENTATION(XEvent *, Client *);
static Bool handle_NET_SYSTEM_TRAY_VISUAL(XEvent *, Client *);
static Bool handle_NET_DESKTOP_LAYOUT(XEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	Bool (*handler) (XEvent *, Client *);
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global					handler					atom value		*/
	/* ----					------					-------					----------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,		NULL,					None			},
	{ "MANAGER",				&_XA_MANAGER,				&handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,			&handle_MOTIF_WM_INFO,			None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,			&handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,			&handle_NET_CLIENT_LIST,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,		NULL,					None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,			NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,		&handle_NET_STARTUP_INFO_BEGIN,		None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,			&handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,			&handle_NET_SUPPORTED,			None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,		&handle_NET_SUPPORTING_WM_CHECK,	None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,			NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,		NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,		NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE_FOCUSED",		&_XA_NET_WM_STATE_FOCUSED,		NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,			&handle_NET_WM_STATE,			None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,		NULL,					None			},
	{ "_NET_WM_DESKTOP",			&_XA_NET_WM_DESKTOP,			&handle_NET_WM_DESKTOP,			None,			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,			&handle_NET_WM_USER_TIME,		None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,			NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,			NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,			&handle_WIN_CLIENT_LIST,		None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,		&handle_WINDOWMAKER_NOTICEBOARD,	None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,		&handle_WIN_SUPPORTING_WM_CHECK,	None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,			&handle_WIN_PROTOCOLS,			None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,			&handle_WIN_WORKSPACE,			None			},
	{ "WM_CLIENT_MACHINE",			NULL,					&handle_WM_CLIENT_MACHINE,		XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,					&handle_WM_COMMAND,			XA_WM_COMMAND		},
	{ "WM_HINTS",				NULL,					&handle_WM_HINTS,			XA_WM_HINTS		},
	{ "WM_STATE",				&_XA_WM_STATE,				&handle_WM_STATE,			None			},
	{ "WM_CLASS",				NULL,					&handle_WM_CLASS,			XA_WM_CLASS		},
	{ "_NET_SYSTEM_TRAY_ORIENTATION",	&_XA_NET_SYSTEM_TRAY_ORIENTATION,	&handle_NET_SYSTEM_TRAY_ORIENTATION,	None			},
	{ "_NET_SYSTEM_TRAY_VISUAL",		&_XA_NET_SYSTEM_TRAY_VISUAL,		&handle_NET_SYSTEM_TRAY_VISUAL,		None			},
	{ "_NET_DESKTOP_LAYOUT",		&_XA_NET_DESKTOP_LAYOUT,		&handle_NET_DESKTOP_LAYOUT,		None			},
	{ "_XDG_ASSIST_CMD_QUIT",		&_XA_XDG_ASSIST_CMD_QUIT,		NULL,					None			},
	{ "_XDG_ASSIST_CMD_REPLACE",		&_XA_XDG_ASSIST_CMD_REPLACE,		NULL,					None			},
	{ "_XDG_ASSIST_CMD",			&_XA_XDG_ASSIST_CMD,			NULL,					None			},
	{ NULL,					NULL,					NULL,					None			}
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
		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		XSetErrorHandler(handler);
		for (screen = 0; screen < ScreenCount(dpy); screen++) {
			root = RootWindow(dpy, screen);
			XSelectInput(dpy, root,
				     VisibilityChangeMask | StructureNotifyMask |
				     SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
#ifdef STARTUP_NOTIFICATION
		sn_dpy = sn_display_new(dpy, NULL, NULL);
#endif
		screen = DefaultScreen(dpy);
		root = RootWindow(dpy, screen);
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

	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check, StructureNotifyMask | PropertyChangeMask);
			XFree(data);
			data = NULL;
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty(dpy, check, atom, 0L, 1L, False, type, &real,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success && format != 0) {
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

	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check, StructureNotifyMask | PropertyChangeMask);
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
	if (XGetWindowProperty(dpy, root, protocols, 0L, num, False,
			       XA_ATOM, &real, &format, &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			DPRINTF("trying harder with num = %lu\n", num);
			goto try_harder;
		}
		if (nitems > 0) {
			unsigned long i;
			Atom *atoms;

			atoms = (Atom *) data;
			for (i = 0; i < nitems; i++) {
				if (atoms[i] == supported) {
					result = True;
					break;
				}
			}
		}
		if (data)
			XFree(data);
	}
	return result;
}

static Window
check_anywm()
{
	if (wm.netwm_check)
		return True;
	if (wm.winwm_check)
		return True;
	if (wm.maker_check)
		return True;
	if (wm.motif_check)
		return True;
	if (wm.icccm_check)
		return True;
	if (wm.redir_check)
		return True;
	return False;
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
		return root;
	return check_nonrecursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

/** @brief Check for an EWMH/NetWM compliant (sorta) window manager.
  */
static Window
check_netwm()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	win = win ?: check_netwm_supported();
	if (win && win != wm.netwm_check)
		DPRINTF("NetWM/EWMH changed from 0x%08lx to 0x%08lx\n", wm.netwm_check, win);
	if (!win && wm.netwm_check)
		DPRINTF("NetWM/EWMH removed from 0x%08lx\n", wm.netwm_check);
	return (wm.netwm_check = win);
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
		return root;
	return check_nonrecursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

/** @brief Check for a GNOME1/WMH/WinWM compliant window manager.
  */
static Window
check_winwm()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	win = win ?: check_winwm_supported();
	if (win && win != wm.winwm_check)
		DPRINTF("WinWM/WMH changed from 0x%08lx to 0x%08lx\n", wm.winwm_check, win);
	if (!win && wm.winwm_check)
		DPRINTF("WinWM/WMH removed from 0x%08lx\n", wm.winwm_check);
	return (wm.winwm_check = win);
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	if (win && win != wm.maker_check)
		DPRINTF("WindowMaker changed from 0x%08lx to 0x%08lx\n", wm.maker_check, win);
	if (!win && wm.maker_check)
		DPRINTF("WindowMaker removed from 0x%08lx\n", wm.maker_check);
	return (wm.maker_check = win);
}

/** @brief Check for an OSF/Motif compliant window manager.
  */
static Window
check_motif()
{
	int i = 0;
	long *data, n = 0;
	Window win = None;

	do {
		if ((data = get_cardinals(root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n))) {
			if (n >= 2 && (win = data[1]))
				XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			XFree(data);
		}
	} while (i++ < 2 && !data);
	if (win && win != wm.motif_check)
		DPRINTF("OSF/MOTIF changed from 0x%08lx to 0x%08lx\n", wm.motif_check, win);
	if (!win && wm.motif_check)
		DPRINTF("OSF/MOTIF removed from 0x%08lx\n", wm.motif_check);
	return (wm.motif_check = win);
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	char buf[32];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "WM_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != wm.icccm_check)
		DPRINTF("ICCCM 2.0 WM changed from 0x%08lx to 0x%08lx\n", wm.icccm_check, win);
	if (!win && wm.icccm_check)
		DPRINTF("ICCCM 2.0 WM removed from 0x%08lx\n", wm.icccm_check);
	return (wm.icccm_check = win);
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
	Window win = None;

	OPRINTF("checking redirection for screen %d\n", screen);
	if (XGetWindowAttributes(dpy, root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			win = root;
	if (win && win != wm.redir_check)
		DPRINTF("WM redirection changed from 0x%08lx to 0x%08lx\n", wm.redir_check, win);
	if (!win && wm.redir_check)
		DPRINTF("WM redirection removed from 0x%08lx\n", wm.redir_check);
	return (wm.redir_check = win);
}

/** @brief Find window manager and compliance for the current screen.
  */
static Bool
check_window_manager()
{
	OPRINTF("checking wm compliance for screen %d\n", screen);
	OPRINTF("checking redirection\n");
	if (check_redir())
		OPRINTF("redirection on window 0x%lx\n", wm.redir_check);
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm())
		OPRINTF("ICCCM 2.0 window 0x%lx\n", wm.icccm_check);
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif())
		OPRINTF("OSF/Motif window 0x%lx\n", wm.motif_check);
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker())
		OPRINTF("WindowMaker window 0x%lx\n", wm.maker_check);
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm())
		OPRINTF("GNOME/WMH window 0x%lx\n", wm.winwm_check);
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm())
		OPRINTF("NetWM/EWMH window 0x%lx\n", wm.netwm_check);
	return check_anywm();
}

static Bool
handle_wmchange(XEvent *e, Client *c)
{
	if (!e || e->type == PropertyNotify)
		if (!check_window_manager())
			check_window_manager();
	return True;
}

static Bool
handle_WINDOWMAKER_NOTICEBOARD(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_MOTIF_WM_INFO(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_NET_SUPPORTING_WM_CHECK(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_NET_SUPPORTED(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_WIN_SUPPORTING_WM_CHECK(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_WIN_PROTOCOLS(XEvent *e, Client *c)
{
	return handle_wmchange(e, c);
}

static Bool
handle_WIN_WORKSPACE(XEvent *e, Client *c)
{
	return False;
}

/** @brief Check for a system tray.
  */
static Window
check_stray()
{
	char buf[64];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel))) {
		if (win != wm.stray_owner) {
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("system tray changed from 0x%08lx to 0x%08lx\n", wm.stray_owner, win);
			wm.stray_owner = win;
		}
	} else if (wm.stray_owner) {
		DPRINTF("system tray removed from 0x%08lx\n", wm.stray_owner);
		wm.stray_owner = None;
	}
	return wm.stray_owner;
}

static Bool
handle_NET_SYSTEM_TRAY_ORIENTATION(XEvent *e, Client *c)
{
	return False;
}

static Bool
handle_NET_SYSTEM_TRAY_VISUAL(XEvent *e, Client *c)
{
	return False;
}

static Window
check_pager()
{
	char buf[64];
	Atom sel;
	Window win;
	long *cards, n = 0;

	snprintf(buf, sizeof(buf), "_NET_DESKTOP_LAYOUT_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	/* selection only held while setting _NET_DESKTOP_LAYOUT */
	if (!win && (cards = get_cardinals(root, _XA_NET_DESKTOP_LAYOUT, XA_CARDINAL, &n))
	    && n >= 4) {
		XFree(cards);
		win = root;
	}
	if (win && win != wm.pager_owner)
		DPRINTF("desktop pager changed from 0x%08lx to 0x%08lx\n", wm.pager_owner, win);
	if (!win && wm.pager_owner)
		DPRINTF("desktop pager removed from 0x%08lx\n", wm.pager_owner);
	return (wm.pager_owner = win);
}

static Bool
handle_NET_DESKTOP_LAYOUT(XEvent *e, Client *c)
{
	if (!e || e->type != PropertyNotify)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		if (!wm.pager_owner)
			wm.pager_owner = root;
		break;
	case PropertyDelete:
		if (wm.pager_owner &&  wm.pager_owner == root)
			wm.pager_owner = None;
		break;
	}
	return True;
}

static Window
check_compm()
{
	char buf[64];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_NET_WM_CM_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != wm.compm_owner)
		DPRINTF("composite manager changed from 0x%08lx to 0x%08lx\n", wm.compm_owner, win);
	if (!win && wm.compm_owner)
		DPRINTF("composite manager removed from 0x%08lx\n", wm.compm_owner);
	return (wm.compm_owner = win);
}

static Window
check_shelp()
{
	char buf[64];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_XDG_ASSIST_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != wm.shelp_owner)
		DPRINTF("startup helper changed from 0x%08lx to 0x%08lx\n", wm.shelp_owner, win);
	if (!win && wm.shelp_owner)
		DPRINTF("startup helper removed from 0x%08lx\n", wm.shelp_owner);
	return (wm.shelp_owner = win);
}


Bool
set_screen_of_root(Window sroot, int *screen)
{
	int s;

	for (s = 0; s < ScreenCount(dpy); s++) {
		if ((root = RootWindow(dpy, s)) == sroot) {
			*screen = s;
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

	vroots = get_windows(root, _XA_NET_VIRTUAL_ROOTS, XA_WINDOW, &n);
	get_window(root, _XA__SWM_VROOT, XA_WINDOW, &vroot);

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
find_focus_screen(int *screen)
{
	Window frame, froot;
	int di;
	unsigned int du;

	if (!(frame = get_focus_frame()))
		return False;

	if (!XGetGeometry(dpy, frame, &froot, &di, &di, &du, &du, &du, &du))
		return False;

	return set_screen_of_root(froot, screen);
}

Bool
find_pointer_screen(int *screen)
{
	Window proot = None, dw;
	int di;
	unsigned int du;

	if (XQueryPointer(dpy, root, &proot, &dw, &di, &di, &di, &di, &du))
		return True;
	return set_screen_of_root(proot, screen);
}

Bool
find_window_screen(Window w, int *screen)
{
	Window wroot, dw, *dwp;
	unsigned int du;

	if (!XQueryTree(dpy, w, &wroot, &dw, &dwp, &du))
		return False;
	if (dwp)
		XFree(dwp);

	return set_screen_of_root(wroot, screen);
}

void
set_screen()
{
	free(fields.screen);
	fields.screen = calloc(64, sizeof(*fields.screen));
	if (options.screen != -1 && 0 <= options.screen && options.screen < ScreenCount(dpy))
		snprintf(fields.screen, 64, "%d", options.screen);
	else if (options.keyboard && find_focus_screen(&options.screen))
		snprintf(fields.screen, 64, "%d", options.screen);
	else if (options.pointer && find_pointer_screen(&options.screen))
		snprintf(fields.screen, 64, "%d", options.screen);
	else if (!options.keyboard && !options.pointer &&
		 (find_focus_screen(&options.screen) || find_pointer_screen(&options.screen)))
		snprintf(fields.screen, 64, "%d", options.screen);
	else {
		options.screen = DefaultScreen(dpy);
		root = DefaultRootWindow(dpy);
		snprintf(fields.screen, 64, "%d", options.screen);
	}
}

static int
segm_overlap(int min1, int max1, int min2, int max2)
{
	int tmp, res = 0;

	/* *INDENT-OFF* */
	if (min1 > max1) { tmp = min1; min1 = max1; max1 = tmp; }
	if (min2 > max2) { tmp = min2; min2 = max2; max2 = tmp; }
	/* *INDENT-ON* */

	if (min1 <= min2 && max1 >= min2)
		// min1 min2 (max2?) max1 (max2?)
		res = (max2 <= max1) ? max2 - min2 : max1 - min2;
	else if (min1 <= max2 && max1 >= max2)
		// (min2?) min1 (min2?) max2 max1
		res = (min2 >= min1) ? max2 - min2 : max2 - min1;
	else if (min2 <= min1 && max2 >= min1)
		// min2 min1 (max1?) max2 (max1?)
		res = (max1 <= max2) ? max1 - min1 : max2 - min1;
	else if (min2 <= max1 && max2 >= max1)
		// (min1?) min2 (min1?) max1 max2
		res = (min1 <= min2) ? max1 - min2 : max1 - min1;
	return res;
}

static int
area_overlap(int xmin1, int ymin1, int xmax1, int ymax1, int xmin2, int ymin2, int xmax2, int ymax2)
{
	int w = 0, h = 0;

	w = segm_overlap(xmin1, xmax1, xmin2, xmax2);
	h = segm_overlap(ymin1, ymax1, ymin2, ymax2);

	return (w && h) ? (w * h) : 0;
}

Bool
find_focus_monitor(int *monitor)
{
#if defined XINERAMA || defined XRANDR
	Window frame, froot;
	int x, y, xmin, xmax, ymin, ymax, a, area, best, di;
	unsigned int w, h, b, du;

	if (!(frame = get_focus_frame()))
		return False;

	if (!XGetGeometry(dpy, frame, &froot, &x, &y, &w, &h, &b, &du))
		return False;

	xmin = x;
	xmax = x + w + 2 * b;
	ymin = y;
	ymax = y + h + 2 * b;
#ifdef XINERAMA
	if (XineramaQueryExtension(dpy, &di, &di) && XineramaIsActive(dpy)) {
		int i, n;
		XineramaScreenInfo *si;

		if (!(si = XineramaQueryScreens(dpy, &n)) || n < 2)
			goto no_xinerama;
		for (best = 0, area = 0, i = 0; i < n; i++) {
			if ((a = area_overlap(xmin, ymin, xmax, ymax,
					      si[i].x_org,
					      si[i].y_org,
					      si[i].x_org + si[i].width,
					      si[i].y_org + si[i].height)) > area) {
				area = a;
				best = i;
			}
		}
		XFree(si);
		if (area) {
			*monitor = best;
			return True;
		}
	}
      no_xinerama:
#endif				/* XINERAMA */
#ifdef XRANDR
	if (XRRQueryExtension(dpy, &di, &di)) {
		XRRScreenResources *sr;
		int i, n;

		if (!(sr = XRRGetScreenResources(dpy, root)) || sr->ncrtc < 2)
			goto no_xrandr;
		n = sr->ncrtc;
		for (best = 0, area = 0, i = 0; i < n; i++) {
			XRRCrtcInfo *ci;

			if (!(ci = XRRGetCrtcInfo(dpy, sr, sr->crtcs[i])))
				continue;
			if (!ci->width || !ci->height)
				continue;
			if ((a = area_overlap(xmin, ymin, xmax, ymax,
					      ci->x,
					      ci->y,
					      ci->x + ci->width, ci->y + ci->height)) > area) {
				area = a;
				best = i;
			}
			XFree(ci);
		}
		XFree(sr);
		if (area) {
			*monitor = best;
			return True;
		}
	}
      no_xrandr:
#endif				/* XRANDR */
#endif				/* defined XINERAMA || defined XRANDR */
	return False;
}

Bool
find_pointer_monitor(int *monitor)
{
#if defined XINERAMA || defined XRANDR
	int di, x, y;
	Window proot, dw;
	unsigned int du;

	XQueryPointer(dpy, root, &proot, &dw, &x, &y, &di, &di, &du);
#ifdef XINERAMA
	if (XineramaQueryExtension(dpy, &di, &di) && XineramaIsActive(dpy)) {
		int i, n;
		XineramaScreenInfo *si;

		if (!(si = XineramaQueryScreens(dpy, &n)) || n < 2)
			goto no_xinerama;
		for (i = 0; i < n; i++)
			if (x >= si[i].x_org && x <= si[i].x_org + si[i].width &&
			    y >= si[i].y_org && y <= si[i].y_org + si[i].height) {
				*monitor = si[i].screen_number;
				XFree(si);
				return True;
			}
		XFree(si);
	}
      no_xinerama:
#endif				/* XINERAMA */
#ifdef XRANDR
	if (XRRQueryExtension(dpy, &di, &di)) {
		XRRScreenResources *sr;
		int i, n;

		if (!(sr = XRRGetScreenResources(dpy, root)) || sr->ncrtc < 2)
			goto no_xrandr;
		n = sr->ncrtc;
		for (i = 0; i < n; i++) {
			XRRCrtcInfo *ci;

			if (!(ci = XRRGetCrtcInfo(dpy, sr, sr->crtcs[i])))
				continue;
			if (!ci->width || !ci->height)
				continue;
			if (x >= ci->x && x <= ci->x + ci->width &&
			    y >= ci->y && y <= ci->y + ci->height) {
				*monitor = i;
				XFree(ci);
				XFree(sr);
				return True;
			}
			XFree(ci);
		}
		XFree(sr);
	}
      no_xrandr:
#endif				/* XRANDR */
#endif				/* defined XINERAMA || defined XRANDR */
	return False;
}

void
set_monitor()
{
	free(fields.monitor);
	fields.monitor = calloc(64, sizeof(*fields.monitor));
	if (options.monitor != -1)
		snprintf(fields.monitor, 64, "%d", options.monitor);
	else if (options.keyboard && find_focus_monitor(&options.monitor))
		snprintf(fields.monitor, 64, "%d", options.monitor);
	else if (options.pointer && find_pointer_monitor(&options.monitor))
		snprintf(fields.monitor, 64, "%d", options.monitor);
	else if (!options.keyboard && !options.pointer &&
		 (find_focus_monitor(&options.monitor) || find_pointer_monitor(&options.monitor)))
		snprintf(fields.monitor, 64, "%d", options.monitor);
	else {
		free(fields.monitor);
		fields.monitor = NULL;
	}
}

void
set_desktop()
{
	Atom atom, real;
	int format, monitor = 0;
	unsigned long nitems, after;
	unsigned long *data = NULL;

	PTRACE();
	if (options.monitor != -1)
		monitor = options.monitor;
	free(fields.desktop);
	fields.desktop = calloc(64, sizeof(*fields.desktop));
	if (options.desktop != -1) {
		snprintf(fields.desktop, 64, "%d", options.desktop);
		return;
	}
	if (check_netwm()) {
		atom = _XA_NET_CURRENT_DESKTOP;
		if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
				       AnyPropertyType, &real, &format,
				       &nitems, &after, (unsigned char **) &data) == Success
		    && format != 0 && nitems > 0) {
			snprintf(fields.desktop, 64, "%lu",
				 nitems > monitor ? data[monitor] : data[0]);
			XFree(data);
			return;
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
		atom = _XA_NET_VISIBLE_DESKTOPS;
		if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
				       AnyPropertyType, &real, &format,
				       &nitems, &after, (unsigned char **) &data) == Success
		    && format != 0 && nitems > 0) {
			snprintf(fields.desktop, 64, "%lu",
				 nitems > monitor ? data[monitor] : data[0]);
			XFree(data);
			return;
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}
	if (check_winwm()) {
		atom = _XA_WIN_WORKSPACE;
		if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
				       AnyPropertyType, &real, &format,
				       &nitems, &after, (unsigned char **) &data) == Success
		    && format != 0 && nitems > 0) {
			snprintf(fields.desktop, 64, "%lu",
				 nitems > monitor ? data[monitor] : data[0]);
			XFree(data);
			return;
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}
	free(fields.desktop);
	fields.desktop = NULL;
	return;
}

void
set_name()
{
	char *pos;

	PTRACE();
	free(fields.name);
	if (options.name)
		fields.name = strdup(options.name);
	else if (entry.Name)
		fields.name = strdup(entry.Name);
	else if (eargv)
		fields.name = (pos = strrchr(eargv[0], '/')) ? strdup(pos) : strdup(eargv[0]);
	else
		fields.name = strdup("");	/* must be included in new: message */
}

void
set_description()
{
	PTRACE();
	free(fields.description);
	if (options.description)
		fields.description = strdup(options.description);
	else if (entry.Comment)
		fields.description = strdup(entry.Comment);
	else
		fields.description = NULL;
}

void
set_icon()
{
	char *icon, *p;

	PTRACE();
	free(fields.icon);
	icon = calloc(512, sizeof(*icon));
	if (options.icon)
		strcat(icon, options.icon);
	else if (entry.Icon)
		strcat(icon, entry.Icon);
	if ((p = strchr(icon, '/')) && p++)
		memmove(icon, p, strlen(p) + 1);
	if ((p = strstr(icon, ".xpm")) && *(p + 4) == '\0')
		*p = '\0';
	if ((p = strstr(icon, ".png")) && *(p + 4) == '\0')
		*p = '\0';
	if ((p = strstr(icon, ".svg")) && *(p + 4) == '\0')
		*p = '\0';
	if ((p = strstr(icon, ".jpg")) && *(p + 4) == '\0')
		*p = '\0';
	fields.icon = icon;
	if (!*fields.icon) {
		free(fields.icon);
		fields.icon = NULL;
	}
}

void
set_action()
{
	PTRACE();
	free(fields.action);
	if (options.action)
		fields.action = strdup(options.action);
	else
		fields.action = NULL;
}

char *
set_wmclass()
{
	char *pos;

	PTRACE();
	free(fields.wmclass);
	if (options.wmclass)
		fields.wmclass = strdup(options.wmclass);
	else if (eargv)
		fields.wmclass = (pos = strrchr(eargv[0], '/'))
		    ? strdup(pos) : strdup(eargv[0]);
	else if (entry.StartupWMClass)
		fields.wmclass = strdup(entry.StartupWMClass);
	else
		fields.wmclass = NULL;
	return fields.wmclass;
}

void
set_pid()
{
	char *p, *q;

	PTRACE();
	free(fields.pid);
	fields.pid = calloc(64, sizeof(*fields.pid));
	if (options.pid)
		snprintf(fields.pid, 64, "%d", options.pid);
	else if ((p = fields.id) &&
		 (p = strchr(p, '/')) && p++ && (p = strchr(p, '/')) && p++ && (q = strchr(p, '-'))
		 && strtoul(p, NULL, 0))
		fields.pid = strncpy(fields.pid, p, q - p);
	else
		snprintf(fields.pid, 64, "%d", (int) getpid());
}

void
set_hostname()
{
	PTRACE();
	free(fields.hostname);
	if (options.hostname)
		fields.hostname = strdup(options.hostname);
	else {
		fields.hostname = calloc(64, sizeof(*fields.hostname));
		gethostname(fields.hostname, 64);
	}
}

void
set_sequence()
{
	PTRACE();
	free(fields.sequence);
	fields.sequence = calloc(64, sizeof(*fields.sequence));
	if (options.sequence)
		strncpy(fields.sequence, options.sequence, 63);
	else if (options.monitor != -1)
		snprintf(fields.sequence, 64, "%d", options.monitor);
	else
		snprintf(fields.sequence, 64, "%d", 0);
}

void
set_timestamp()
{
	char *p;

	PTRACE();
	free(fields.timestamp);
	fields.timestamp = calloc(64, sizeof(*fields.timestamp));
	if (options.timestamp)
		snprintf(fields.timestamp, 64, "%lu", options.timestamp);
	else if (fields.id && (p = strstr(fields.id, "_TIME")) && strtoul(p + 5, NULL, 0))
		strncpy(fields.timestamp, p + 5, 63);
	else {
		XEvent ev;
		Atom atom = _XA_TIMESTAMP_PROP;
		unsigned char data = 'a';

		XSelectInput(dpy, root, PropertyChangeMask);
		XSync(dpy, False);
		XChangeProperty(dpy, root, atom, atom, 8, PropModeReplace, &data, 1);
		XMaskEvent(dpy, PropertyChangeMask, &ev);
		XSelectInput(dpy, root, NoEventMask);

		snprintf(fields.timestamp, 64, "%lu", ev.xproperty.time);
	}
}

void
do_subst(char *cmd, char *chars, char *str)
{
	int len = 0;
	char *p;

	if (options.output > 2)
		fprintf(stderr, "starting %s at %s:%d (cmd %s, char %s, str %s)\n",
			__FUNCTION__, __FILE__, __LINE__, cmd, chars, str);
	len = str ? strlen(str) : 0;
	for (p = cmd; (p = strchr(p, '%')); p++) {
		if (*(p - 1) != '%' && strspn(p + 1, chars)) {
			memmove(p + len, p + 2, strlen(p + 2) + 1);
			memcpy(p, str, len);
			p += len - 1;
		}
	}
}

void
subst_command(char *cmd)
{
	int len;
	char *p;

	do_subst(cmd, "i", fields.icon);
	do_subst(cmd, "c", fields.name);
	do_subst(cmd, "C", fields.wmclass);
	do_subst(cmd, "k", fields.application_id);
	do_subst(cmd, "uU", options.url);
	do_subst(cmd, "fF", options.file);
	do_subst(cmd, "dDnNvm", NULL);
	// do_subst(cmd, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", NULL);
	do_subst(cmd, "%", "%");
	if ((len = strspn(cmd, " \t")))
		memmove(cmd, cmd + len, strlen(cmd + len));
	if (((p = strrchr(cmd, ' ')) && strspn(p, " \t") == strlen(p)) ||
	    ((p = strrchr(cmd, '\t')) && strspn(p, " \t") == strlen(p)))
		*p = '\0';
}

Bool
truth_value(char *p)
{
	if (!p)
		return False;
	if (!strcasecmp(p, "true"))
		return True;
	if (!strcasecmp(p, "t"))
		return True;
	if (!strcasecmp(p, "yes"))
		return True;
	if (!strcasecmp(p, "y"))
		return True;
	if (atoi(p))
		return True;
	return False;
}

char *
set_command()
{
	char *cmd;

	PTRACE();
	free(fields.command);
	cmd = calloc(2048, sizeof(*cmd));
	if (truth_value(entry.Terminal)) {
		/* More to do here: if there is no wmclass set, and there is a
		   application id, use that; otherwise, if there is a binary, use the
		   binary name, and set the wmclass. */
		if (!fields.wmclass && fields.application_id) {
			free(fields.wmclass);
			fields.wmclass = strdup(fields.application_id);
		}
		if (!fields.wmclass && fields.bin) {
			char *p;

			free(fields.wmclass);
			fields.wmclass = (p = strrchr(fields.bin, '/')) ?
			    strdup(p) : strdup(fields.bin);
		}
		if (!fields.wmclass) {
			free(fields.wmclass);
			fields.wmclass = strdup("xterm");
		}
		if (fields.wmclass) {
			snprintf(cmd, 1024, "xterm -name \"%s\" -T \"%%c\" -e ", fields.wmclass);
			free(fields.silent);
			fields.silent = NULL;
		} else {
			strncat(cmd, "xterm -T \"%c\" -e ", 1024);
		}
	}
	if (options.exec)
		strncat(cmd, options.exec, 1024);
	else if (entry.Exec)
		strncat(cmd, entry.Exec, 1024);
	else if (!eargv) {
		EPRINTF("cannot launch anything without a command\n");
		exit(EXIT_FAILURE);
	}
	free(options.rawcmd);
	options.rawcmd = strdup(cmd);
	subst_command(cmd);
	return (fields.command = cmd);
}

void
set_silent()
{
	PTRACE();
	free(fields.silent);
	if (!truth_value(entry.StartupNotify) && !fields.wmclass && !set_wmclass()) {
		fields.silent = calloc(64, sizeof(*fields.silent));
		snprintf(fields.silent, 64, "%d", 1);
	} else
		fields.silent = NULL;
}

char *
first_word(char *str)
{
	char *q = strchrnul(str, ' ');

	return strndup(str, q - str);
}

char *
set_bin()
{
	char *p, *q;

	PTRACE();
	free(fields.bin);
	if (options.binary)
		fields.bin = strdup(options.binary);
	else if (eargv)
		fields.bin = strdup(eargv[0]);
	else if (fields.id && (p = strrchr(fields.id, '/')) && p++ && (q = strchr(p, '-')))
		fields.bin = strndup(p, q - p);
	else if (options.exec)
		fields.bin = first_word(options.exec);
	else if (entry.TryExec)
		fields.bin = first_word(entry.TryExec);
	else if (entry.Exec)
		fields.bin = first_word(entry.Exec);
	else if (!truth_value(entry.Terminal) && (fields.command || set_command()))
		fields.bin = first_word(fields.command);
	else
		fields.bin = NULL;
	for (; (p = strchr(fields.bin, '|')); *p = '/') ;
	return fields.bin;
}

char *
set_application_id()
{
	char *p, *q;

	PTRACE();
	free(fields.application_id);
	if (options.appid) {
		if ((p = strrchr(options.appid, '/')))
			p++;
		else
			p = options.appid;
		if ((q = strstr(options.appid, ".desktop")) && !q[8])
			fields.application_id = strndup(p, q - p);
		else
			fields.application_id = strdup(p);
	} else
		fields.application_id = NULL;
	return fields.application_id;
}

void
set_launcher()
{
	char *p;

	PTRACE();
	free(fields.launcher);
	if (options.launcher)
		fields.launcher = strdup(options.launcher);
	else if (fields.id && (p = strchr(fields.id, '/')))
		fields.launcher = strndup(fields.id, p - fields.id);
	else
		fields.launcher = strdup(NAME);
	for (; (p = strchr(fields.launcher, '|')); *p = '/') ;
}

void
set_launchee()
{
	char *p, *q;

	PTRACE();
	free(fields.launchee);
	if (options.launchee)
		fields.launchee = strdup(options.launchee);
	else if (fields.id && (p = strchr(fields.id, '/')) && p++ && (q = strchr(p, '/')))
		fields.launchee = strndup(p, q - p);
	else if (fields.bin || set_bin())
		fields.launchee = strdup(fields.bin);
	else if (fields.application_id || set_application_id())
		fields.launchee = strdup(fields.application_id);
	else
		fields.launchee = strdup("");
	for (; (p = strchr(fields.launchee, '|')); *p = '/') ;
}

void
set_id()
{
	char *launcher, *launchee, *p;

	PTRACE();
	free(fields.id);
	fields.id = NULL;

	if (options.id) {
		fields.id = strdup(options.id);
		return;
	}
	if (!fields.launcher)
		set_launcher();
	if (!fields.launchee)
		set_launchee();
	if (!fields.pid)
		set_pid();
	if (!fields.sequence)
		set_sequence();
	if (!fields.hostname)
		set_hostname();
	if (!fields.timestamp)
		set_timestamp();

	/* canonicalize paths */
	launcher = strdup(fields.launcher);
	for (; (p = strchr(launcher, '/')); *p = '|') ;
	launchee = strdup(fields.launchee);
	for (; (p = strchr(launchee, '/')); *p = '|') ;

	fields.id = calloc(512, sizeof(*fields.id));
	snprintf(fields.id, 512, "%s/%s/%s-%s-%s_TIME%s",
		 launcher, launchee, fields.pid, fields.monitor, fields.hostname, fields.timestamp);
	free(launcher);
	free(launchee);
}

void
set_all()
{
	PTRACE();
	if (!fields.name)
		set_name();
	if (!fields.icon)
		set_icon();
	if (!fields.bin)
		set_bin();
	if (!fields.description)
		set_description();
	if (!fields.wmclass)
		set_wmclass();
	if (!fields.silent)
		set_silent();
	if (!fields.application_id)
		set_application_id();
	if (!fields.screen)
		set_screen();
	/* must be on correct screen before doing monitor */
	if (!fields.monitor)
		set_monitor();
	/* must be on correct screen before doing desktop */
	if (!fields.desktop)
		set_desktop();
	if (!fields.timestamp)
		set_timestamp();
	if (!fields.hostname)
		set_hostname();
	if (!fields.action)
		set_action();
	if (!fields.command)
		set_command();
	if (!fields.pid)
		set_pid();
	if (!fields.id)
		set_id();
}

void
reset_pid(pid_t pid)
{
	PTRACE();
	if (pid) {
		/* this is the parent */
		options.pid = pid;
		set_pid();
		set_id();
		if (options.output > 1) {
			const char **lp;
			char **fp;

			OPRINTF("Final notify fields:\n");
			for (lp = StartupNotifyFields, fp = &fields.launcher; *lp; lp++, fp++)
				if (*fp)
					OPRINTF("%-24s = %s\n", *lp, *fp);
		}
		if (options.info) {
			const char **lp;
			char **fp;

			fputs("Final notify fields:\n\n", stdout);
			for (lp = StartupNotifyFields, fp = &fields.launcher; *lp; lp++, fp++)
				if (*fp)
					fprintf(stdout, "%-24s = %s\n", *lp, *fp);
			fputs("\n", stdout);
		}
	} else {
		/* this is the child */
		options.pid = getpid();
		set_pid();
		set_id();
	}
}

Client *
find_client(Window w)
{
	Client *c = NULL;

	if (XFindContext(dpy, w, ClientContext, (XPointer *) &c))
		find_window_screen(w, &screen);
	else {
		screen = c->screen;
		root = RootWindow(dpy, screen);
	}
	return (c);
}

/*
 * Check whether the window manager needs assitance: return True when it needs assistance
 * and False otherwise.
 */
Bool
need_assistance()
{
	if (check_shelp()) {
		DPRINTF("No assistance needed: Startup notification helper running\n");
		return False;
	}
	if (!check_netwm()) {
		DPRINTF("Failed NetWM check!\n");
		if (options.info)
			fputs("Assistance required: window manager failed NetWM check\n", stdout);
		return True;
	}
	if (!check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
		DPRINTF("_NET_STARTUP_ID not supported\n");
		if (options.info)
			fputs("Assistance required: window manager does not support _NET_STARTUP_ID\n", stdout);
		return True;
	}
	return False;
}

int
parse_file(char *path)
{
	FILE *file;
	char buf[4096], *p, *q;
	int outside_entry = 1;
	char *section = NULL;
	char *key, *val, *lang, *llang, *slang;
	int ok = 0, action_found = options.action ? 0 : 1;

	if (getenv("LANG") && *getenv("LANG"))
		llang = strdup(getenv("LANG"));
	else
		llang = strdup("POSIX");
	DPRINTF("language is '%s'\n", llang);

	q = strchr(llang, '@');
	slang = strdup(llang);
	if ((p = strchr(slang, '_')))
		*p = '\0';
	if ((p = strchr(llang, '.')))
		*p = '\0';
	if (q) {
		strcat(slang, q);
		strcat(llang, q);
	}

	OPRINTF("long  language string is '%s'\n", llang);
	OPRINTF("short language string is '%s'\n", slang);

	if (!(file = fopen(path, "r"))) {
		EPRINTF("cannot open file '%s' for reading\n", path);
		exit(EXIT_FAILURE);
	}
	while ((p = fgets(buf, sizeof(buf), file))) {
		/* watch out for DOS formatted files */
		if ((q = strchr(p, '\r')))
			*q = '\0';
		if ((q = strchr(p, '\n')))
			*q = '\0';
		if (*p == '[' && (q = strchr(p, ']'))) {
			*q = '\0';
			free(section);
			section = strdup(p + 1);
			outside_entry = strcmp(section, "Desktop Entry");
			if (outside_entry && options.xsession)
				outside_entry = strcmp(section, "Window Manager");
			if (outside_entry && options.action &&
			    strstr(section, "Desktop Action ") == section &&
			    strcmp(section + 15, options.action) == 0) {
				outside_entry = 0;
				action_found = 1;
			}
		}
		if (outside_entry)
			continue;
		if (*p == '#' || !(q = strchr(p, '=')))
			continue;
		*q = '\0';
		key = p;
		val = q + 1;

		/* space before and after the equals sign should be ignored */
		for (q = q - 1; q >= key && isspace(*q); *q = '\0', q--) ;
		for (q = val; *q && isspace(*q); q++) ;

		if (strstr(key, "Type") == key) {
			if (strcmp(key, "Type") == 0) {
				free(entry.Type);
				entry.Type = strdup(val);
				/* autodetect XSession */
				if (!strcmp(val, "XSession") && !options.xsession)
					options.xsession = True;
				ok = 1;
				continue;
			}
		} else if (strstr(key, "Name") == key) {
			if (strcmp(key, "Name") == 0) {
				free(entry.Name);
				entry.Name = strdup(val);
				ok = 1;
				continue;
			}
			if ((p = strchr(key, '[')) && (q = strchr(key, ']'))) {
				*q = '\0';
				lang = p + 1;
				if ((q = strchr(lang, '.')))
					*q = '\0';
				if ((q = strchr(lang, '@')))
					*q = '\0';
				if (strcmp(lang, llang) == 0 || strcmp(lang, slang) == 0) {
					free(entry.Name);
					entry.Name = strdup(val);
					ok = 1;
				}
			}
		} else if (strstr(key, "Comment") == key) {
			if (strcmp(key, "Comment") == 0) {
				free(entry.Comment);
				entry.Comment = strdup(val);
				ok = 1;
				continue;
			}
			if ((p = strchr(key, '[')) && (q = strchr(key, ']'))) {
				*q = '\0';
				lang = p + 1;
				if ((q = strchr(lang, '.')))
					*q = '\0';
				if ((q = strchr(lang, '@')))
					*q = '\0';
				if (strcmp(lang, llang) == 0 || strcmp(lang, slang) == 0) {
					free(entry.Comment);
					entry.Comment = strdup(val);
				}
			}
		} else if (strcmp(key, "Icon") == 0) {
			free(entry.Icon);
			entry.Icon = strdup(val);
			ok = 1;
		} else if (strcmp(key, "TryExec") == 0) {
			if (!entry.TryExec)
				entry.TryExec = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Exec") == 0) {
			free(entry.Exec);
			entry.Exec = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Terminal") == 0) {
			if (!entry.Terminal)
				entry.Terminal = strdup(val);
			ok = 1;
		} else if (strcmp(key, "StartupNotify") == 0) {
			if (!entry.StartupNotify)
				entry.StartupNotify = strdup(val);
			ok = 1;
		} else if (strcmp(key, "StartupWMClass") == 0) {
			if (!entry.StartupWMClass)
				entry.StartupWMClass = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Categories") == 0) {
			if (!entry.Categories)
				entry.Categories = strdup(val);
			ok = 1;
		} else if (strcmp(key, "MimeType") == 0) {
			if (!entry.MimeType)
				entry.MimeType = strdup(val);
			ok = 1;
		} else if (strcmp(key, "AsRoot") == 0) {
			if (!entry.AsRoot)
				entry.AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-KDE-RootOnly") == 0) {
			if (!entry.AsRoot)
				entry.AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-SuSE-YaST-RootOnly") == 0) {
			if (!entry.AsRoot)
				entry.AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-GNOME-Autostart-Phase") == 0) {
			if (!entry.AutostartPhase) {
				if (strcmp(val, "Applications") == 0)
					entry.AutostartPhase = strdup("Application");
				else
					entry.AutostartPhase = strdup(val);
			}
		} else if (strcmp(key, "X-KDE-autostart-phase") == 0) {
			if (!entry.AutostartPhase) {
				switch (atoi(val)) {
				case 0:
				case 1:
					entry.AutostartPhase = strdup("Initializing");
					break;
				case 2:
					entry.AutostartPhase = strdup("Application");
					break;
				}
			}
		}
	}
	fclose(file);
	return (ok && action_found);
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

	appid = calloc(1024, sizeof(*appid));
	strncat(appid, name, 1023);

	if (strstr(appid, ".desktop") != appid + strlen(appid) - 8)
		strncat(appid, ".desktop", 1024);
	if ((*appid != '/') && (*appid != '.')) {
		char *home, *copy, *dirs, *env;

		if (options.autostart) {
			/* need to look in autostart subdirectory of XDG_CONFIG_HOME and
			   then each of the subdirectories in XDG_CONFIG_DIRS */
			if ((env = getenv("XDG_CONFIG_DIRS")) && *env)
				dirs = env;
			else
				dirs = "/etc/xdg";
			home = calloc(PATH_MAX, sizeof(*home));
			if ((env = getenv("XDG_CONFIG_HOME")) && *env)
				strcpy(home, env);
			else {
				if ((env = getenv("HOME")))
					strcpy(home, env);
				else
					strcpy(home, ".");
				strcat(home, "/.config");
			}
		} else {
			/* need to look in applications subdirectory of XDG_DATA_HOME and 
			   then each of the subdirectories in XDG_DATA_DIRS */
			if ((env = getenv("XDG_DATA_DIRS")) && *env)
				dirs = env;
			else
				dirs = "/usr/local/share:/usr/share";
			home = calloc(PATH_MAX, sizeof(*home));
			if ((env = getenv("XDG_DATA_HOME")) && *env)
				strcpy(home, env);
			else {
				if ((env = getenv("HOME")))
					strcpy(home, env);
				else
					strcpy(home, ".");
				strcat(home, "/.local/share");
			}
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
			if (options.autostart)
				strcat(path, "/autostart/");
			else if (options.xsession)
				strcat(path, "/xsessions/");
			else
				strcat(path, "/applications/");
			strcat(path, appid);
			if (access(path, R_OK)) {
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
		/* only look in autostart for autostart entries */
		if (options.autostart) {
			free(home);
			free(appid);
			return (NULL);
		}
		/* only look in xsessions for xsession entries */
		else if (options.xsession) {
			free(home);
			free(appid);
			return (NULL);
		}
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
			if (access(path, R_OK)) {
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
		if ((env = getenv("XDG_CONFIG_DIRS")) && *env)
			dirs = env;
		else
			dirs = "/etc/xdg";
		if ((env = getenv("XDG_CONFIG_HOME")) && *env)
			strcpy(home, env);
		else {
			if ((env = getenv("HOME")))
				strcpy(home, env);
			else
				strcpy(home, ".");
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
			if (access(path, R_OK)) {
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
		if (access(path, R_OK)) {
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

	DPRINTF("Message to 0x%lx is: '%s'\n", root, msg);

	if (options.info) {
		fputs("Would send the following startup notification message:\n\n", stdout);
		fprintf(stdout, "%s\n\n", msg);
		return;
	}

	from = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, ParentRelative, ParentRelative);

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
		/* just PropertyChange mask in the spec doesn't work :( */
		if (!XSendEvent(dpy, root, False, StructureNotifyMask |
				SubstructureNotifyMask | SubstructureRedirectMask |
				PropertyChangeMask, &xev))
			EPRINTF("XSendEvent: failed!\n");
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}

	XSync(dpy, False);
	XDestroyWindow(dpy, from);
}

struct {
	char *label;
	char **value;
} labels[] = {
	/* *INDENT-OFF* */
	{ " NAME=",		&fields.name		},
	{ " ICON=",		&fields.icon		},
	{ " BIN=",		&fields.bin		},
	{ " DESCRIPTION=",	&fields.description	},
	{ " WMCLASS=",		&fields.wmclass		},
	{ " SILENT=",		&fields.silent		},
	{ " APPLICATION_ID=",	&fields.application_id	},
	{ " DESKTOP=",		&fields.desktop		},
	{ " SCREEN=",		&fields.screen		},
	{ " MONITOR=",		&fields.monitor		},
	{ " TIMESTAMP=",	&fields.timestamp	},
	{ " PID=",		&fields.pid		},
	{ " HOSTNAME=",		&fields.hostname	},
	{ " COMMAND=",		&fields.command		},
        { " ACTION=",           &fields.action          },
	{ NULL,			NULL			}
	/* *INDENT-ON* */
};

void
add_field(char **p, char *label, char *value)
{
	if (value) {
		strcat(*p, label);
		apply_quotes(p, value);
	}
}

void
add_fields(char *msg)
{
	int i;

	for (i = 0; labels[i].label; i++)
		add_field(&msg, labels[i].label, *labels[i].value);
}

void
send_new()
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "new:");
	add_field(&p, " ID=", fields.id);
	add_fields(p);
	send_msg(msg);
	free(msg);
}

void
send_change()
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "change:");
	add_field(&p, " ID=", fields.id);
	add_fields(p);
	send_msg(msg);
	free(msg);
}

void
send_remove()
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "remove:");
	add_field(&p, " ID=", fields.id);
	send_msg(msg);
	free(msg);
}

static void
push_time(Time *accum, Time now)
{
	if (now == CurrentTime)
		return;
	else if (*accum == CurrentTime) {
		*accum = now;
		return;
	} else if ((int) ((int) now - (int) *accum) > 0)
		*accum = now;
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
	char *file, *buf;
	ssize_t size;

	file = calloc(64, sizeof(*file));
	snprintf(file, 64, "/proc/%d/exe", pid);
	if (access(file, R_OK)) {
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

/** @brief a sophisticated dock app test
  */
Bool
is_dockapp(Client *c)
{
	if (!c->wmh)
		return False;
	if ((c->wmh->flags & StateHint) && c->wmh->initial_state == WithdrawnState)
		return True;
	if ((c->wmh->flags & ~IconPositionHint) == (StateHint | IconWindowHint | WindowGroupHint))
		return True;
	if (!c->ch.res_class)
		return False;
	if (!strcmp(c->ch.res_class, "DockApp"))
		return True;
	return False;
}

/** @brief a sophisticated tray icon test
  */
Bool
is_trayicon(Client *c)
{
	return False;
}

static Bool
test_client(Client *c)
{
	pid_t pid;
	char *str;

	/* are we managed yet? */
	if (!c->managed)
		return False;
	/* already considered? */
	if (c->counted)
		return False;
	if (c->startup_id) {
		if (!strcmp(c->startup_id, fields.id))
			return True;
		else
			return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcasecmp(fields.hostname, c->hostname)))
		pid = 0;
	if (pid && (str = get_proc_startup_id(pid))) {
		if (strcmp(fields.id, str))
			return False;
		else
			return True;
		free(str);
	}
	/* correct wmclass */
	if (fields.wmclass) {
		if (c->ch.res_name && !strcasecmp(fields.wmclass, c->ch.res_name))
			return True;
		if (c->ch.res_class && !strcasecmp(fields.wmclass, c->ch.res_class))
			return True;
	}
	/* same process id */
	if (pid && atoi(fields.pid) == pid)
		return True;
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == strtoul(fields.timestamp, NULL, 0))
		return True;
	/* correct executable */
	if (pid && fields.bin) {
		if ((str = get_proc_comm(pid)))
			if (!strcmp(fields.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			if (!strcmp(fields.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			if (!strcmp(fields.bin, str)) {
				free(str);
				return True;
			}
		free(str);
	}
	/* NOTE: we use the PID to look in: /proc/[pid]/cmdline for argv[]
	   /proc/[pid]/environ for env[] /proc/[pid]/comm basename of executable
	   /proc/[pid]/exe symbolic link to executable */
	/* correct command */
	if (c->command && (eargv || fields.command)) {
		int i;

		if (eargv) {
			if (c->count != eargc)
				return False;
			for (i = 0; i < c->count; i++)
				if (strcmp(eargv[i], c->command[i]))
					return False;
			return True;
		} else if (fields.command) {
			wordexp_t we = { 0, };

			if (wordexp(fields.command, &we, 0) == 0) {
				if (we.we_wordc != c->count) {
					wordfree(&we);
					return False;
				}
				for (i = 0; i < c->count; i++)
					if (strcmp(we.we_wordv[i], c->command[i])) {
						wordfree(&we);
						return False;
					}
				wordfree(&we);
				return True;
			}
		}
	}
	return False;
}

/** @brief assist by setting up the client
  *
  * Setting up the client consists of setting some EWMH and other properties on
  * the (group or leader) window.  Because we can get here for tool wait, we
  * must check whether assistance is desired.
  */
void
setup_client(Client *c)
{
	/* only if assitance was requested or necessary */
	if (!options.assist)
		return;
	if (!c->startup_id && fields.id) {
		XTextProperty xtp = { 0, };
		char *list[2] = { NULL, };
		int count = 1;

		list[0] = fields.id;
		Xutf8TextListToTextProperty(dpy, list, count, XUTF8StringStyle, &xtp);
		XSetTextProperty(dpy, c->group, &xtp, _XA_NET_STARTUP_ID);
		if (xtp.value)
			XFree(xtp.value);
	}
	if (!c->pid && fields.pid && atoi(fields.pid)) {
		long data = atoi(fields.pid);

		XChangeProperty(dpy, c->group, _XA_NET_WM_PID, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
	}
	if (!c->user_time && fields.timestamp && atoi(fields.timestamp)) {
		long data = atoi(fields.timestamp);

		XChangeProperty(dpy, c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
	}
	if (fields.desktop) {
		long data = atoi(fields.desktop);

		XChangeProperty(dpy, c->win, _XA_NET_WM_DESKTOP, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
	}
	if (!c->hostname && fields.hostname) {
		XTextProperty xtp = { 0, };
		char *list[2] = { NULL, };
		int count = 1;

		list[0] = fields.hostname;
		Xutf8TextListToTextProperty(dpy, list, count, XStringStyle, &xtp);
		XSetTextProperty(dpy, c->group, &xtp, XA_WM_CLIENT_MACHINE);
		if (xtp.value)
			XFree(xtp.value);
	}
	if (!c->command && (eargv || fields.command)) {
		if (eargv)
			XSetCommand(dpy, c->win, eargv, eargc);
		else if (fields.command) {
			wordexp_t we = { 0, };

			if (wordexp(fields.command, &we, 0) == 0) {
				XSetCommand(dpy, c->win, we.we_wordv, we.we_wordc);
				wordfree(&we);
			}
		}
	}
	/* use /proc/[pid]/cmdline to set up WM_COMMAND if not present */
}

static void
recheck_client(Client *c)
{
	long card = 0;
	Window win;

	/* WM_STATE */
	if (!c->state && get_cardinal(c->win, _XA_WM_STATE, XA_CARDINAL, &card)) {
		c->state = card;
		c->managed = True;
	}
	if (!c->ch.res_name && !c->ch.res_class)
		XGetClassHint(dpy, c->win, &c->ch);
	if (!c->name)
		XFetchName(dpy, c->win, &c->name);
	if (c->wmh)
		XFree(c->wmh);
	c->group = c->win;
	c->icon_win = c->win;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			if ((win = c->wmh->window_group) && win != root)
				c->group = win;
		c->icon_win = c->win;
		if (c->wmh->flags & IconWindowHint)
			if ((win = c->wmh->icon_window))
				c->icon_win = win;
		c->dockapp = is_dockapp(c);
	}
	c->time_win = c->win;
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);
	}
	c->time_win = c->time_win ? : c->win;
	get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &c->user_time);
	if (!c->hostname && !(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)))
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);
	if (!c->startup_id && !(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)))
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);
	if (!c->pid && !get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card))
		if (get_cardinal(c->group, _XA_NET_WM_PID, XA_CARDINAL, &card))
			c->pid = card;
	if (c->managed && !c->counted) {
		if (test_client(c)) {
			c->counted = True;
			if (options.assist)
				setup_client(c);
			if (options.toolwait) {
				if (--options.mappings == 0)
					running = False;
			} else if (options.assist)
				running = False;
		}
	}
}

static void
update_client(Client *c)
{
	Client *g = NULL;

	recheck_client(c);
	if (XFindContext(dpy, c->win, ClientContext, (XPointer *) &g)) {
		XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->win, ExposureMask | VisibilityChangeMask |
			     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
	}
	if (c->group && c->group != c->win) {
		if (XFindContext(dpy, c->group, ClientContext, (XPointer *) &g)) {
			XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->group, ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
	}
	if (c->icon_win && c->icon_win != c->win) {
		if (XFindContext(dpy, c->icon_win, ClientContext, (XPointer *) &g)) {
			XSaveContext(dpy, c->icon_win, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->icon_win, ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
	}
	if (c->time_win && c->time_win != c->win) {
		if (XFindContext(dpy, c->time_win, ClientContext, (XPointer *) &g)) {
			XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->time_win, ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
	}
	c->new = False;
}

static Client *
add_client(Window win)
{
	Client *c;

	c = calloc(1, sizeof(*c));
	c->win = win;
	c->next = clients;
	clients = c;
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
	if (c->name)
		XFree(c->name);
	if (c->hostname)
		XFree(c->hostname);
	if (c->startup_id)
		XFree(c->startup_id);
	XDeleteContext(dpy, c->win, ClientContext);
	if (c->time_win)
		XDeleteContext(dpy, c->time_win, ClientContext);
	if (c->command)
		XFreeStringList(c->command);
#ifdef STARTUP_NOTIFICATION
	if (c->seq)
		sn_startup_sequence_unref(c->seq);
#endif
	free(c);
}

void
del_client(Client *r)
{
	Client *c, **cp;

	for (cp = &clients, c = *cp; c && c != r; cp = &c->next, c = *cp) ;
	if (c == r)
		*cp = c->next;
	remove_client(r);
}

static Bool
handle_WM_CLIENT_MACHINE(XEvent *e, Client *c)
{
	if (!c || e->type != PropertyNotify)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		break;
	case PropertyDelete:
		break;
	}
	return True;
}

/** @brief track client command
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  */
static Bool
handle_WM_COMMAND(XEvent *e, Client *c)
{
	if (!c || e->type != PropertyNotify)
		return False;
	if (e->xproperty.window != c->win)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		if (c->command) {
			XFreeStringList(c->command);
			c->command = NULL;
			c->count = 0;
		}
		if (XGetCommand(dpy, c->win, &c->command, &c->count))
			recheck_client(c);
		return True;
	case PropertyDelete:
		if (c->command) {
			XFreeStringList(c->command);
			c->command = NULL;
			c->count = 0;
		}
		return True;
	}
	return False;
}

static Bool
handle_WM_HINTS(XEvent *e, Client *c)
{
	Window grp = None;
	Client *g = NULL;

	if (!c || e->type != PropertyNotify)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		if (c->wmh)
			XFree(c->wmh);
		if ((c->wmh = XGetWMHints(dpy, c->win))) {
			/* ensure that the group leader is also tracked */
			if (c->wmh->flags & WindowGroupHint)
				if ((grp = c->wmh->window_group) && grp != root)
					if (XFindContext(dpy, grp, ClientContext, (XPointer *) &g))
						add_client(grp);
		}
		break;
	case PropertyDelete:
		if (c->wmh) {
			XFree(c->wmh);
			c->wmh = NULL;
		}
		break;
	}
	return True;
}

/** @brief track client state
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the WM_STATE of the client.  If the client was added on
  * CreateNotify of its top-level window it can be addressed now.  If the state
  * is WithdrawnState and the client is not a dockapp, the state is
  * transitioning in the wrong direction.  Otherwise, the window has (or is
  * about to be) mapped and we can check startup notification completion.
  */
static Bool
handle_WM_STATE(XEvent *e, Client *c)
{
	long data;

	if (!c || e->type != PropertyNotify)
		return False;
	if (e->xproperty.window != c->win)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		if (get_cardinal(e->xany.window, _XA_WM_STATE, AnyPropertyType, &data)) {
			c->state = data;
			c->managed = True;
			if (c->state != WithdrawnState || c->dockapp)
				recheck_client(c);
			return True;
		}
		break;
	case PropertyDelete:
		c->state = 0;
		return True;
	}
	return False;
}

/** @brief track client class
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * Typically we expect that the client sets WM_CLASS on a window between the
  * creation of the top-level window and the map request for that window.  We
  * can recheck the client; however, when the property changes in case it is
  * incorrectly altered by the client after the window is mapped.
  */
static Bool
handle_WM_CLASS(XEvent *e, Client *c)
{
	if (!c || e->type != PropertyNotify)
		return False;
	if (e->xproperty.window != c->win)
		return False;
	switch (e->xproperty.state) {
	case PropertyNewValue:
		if (c->ch.res_name) {
			XFree(c->ch.res_name);
			c->ch.res_name = NULL;
		}
		if (c->ch.res_class) {
			XFree(c->ch.res_class);
			c->ch.res_class = NULL;
		}
		XGetClassHint(dpy, e->xproperty.window, &c->ch);
		if (c->ch.res_name || c->ch.res_class)
			recheck_client(c);
		break;
	case PropertyDelete:
		if (c->ch.res_name) {
			XFree(c->ch.res_name);
			c->ch.res_name = NULL;
		}
		if (c->ch.res_class) {
			XFree(c->ch.res_class);
			c->ch.res_class = NULL;
		}
		return True;
	}
	return False;
}

static Bool
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
		return True;
	}
	return False;
}

static Bool
handle_NET_WM_DESKTOP(XEvent *e, Client *c)
{
	return False;
}

/** @brief track the active window
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the _NET_ACTIVE_WINDOW property on the root window set
  * by EWMH/NetWM compliant window managers to indicate the active window.  We
  * keep track of the last time that each client window was active.
  */
static Bool
handle_NET_ACTIVE_WINDOW(XEvent *e, Client *c)
{
	Window active = None;

	if (get_window(root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW, &active) && active) {
		if (XFindContext(dpy, active, ClientContext, (XPointer *) &c) != Success)
			c = add_client(active);
		if (e)
			c->active_time = e->xproperty.time;
		return True;
	}
	return False;
}

static Bool
handle_NET_WM_USER_TIME(XEvent *e, Client *c)
{
	if (c && get_time(e->xany.window, _XA_NET_WM_USER_TIME, XA_CARDINAL, &c->user_time)) {
		push_time(&last_user_time, c->user_time);
		return True;
	}
	return False;
}

static Bool
handle_CLIENT_LIST(XEvent *e, Atom atom, Atom type)
{
	Window *list;
	long i, n;
	Client *c, **cp;

	if ((list = get_windows(root, atom, type, &n))) {
		for (c = clients; c; c->breadcrumb = False, c->new = False, c = c->next) ;
		for (i = 0, c = NULL; i < n; c = NULL, i++) {
			if (XFindContext(dpy, list[i], ClientContext, (XPointer *) &c) != Success)
				c = add_client(list[i]);
			c->breadcrumb = True;
		}
		XFree(list);
		for (cp = &clients, c = *cp; c;) {
			if (!c->breadcrumb) {
				*cp = c->next;
				remove_client(c);
				c = *cp;
			} else {
				cp = &c->next;
				c = *cp;
			}
		}
		return True;
	}
	return False;
}

static Bool
handle_NET_CLIENT_LIST(XEvent *e, Client *c)
{
	return handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static Bool
handle_WIN_CLIENT_LIST(XEvent *e, Client *c)
{
	return handle_CLIENT_LIST(e, _XA_WIN_CLIENT_LIST, XA_CARDINAL);
}

static Bool
handle_MANAGER(XEvent *e, Client *c)
{
	check_compm();
	check_pager();
	check_stray();
	check_icccm();
	return True;
}

static void
update_startup_client(Sequence *seq)
{
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

#ifdef STARTUP_NOTIFICATION
static Notify *notifies;

SnStartupSequence *
find_startup_seq(Client *c)
{
	Notify *n = NULL;
	SnStartupSequence *seq = NULL;
	XClassHint ch;
	char **argv;
	int argc;
	const char *binary, *wmclass;
	char *startup_id;
	Window win, grp = None;

	if ((seq = c->seq))
		return seq;
	win = c->win;
	if (!(startup_id = get_text(win, _XA_NET_STARTUP_ID))) {
		XWMHints *wmh;

		if ((wmh = XGetWMHints(dpy, c->win))) {
			if (wmh->flags & WindowGroupHint) {
				grp = wmh->window_group;
				startup_id = get_text(grp, _XA_NET_STARTUP_ID);
			}
			XFree(wmh);
		}
	}
	if (startup_id) {
		for (n = notifies; n; n = n->next)
			if (!strcmp(startup_id, sn_startup_sequence_get_id(n->seq)))
				break;
		if (!n) {
			DPRINTF("Cannot find startup id '%s'\n", startup_id);
			XFree(startup_id);
			return NULL;
		}
		XFree(startup_id);
		goto found_it;
	}
	if (XGetClassHint(dpy, win, &ch)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((wmclass = sn_startup_sequence_get_wmclass(n->seq))) {
				if (ch.res_name && !strcmp(wmclass, ch.res_name))
					break;
				if (ch.res_class && !strcmp(wmclass, ch.res_class))
					break;
			}
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
		if (n)
			goto found_it;
	}
	if (grp && XGetClassHint(dpy, grp, &ch)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((wmclass = sn_startup_sequence_get_wmclass(n->seq))) {
				if (ch.res_name && !strcmp(wmclass, ch.res_name))
					break;
				if (ch.res_class && !strcmp(wmclass, ch.res_class))
					break;
			}
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
		if (n)
			goto found_it;
	}
	if (XGetCommand(dpy, win, &argv, &argc)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((binary = sn_startup_sequence_get_binary_name(n->seq)))
				if (argv[0] && !strcmp(binary, argv[0]))
					break;
		}
		if (argv)
			XFreeStringList(argv);
		if (n)
			goto found_it;
	}
	if (grp && XGetCommand(dpy, win, &argv, &argc)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((binary = sn_startup_sequence_get_binary_name(n->seq)))
				if (argv[0] && !strcmp(binary, argv[0]))
					break;
		}
		if (argv)
			XFreeStringList(argv);
		if (n)
			goto found_it;
	}
	/* try again case insensitive */
	if (XGetClassHint(dpy, win, &ch)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((wmclass = sn_startup_sequence_get_wmclass(n->seq))) {
				if (ch.res_name && !strcasecmp(wmclass, ch.res_name))
					break;
				if (ch.res_class && !strcasecmp(wmclass, ch.res_class))
					break;
			}
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
		if (n)
			goto found_it;
	}
	if (grp && XGetClassHint(dpy, grp, &ch)) {
		for (n = notifies; n; n = n->next) {
			if (n->assigned)
				continue;
			if (sn_startup_sequence_get_completed(n->seq))
				continue;
			if ((wmclass = sn_startup_sequence_get_wmclass(n->seq))) {
				if (ch.res_name && !strcasecmp(wmclass, ch.res_name))
					break;
				if (ch.res_class && !strcasecmp(wmclass, ch.res_class))
					break;
			}
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
		if (n)
			goto found_it;
	}
	return NULL;
      found_it:
	n->assigned = True;
	seq = n->seq;
	sn_startup_sequence_ref(seq);
	sn_startup_sequence_complete(seq);
	c->seq = seq;
	return seq;
}
#endif

static Sequence *
find_seq_by_id(char *id)
{
	Sequence *seq;

	for (seq = sequences; seq; seq = seq->next)
		if (!strcmp(seq->f.id, id))
			break;
	return (seq);
}

static void
add_sequence(Sequence *seq)
{
	seq->next = sequences;
	sequences = seq;
}

static Sequence *
remove_sequence(Sequence *del)
{
	Sequence *seq, **prev;

	for (prev = &sequences, seq = *prev; seq && seq != del; prev = &seq->next, seq = *prev) ;
	if (seq) {
		*prev = seq->next;
		seq->next = NULL;
		if (!seq->client) {
			free_sequence_fields(seq);
			free(seq);
		}
	} else
		EPRINTF("could not find sequence\n");
	return (seq);
}

static void
process_startup_msg(Message *m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q;
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
		if ((v = strchr(k, '=')) == NULL) {
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
	if (cmd.f.id == NULL) {
		free_sequence_fields(&cmd);
		DPRINTF("message with no ID= field\n");
		return;
	}
	/* get timestamp from ID if necessary */
	if (cmd.f.timestamp == NULL && (p = strstr(cmd.f.id, "_TIME")) != NULL)
		cmd.f.timestamp = strdup(p + 5);
	convert_sequence_fields(&cmd);
	if ((seq = find_seq_by_id(cmd.f.id)) == NULL) {
		if (cmd.state != StartupNotifyNew) {
			free_sequence_fields(&cmd);
			DPRINTF("message out of sequence\n");
			return;
		}
		if ((seq = calloc(1, sizeof(*seq))) == NULL) {
			free_sequence_fields(&cmd);
			DPRINTF("no memory\n");
			return;
		}
		*seq = cmd;
		add_sequence(seq);
		seq->client = NULL;

		seq->changed = False;
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

static Bool
handle_NET_STARTUP_INFO_BEGIN(XEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	if (!e || e->type != ClientMessage)
		return False;
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
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
	return True;
}

static Bool
handle_NET_STARTUP_INFO(XEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	if (!e || e->type != ClientMessage)
		return False;
	from = e->xclient.window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m)
		return False;
	m->data = realloc(m->data, m->len + 21);
	/* unpack data */
	len = strnlen(e->xclient.data.b, 20);
	strncat(m->data, e->xclient.data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
	return True;
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

#ifdef STARTUP_NOTIFICATION
static void
sn_handler(SnMonitorEvent * event, void *dummy)
{
	Notify *n, **np;
	SnStartupSequence *seq;

	seq = sn_monitor_event_get_startup_sequence(event);

	switch (sn_monitor_event_get_type(event)) {
	case SN_MONITOR_EVENT_INITIATED:
		n = calloc(1, sizeof(*n));
		n->seq = sn_monitor_event_get_startup_sequence(event);
		n->assigned = False;
		n->next = notifies;
		notifies = n;
		break;
	case SN_MONITOR_EVENT_CHANGED:
		break;
	case SN_MONITOR_EVENT_COMPLETED:
	case SN_MONITOR_EVENT_CANCELED:
		seq = sn_monitor_event_get_startup_sequence(event);
		np = &notifies;
		while ((n = *np)) {
			if (n->seq == seq) {
				sn_startup_sequence_unref(n->seq);
				*np = n->next;
				free(n);
			} else {
				np = &n->next;
			}
		}
		break;
	}
	if (seq)
		sn_startup_sequence_unref(seq);
	sn_monitor_event_unref(event);
}
#endif

Bool
handle_atom(XEvent *e, Client *c, Atom atom)
{
	struct atoms *a;

	for (a = atoms; a->atom; a++) {
		if (a->value == atom) {
			if (a->handler)
				return a->handler(e, c);
			break;
		}
	}
	return False;
}

/** @brief handle CreateNotify event
  *
  * When a top-level, non-override-redirect window is created, add it to the
  * potential client list as unmanaged.
  */
Bool
handle_CreateNotify(XEvent *e, Client *c)
{
	if (!e || e->type != CreateNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	/* no synthetics please */
	if (e->xcreatewindow.send_event)
		return False;
	/* must be created on correct root */
	if (e->xcreatewindow.parent != root)
		return False;
	/* no override redirect windows */
	if (e->xcreatewindow.override_redirect)
		return False;
	c = c ? : add_client(e->xcreatewindow.window);
	return True;
}

/** @brief handle DestroyNotify event
  *
  * When the window destroyed is one of our check windows, recheck the
  * compliance of the window manager to the appropriate check.
  */
Bool
handle_DestroyNotify(XEvent *e, Client *c)
{
	Bool handled = False;

	if (!e || e->type != DestroyNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (c) {
		del_client(c);
		return True;
	}
	DPRINTF("DestroyNotify not for client!\n");
	clean_msgs(e->xdestroywindow.window);
	if (wm.netwm_check && e->xdestroywindow.window == wm.netwm_check) {
		check_netwm();
		handled = True;
	}
	if (wm.winwm_check && e->xdestroywindow.window == wm.winwm_check) {
		check_winwm();
		handled = True;
	}
	if (wm.maker_check && e->xdestroywindow.window == wm.maker_check) {
		check_maker();
		handled = True;
	}
	if (wm.motif_check && e->xdestroywindow.window == wm.motif_check) {
		check_motif();
		handled = True;
	}
	if (wm.icccm_check && e->xdestroywindow.window == wm.icccm_check) {
		check_icccm();
		handled = True;
	}
	if (wm.redir_check && e->xdestroywindow.window == wm.redir_check) {
		check_redir();
		handled = True;
	}
	return handled;
}

/** @brief handle FocusIn event
  *
  * If a client receives a FocusIn event (receives the focus); then it was
  * mapped and is managed.  We can updated the client and mark it as managed.
  */
Bool
handle_FocusIn(XEvent *e, Client *c)
{
	if (!e || e->type != FocusIn) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (!c) {
		DPRINTF("FocusIn not for us!\n");
		return False;
	}
	c->managed = True;
	recheck_client(c);
	return True;
}

/** @brief handle VisibilityNotify event
  *
  * If a client becomes something other than fully obscured, then it is mapped
  * and visible, and was managed.  We can update the client and mark it as
  * managed.
  */
Bool
handle_VisibilityNotify(XEvent *e, Client *c)
{
	if (!e || e->type != VisibilityNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (!c) {
		DPRINTF("VisibilityNotify not for us!\n");
		return False;
	}
	c->managed = True;
	recheck_client(c);
	return True;
}

/** @brief handle UnmapNotify event
  *
  * If the client is unmapped (can be synthetic), we can perform deletion of
  * managed client window information.
  */
Bool
handle_UnmapNotify(XEvent *e, Client *c)
{
	if (!e || e->type != UnmapNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (!c) {
		DPRINTF("UnmapNotify not for us!\n");
		return False;
	}
	return True;
}

/** @brief handle MapNotify event
  *
  * If a client becomes mapped, then it is mapped and visible, and was managed.
  * We can update the client and mark it as managed.
  */
Bool
handle_MapNotify(XEvent *e, Client *c)
{
	if (!e || e->type != MapNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (!c) {
		DPRINTF("MapNotify not for us!\n");
		return False;
	}
	c->managed = True;
	recheck_client(c);
	return True;
}


/** @brief handle monitoring events.
  * @param e - X event to handle
  *
  * If the window manager has a client list, we can check for newly mapped
  * windows by additions to the client list.  We can calculate the user time by
  * tracking _NET_WM_USER_TIME and _NET_WM_USER_TIME_WINDOW on all clients.  If
  * the window manager supports _NET_WM_STATE_FOCUSED or at least
  * _NET_ACTIVE_WINDOW, coupled with FocusIn and FocusOut events, we should be
  * able to track the last focused window at the time that the app started (not
  * all apps support _NET_WM_USER_TIME).
  */
void
handle_event(XEvent *e)
{
	Client *c;

	switch (e->type) {
	case PropertyNotify:
		c = find_client(e->xproperty.window);
		handle_atom(e, c, e->xproperty.atom);
		break;
	case FocusIn:
		c = find_client(e->xfocus.window);
		handle_FocusIn(e, c);
		break;
	case FocusOut:
		break;
	case Expose:
		break;
	case VisibilityNotify:
		c = find_client(e->xvisibility.window);
		handle_VisibilityNotify(e, c);
		break;
	case CreateNotify:
		c = find_client(e->xcreatewindow.window);
		handle_CreateNotify(e, c);
		break;
	case DestroyNotify:
		c = find_client(e->xdestroywindow.window);
		handle_DestroyNotify(e, c);
		break;
	case UnmapNotify:
		c = find_client(e->xunmap.window);
		handle_UnmapNotify(e, c);
		break;
	case MapNotify:
		c = find_client(e->xmap.window);
		handle_MapNotify(e, c);
		break;
	case ReparentNotify:
		break;
	case ConfigureNotify:
		break;
	case ClientMessage:
		c = find_client(e->xclient.window);
		handle_atom(e, c, e->xclient.message_type);
		break;
	case MappingNotify:
		break;
	default:
		break;
	}
}

/** @brief set up for an assist
  *
  * We want to perform as much as possible in the master process before the
  * acutal launch so that this information is immediately available to the child
  * process before the launch.
  */
void
setup_to_assist()
{
	if (!handle_NET_CLIENT_LIST(NULL, NULL))
		if (!handle_WIN_CLIENT_LIST(NULL, NULL))
			;
	handle_NET_ACTIVE_WINDOW(NULL, NULL);
	if (fields.timestamp)
		push_time(&launch_time, (Time) strtoul(fields.timestamp, NULL, 0));
}

volatile int signum = 0;

void
sighandler(int sig)
{
	signum = sig;
}

/** @brief assist the window manager.
  *
  * Assist the window manager to do the right thing with respect to focus and
  * with respect to positioning of the window on the correct monitor and the
  * correct desktop.
  *
  * Launch without toolwait, with assistance: a child is forked that will
  * perform the assistance.  The parent will execute the command.  The parent
  * should initialize client lists before starting the child.  The parent sends
  * startup notification before executing the command.  The child performs
  * assistance and then exits.  The child owns the display connection.
  */
void
assist()
{
	pid_t pid = getpid();

	setup_to_assist();
	XSync(dpy, False);
	reset_pid(pid);
	if (options.info) {
		fputs("Would launch with wm assistance\n\n", stdout);
		return;
	}
	if ((pid = fork()) < 0) {
		EPRINTF("%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pid) {
		/* parent returns and launches */
		return;
	}
	/* for now the child just exits */
	exit(EXIT_SUCCESS);

	/* continue on monitoring */
	{
		int xfd;
		XEvent ev;

		signum = 0;
		signal(SIGHUP, sighandler);
		signal(SIGINT, sighandler);
		signal(SIGTERM, sighandler);
		signal(SIGQUIT, sighandler);

#ifdef STARTUP_NOTIFICATION
		sn_ctx = sn_monitor_context_new(sn_dpy, screen, &sn_handler, NULL, NULL);
#endif

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
}

/** @brief launch with tool wait
  *
  * Launch with toolwait, with or without assist: a child is forked that will
  * execute the command.  The parent must determine existing clients before
  * forking the child.  The child must send the startup notification before
  * executing the command.  The parent will perform any assistance that is
  * required and exit when the startup conditions have been satisfied.  The
  * parent owns the display connection. 
  *
  * If we do not need assistance, all that is needed here is to wait for the
  * startup notification "end:" message (or timeout) and then consider the
  * startup complete.
  *
  * If assistance is required, we must identify when the application maps its
  * windows and send the "end:" message ourselves.  Upon transmission of the
  * "end:" message (or timeout), consider the startup complete.  In addition,
  * EWMH properties are set on initial windows that are not set by the
  * application.
  */
void
toolwait()
{
	pid_t pid = getpid();

	setup_to_assist();
	XSync(dpy, False);
	if (options.info) {
		fputs("Would launch with tool wait support\n\n", stdout);
		reset_pid(pid);
		return;
	}
	if ((pid = fork()) < 0) {
		EPRINTF("%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	reset_pid(pid);
	if (!pid) {
		/* child returns and launches */
		return;
	}
	/* for now the parent just exits */
	exit(EXIT_SUCCESS);
}

/** @brief launch normally without assist or tool wait
  *
  * Normal launch without assist without toolwait: No child is generated, the
  * parent process sends the startup notification message and then executes the
  * command.  The main process owns the display connection.
  *
  * No startup notification completion is tracked nor generated: it is assumed
  * that the window manager or application will complete startup notification
  * and set the appropriate EWMH properties on all windows.
  */
void
normal()
{
	pid_t pid = getpid();

	if (options.info) {
		fputs("Would launch without assistance or tool wait\n\n", stdout);
		reset_pid(pid);
		return;
	}
	reset_pid(pid);
	/* main process returns and launches */
	return;
}

static Bool
wait_for_condition(Window (*until) (void))
{
	int xfd;
	XEvent ev;

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGALRM, sighandler);

#ifdef STARTUP_NOTIFICATION
	sn_ctx = sn_monitor_context_new(sn_dpy, screen, &sn_handler, NULL, NULL);
#endif
	/* main event loop */
	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	while (running) {
		struct pollfd pfd = { xfd, POLLIN | POLLHUP | POLLERR, 0 };

		if (signum) {
			if (signum == SIGALRM)
				return True;
			exit(EXIT_SUCCESS);
		}
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
				if (until())
					return True;
			}
		}
	}
	return False;
}

static Bool
check_for_window_manager()
{
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm())
		OPRINTF("NetWM/EWMH window 0x%lx\n", wm.netwm_check);
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm())
		OPRINTF("GNOME/WMH window 0x%lx\n", wm.winwm_check);
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker())
		OPRINTF("WindowMaker window 0x%lx\n", wm.maker_check);
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif())
		OPRINTF("OSF/Motif window 0x%lx\n", wm.motif_check);
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm())
		OPRINTF("ICCCM 2.0 window 0x%lx\n", wm.icccm_check);
	OPRINTF("checking redirection\n");
	if (check_redir())
		OPRINTF("redirection on window 0x%lx\n", wm.redir_check);
	return check_anywm();
}

void
wait_for_window_manager()
{
	if (check_for_window_manager()) {
		if (options.info) {
			fputs("Have a window manager:\n\n", stdout);
			if (wm.netwm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window NetWM/EWMH", wm.netwm_check);
			if (wm.winwm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window WinWM/GNOME", wm.winwm_check);
			if (wm.maker_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window WindowMaker", wm.maker_check);
			if (wm.motif_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window OSF/MOTIF", wm.motif_check);
			if (wm.icccm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window ICCCM 2.0", wm.icccm_check);
			if (wm.redir_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window redirection", wm.redir_check);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for window manager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_anywm);
}

void
wait_for_system_tray()
{
	if (check_stray()) {
		if (options.info) {
			fputs("Have a system tray:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "System Tray window", wm.stray_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for system tray\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_stray);
}

void
wait_for_desktop_pager()
{
	if (check_pager()) {
		if (options.info) {
			fputs("Have a desktop pager:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "Desktop pager window", wm.pager_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for desktop pager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_pager);
}

void
wait_for_composite_manager()
{
	if (check_compm()) {
		if (options.info) {
			fputs("Have a composite manager:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "Composite manager window", wm.compm_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for composite manager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_compm);
}

/* NOTES:
 *
 * We really need a guard timer option setting.  We should typically only wait
 * for 2 seconds for a window manager.  Setting the guard timer to zero should
 * result in indefinite wait.
 *
 * When options.xsession is true, we should never wait for resources.
 */
void
wait_for_resource()
{
	if (options.xsession) {
		DPRINTF("Cannot wait for resources when launching xsessions.\n");
		return;
	}
	if (entry.AutostartPhase) {
		/* When autostarting we should use the autostart phase to
		 * determine the resources for which to wait. */
		if (strcmp(entry.AutostartPhase, "Initializing") == 0) {
			/* Initializing: do not wait for anything. */
			options.manager = False;
			options.tray = False;
			options.pager = False;
			options.composite = False;
		} else if (strcmp(entry.AutostartPhase, "WindowManager") == 0) {
			/* WindowManager: do not wait for anything. */
			options.manager = False;
			options.tray = False;
			options.pager = False;
			options.composite = False;
		} else if (strcmp(entry.AutostartPhase, "Panel") == 0) {
			/* Panel: wait only for window manager (if requested?). */
			options.manager = True;
			options.tray = False;
			options.pager = False;
		} else if (strcmp(entry.AutostartPhase, "Desktop") == 0) {
			/* Desktop: wait only for window manager (if requested?). */
			options.manager = True;
		} else if (strcmp(entry.AutostartPhase, "Application") == 0) {
			/* Application(s): wait for window manager, others if requested. */
			options.manager = True;
		} else {
			/* default is same as Application */
			options.manager = True;
		}
	} else if (options.autostart) {
		options.manager = True;
		if (entry.Categories) {
			if (strstr(entry.Categories, "TrayIcon"))
				options.tray = True;
			if (strstr(entry.Categories, "DockApp"))
				options.manager = True;
			if (strstr(entry.Categories, "SystemTray"))
				options.tray = False;
		}
	}
	if (options.xsession) {
		/* Xsessions cannot wait (responsible for all). */
		options.manager = False;
		options.tray = False;
		options.pager = False;
		options.composite = False;
	}

	/* Be sure to wait for window manager before checking whether assistance is
	   needed. */
	if (options.manager || options.tray || options.pager || options.composite) {
		alarm(120);	/* guard time for wait */
		if (options.manager)
			wait_for_window_manager();
		if (options.tray)
			wait_for_system_tray();
		if (options.pager)
			wait_for_desktop_pager();
		if (options.composite)
			wait_for_composite_manager();
	} else
		DPRINTF("No resource wait requested.\n");
}

Bool
need_assist()
{
	Bool need_assist = options.assist;

	if (need_assist) {
		DPRINTF("Assistance requested\n");
		if (options.info)
			fputs("Assistance requested\n", stdout);
		return need_assist;
	}
	if (options.xsession) {
		DPRINTF("XSession: always needs assistance\n");
		need_assist = True;
		if (options.info)
			fputs("Launching XSession entry always requires assistance.\n", stdout);
	} else if (options.autostart) {
		DPRINTF("AutoStart: always needs assistance\n");
		need_assist = True;
		if (options.info)
			fputs("Launching AutoStart entry always requires assistance.\n", stdout);
	} else if (need_assistance()) {
		OPRINTF("WindowManager: needs assistance\n");
		if (fields.wmclass) {
			OPRINTF("WMCLASS: requires assistance\n");
			need_assist = True;
			if (options.info)
				fputs("Launching StartupWMClass entry always requires assistance.\n", stdout);
		}
		if (fields.silent && atoi(fields.silent)) {
			OPRINTF("SILENT: requires assistance\n");
			need_assist = True;
			if (options.info)
				fputs("Launching SILENT entry always requires assistance.\n", stdout);
		}
	}
	return need_assist;
}

void
launch()
{
	size_t size;
	char *disp, *cmd, *p;
	Bool change_only = False;

	wait_for_resource();
	options.assist = need_assist();

	/* make the call... */

	if (options.toolwait) {
		OPRINTF("Tool wait requested\n");
		if (options.info)
			fputs("Tool wait requested\n\n", stdout);
		toolwait();
	} else if (options.assist) {
		OPRINTF("Assistance is required\n");
		if (options.info)
			fputs("Assistance is required\n\n", stdout);
		assist();
	} else {
		OPRINTF("Assistance is NOT needed, no tool wait\n");
		if (options.info)
			fputs("Assistance is NOT needed, no tool wait\n\n", stdout);
		normal();
	}

	if (options.id)
		change_only = True;

	if (change_only)
		send_change();
	else
		send_new();
	XCloseDisplay(dpy);

	/* set the DESKTOP_STARTUP_ID environment variable */
	setenv("DESKTOP_STARTUP_ID", fields.id, 1);

	/* set the DISPLAY environment variable */
	p = getenv("DISPLAY");
	size = strlen(p) + strlen(fields.screen) + 2;
	disp = calloc(size, sizeof(*disp));
	strcpy(disp, p);
	if ((p = strrchr(disp, '.')) && strspn(p + 1, "0123456789") == strlen(p + 1))
		*p = '\0';
	strcat(disp, ".");
	strcat(disp, fields.screen);
	setenv("DISPLAY", disp, 1);

	if (eargv) {
		if (options.info) {
			char **p;

			fputs("Command would be:\n\n", stdout);
			for (p = &eargv[0]; p && *p; p++)
				fprintf(stdout, "'%s' ", *p);
			fputs("\n\n", stdout);
			return;
		}
		execvp(eargv[0], eargv);
	} else {
		cmd = calloc(strlen(fields.command) + 32, sizeof(cmd));
		strcat(cmd, "exec ");
		strcat(cmd, fields.command);
		if (options.info) {
			fputs("Command would be:\n\n", stdout);
			fprintf(stdout, "%s\n\n", cmd);
			free(cmd);
			return;
		}
		OPRINTF("Command is: '%s'\n", cmd);
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
	}
	EPRINTF("Should never get here!\n");
	exit(127);
}

#ifdef RECENTLY_USED

time_t now;
int age = 30;

char *
get_mime_type(const char *uri)
{
	GFile *file;
	GFileInfo *info;
	char *mime = NULL;

	if (!(file = g_file_new_for_uri(uri))) {
		EPRINTF("could not get GFile for %s\n", uri);
	} else
	    if (!(info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
					   G_PRIORITY_DEFAULT, NULL, NULL))) {
		EPRINTF("could not get GFileInfo for %s\n", uri);
		g_object_unref(file);
	} else
	    if (!(mime = g_file_info_get_attribute_as_string(info,
							     G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)))
	{
		EPRINTF("could not get content-type for %s\n", uri);
		g_object_unref(info);
		g_object_unref(file);
	}
	return (mime);
}

#ifdef RECENTLY_USED_XBEL

static void
put_recent_applications_xbel(char *filename)
{
	GtkRecentManager *mgr;
	GtkRecentData data = { NULL, };
	gchar *groups[2] = { "Application", NULL };
	char *file;

	file = g_build_filename(g_get_user_data_dir(), filename, NULL);
	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		return;
	}
	if (!options.uri) {
		EPRINTF("cannot record %s without a uri\n", filename);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		return;
	}

	/* 1) read in the recently-used.xbel file (uri only) */
	if (!(mgr = g_object_new(GTK_TYPE_RECENT_MANAGER, "filename", file, NULL))) {
		EPRINTF("could not get recent manager instance\n");
		g_free(file);
		return;
	}
	g_free(file);

	/* 2) append new information (uri only) */
	data.display_name = fields.name;
	data.description = fields.description;
	data.mime_type = "application/x-desktop";
	data.app_name = "XDG Launcher";
	data.app_exec = "xdg-launch %f";
	data.groups = groups;
	data.is_private = TRUE;
	if (!gtk_recent_manager_add_full(mgr, options.uri, &data)) {
		EPRINTF("could not add recent data info\n");
		return;
	}

	/* 3) write out the recently-used.xbel file (uri only) */
	g_object_unref(mgr);
	do {
		gtk_main_iteration_do(FALSE);
	}
	while (gtk_events_pending());
}

static void
put_recently_used_xbel(char *filename)
{
	GtkRecentManager *mgr;
	GtkRecentData data = { NULL, };
	gchar *groups[2] = { NULL, NULL };
	char *file, *p, *mime = NULL, *group = NULL, *exec = NULL;
	int len;

	file = g_build_filename(g_get_user_data_dir(), filename, NULL);
	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		return;
	}
	if (!options.url) {
		EPRINTF("cannot record %s without a url\n", filename);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		return;
	}

	/* 1) read in the recently-used.xbel file (url only) */
	if (!(mgr = g_object_new(GTK_TYPE_RECENT_MANAGER, "filename", file, NULL))) {
		EPRINTF("could not get recent manager instance\n");
		g_free(file);
		return;
	}
	g_free(file);

	/* 2) append new information (url only) */
	data.display_name = NULL;
	data.description = NULL;
	if (!(mime = get_mime_type(options.url))) {
		if (entry.MimeType) {
			mime = strdup(entry.MimeType);
			if ((p = strchr(mime, ';')))
				*p = '\0';
		}
	}
	data.mime_type = mime;
	if (fields.application_id) {
		len = strlen(NAME) + 1 + strlen(fields.application_id) + 1 + strlen("%u") + 1;
		exec = calloc(len, sizeof(*exec));
		snprintf(exec, len, "%s %s %%u", NAME, fields.application_id);

		data.app_name = NAME;
		data.app_exec = exec;
	} else {
		data.app_name = fields.bin ? : entry.Name;
		data.app_exec = options.rawcmd;
	}

	/* FIXME: why not do multiple groups? */
	if (entry.Categories) {
		group = strdup(entry.Categories);
		if ((p = strchr(group, ';')))
			*p = '\0';
		/* reserved for application launching */
		if (!strcmp(group, "Application")) {
			free(group);
			group = NULL;
		}
	}
	if (group) {
		groups[0] = group;
		data.groups = groups;
	} else
		data.groups = NULL;
	data.is_private = TRUE;
	if (!gtk_recent_manager_add_full(mgr, options.url, &data)) {
		EPRINTF("could not add recent data info\n");
		free(mime);
		free(exec);
		free(group);
		return;
	}
	free(mime);
	free(exec);
	free(group);

	/* 3) write out the recently-used.xbel file (url only) */
	g_object_unref(mgr);
	do {
		gtk_main_iteration_do(FALSE);
	}
	while (gtk_events_pending());
}

#else				/* RECENTLY_USED_XBEL */

/*
 * XXX: Without gtk2 present (or desired for dependencies), I wrote this
 * implementation for parsing the desktop bookmark specification XBEL file
 * parser using the glib XML parser.  There is also a gio parser specifically
 * for the desktop-bookmark XBEL file format, but I didn't know about it when I
 * wrote this... :(
 */

typedef struct {
	gchar *name;
	gchar *exec;
	time_t modified;
	guint count;
} XbelApplication;

typedef struct {
	gchar *href;
	time_t added;
	time_t modified;
	time_t visited;
	gchar *title;
	gchar *desc;
	gchar *owner;
	gchar *mime;
	GList *groups;
	GList *applications;
	gboolean private;
} XbelBookmark;

static void
xbel_start_element(GMarkupParseContext *ctx, const gchar *name, const gchar
		   **attrs, const gchar **values, gpointer user, GError **err)
{
	GList **list = user;
	GList *item = g_list_last(*list);
	XbelBookmark *mark = item ? item->data : NULL;

	if (!strcmp(name, "xbel")) {
	} else if (!strcmp(name, "bookmark")) {
		struct tm tm = { 0, };

		if (!(mark = calloc(1, sizeof(*mark)))) {
			EPRINTF("could not allocate bookmark\n");
			exit(1);
		}
		for (; *attrs; attrs++, values++) {
			if (!strcmp(*attrs, "href")) {
				free(mark->href);
				mark->href = strdup(*values);
			} else if (!strcmp(*attrs, "added")) {
				if (strptime(*values, "%FT%H:%M:%S%Z", &tm))
					mark->added = timegm(&tm);
			} else if (!strcmp(*attrs, "modified")) {
				if (strptime(*values, "%FT%H:%M:%S%Z", &tm))
					mark->modified = timegm(&tm);
			} else if (!strcmp(*attrs, "visited")) {
				if (strptime(*values, "%FT%H:%M:%S%Z", &tm))
					mark->visited = timegm(&tm);
			} else {
				DPRINTF("unrecognized attribute of bookmark: '%s'\n", *attrs);
			}
		}
		*list = g_list_append(*list, mark);
	} else if (!strcmp(name, "title")) {
	} else if (!strcmp(name, "desc")) {
	} else if (!strcmp(name, "info")) {
	} else if (!strcmp(name, "metadata")) {
		if (mark) {
			for (; *attrs; attrs++, values++) {
				if (!strcmp(*attrs, "owner")) {
					free(mark->owner);
					mark->owner = strdup(*values);
				}
			}
		}
	} else if (!strcmp(name, "mime:mime-type")) {
		if (mark) {
			for (; *attrs; attrs++, values++) {
				if (!strcmp(*attrs, "type")) {
					free(mark->mime);
					mark->mime = strdup(*values);
				} else {
					DPRINTF("unrecognized attribute of mime:mime-type: '%s'\n",
						*attrs);
				}
			}
		}
	} else if (!strcmp(name, "bookmark:groups")) {
	} else if (!strcmp(name, "bookmark:group")) {
	} else if (!strcmp(name, "bookmark:applications")) {
	} else if (!strcmp(name, "bookmark:application")) {
		if (mark) {
			XbelApplication *appl;
			struct tm tm = { 0, };

			if (!(appl = calloc(1, sizeof(*appl)))) {
				EPRINTF("could not allocate application\n");
				exit(1);
			}
			for (; *attrs; attrs++, values++) {
				if (!strcmp(*attrs, "name")) {
					free(appl->name);
					appl->name = strdup(*values);
				} else if (!strcmp(*attrs, "exec")) {
					free(appl->exec);
					appl->exec = strdup(*values);
				} else if (!strcmp(*attrs, "modified")) {
					if (strptime(*values, "%FT%H:%M:%S%Z", &tm))
						appl->modified = timegm(&tm);
				} else if (!strcmp(*attrs, "count")) {
					appl->count = strtoul(*values, NULL, 0);
				} else {
					DPRINTF
					    ("unrecognized attribute of bookmark:application: '%s'\n",
					     *attrs);
				}
			}
			mark->applications = g_list_append(mark->applications, appl);
		}
	} else if (!strcmp(name, "bookmark:private")) {
		if (mark)
			mark->private = TRUE;
	} else {
		DPRINTF("unrecognized start tag: '%s'\n", name);
	}
}

static void
xbel_text(GMarkupParseContext *ctx, const gchar *text, gsize len, gpointer user, GError **err)
{
	GList **list = user;
	GList *item = g_list_last(*list);
	XbelBookmark *mark = item ? item->data : NULL;
	const gchar *name;

	name = g_markup_parse_context_get_element(ctx);

	if (!strcmp(name, "xbel")) {
	} else if (!strcmp(name, "bookmark")) {
	} else if (!strcmp(name, "title")) {
		if (mark) {
			free(mark->title);
			mark->title = strndup(text, len);
		}
	} else if (!strcmp(name, "desc")) {
		if (mark) {
			free(mark->desc);
			mark->desc = strndup(text, len);
		}
	} else if (!strcmp(name, "info")) {
	} else if (!strcmp(name, "metadata")) {
	} else if (!strcmp(name, "mime:mime-type")) {
	} else if (!strcmp(name, "bookmark:groups")) {
	} else if (!strcmp(name, "bookmark:group")) {
		if (mark)
			mark->groups = g_list_append(mark->groups, strndup(text, len));
	} else if (!strcmp(name, "bookmark:applications")) {
	} else if (!strcmp(name, "bookmark:application")) {
	} else if (!strcmp(name, "bookmark:private")) {
	} else {
		DPRINTF("unrecognized cdata tag: '%s'\n", name);
	}
}

static void
text_free(gpointer data)
{
	free(data);
}

static void
appl_free(gpointer data)
{
	XbelApplication *appl = data;

	free(appl->name);
	appl->name = NULL;
	free(appl->exec);
	appl->exec = NULL;
	appl->modified = 0;
	appl->count = 0;
	free(appl);
}

static void
xbel_free(gpointer data)
{
	XbelBookmark *book = data;

	free(book->href);
	book->href = NULL;
	book->added = 0;
	book->modified = 0;
	book->visited = 0;
	free(book->title);
	book->title = NULL;
	free(book->desc);
	book->desc = NULL;
	free(book->owner);
	book->owner = NULL;
	free(book->mime);
	book->mime = NULL;
	g_list_free_full(book->groups, text_free);
	g_list_free_full(book->applications, appl_free);
	free(book);
}

static void
xbel_grp_write(gpointer data, gpointer user)
{
	gchar *grp = data;
	FILE *f = user;
	char *s;

	s = g_markup_printf_escaped("          <bookmark:group>%s</bookmark:group>\n", grp);
	fputs(s, f);
	g_free(s);
}

static void
xbel_app_write(gpointer data, gpointer user)
{
	XbelApplication *a = data;
	FILE *f = user;
	char *s;

	fputs("          <bookmark:application", f);
	if (a->name) {
		s = g_markup_printf_escaped(" name=\"%s\"", a->name);
		fputs(s, f);
		g_free(s);
	}
	if (a->exec) {
		s = g_markup_printf_escaped(" exec=\"%s\"", a->exec);
		fputs(s, f);
		g_free(s);
	}
	if (a->modified) {
		struct tm tm = { 0, };
		char t[32] = { 0, };

		gmtime_r(&a->modified, &tm);
		strftime(t, 32, "%FT%H:%M:%SZ", &tm);
		s = g_markup_printf_escaped(" modified=\"%s\"", t);
		fputs(s, f);
		g_free(s);
	}
	if (a->count) {
		s = g_markup_printf_escaped(" count=\"%d\"", a->count);
		fputs(s, f);
		g_free(s);
	}
	fputs("/>\n", f);
}

static void
xbel_write(gpointer data, gpointer user)
{
	XbelBookmark *b = data;
	FILE *f = user;
	char *s;

	/* XXX: we should be trimming bookmarks that are beyond a maximum age factor. We
	   could get the value from GSettings, but we don't have it available if we are
	   here.  We could use some other value or look for an XSettings daemon on the
	   primary screen.

	   The age is in gtk-recent-files-max-age (GtkRecentFilesMaxAge) and has a
	   default value of 30 (days, I suppose).  */

	if (((int) ((now - b->modified) / (60 * 60 * 24))) > age)
		return;

	fputs("  <bookmark", f);
	if (b->href) {
		s = g_markup_printf_escaped(" href=\"%s\"", b->href);
		fputs(s, f);
		g_free(s);
	}
	if (b->added) {
		struct tm tm = { 0, };
		char t[32] = { 0, };

		gmtime_r(&b->added, &tm);
		strftime(t, 32, "%FT%H:%M:%SZ", &tm);
		s = g_markup_printf_escaped(" added=\"%s\"", t);
		fputs(s, f);
		g_free(s);
	}
	if (b->modified) {
		struct tm tm = { 0, };
		char t[32] = { 0, };

		gmtime_r(&b->modified, &tm);
		strftime(t, 32, "%FT%H:%M:%SZ", &tm);
		s = g_markup_printf_escaped(" modified=\"%s\"", t);
		fputs(s, f);
		g_free(s);
	}
	if (b->visited) {
		struct tm tm = { 0, };
		char t[32] = { 0, };

		gmtime_r(&b->visited, &tm);
		strftime(t, 32, "%FT%H:%M:%SZ", &tm);
		s = g_markup_printf_escaped(" visited=\"%s\"", t);
		fputs(s, f);
		g_free(s);
	}
	fputs(">\n", f);
	if (b->title) {
		s = g_markup_printf_escaped("    <title>%s</title>\n", b->title);
		fputs(s, f);
		g_free(s);
	}
	if (b->desc) {
		s = g_markup_printf_escaped("    <desc>%s</desc>\n", b->desc);
		fputs(s, f);
		g_free(s);
	}
	fputs("    <info>\n", f);
	fputs("      <metadata", f);
	if (b->owner) {
		s = g_markup_printf_escaped(" owner=\"%s\"", b->owner);
		fputs(s, f);
		g_free(s);
	}
	fputs(">\n", f);
	fputs("        <mime:mime-type", f);
	if (b->mime) {
		s = g_markup_printf_escaped(" type=\"%s\"", b->mime);
		fputs(s, f);
		g_free(s);
	}
	fputs("/>\n", f);
	if (b->groups) {
		fputs("        <bookmark:groups>\n", f);
		g_list_foreach(b->groups, xbel_grp_write, f);
		fputs("        </bookmark:groups>\n", f);
	}
	if (b->applications) {
		fputs("        <bookmark:applications>\n", f);
		g_list_foreach(b->applications, xbel_app_write, f);
		fputs("        </bookmark:applications>\n", f);
	}
	if (b->private)
		fputs("        <bookmark:private/>\n", f);
	fputs("      </metadata>\n", f);
	fputs("    </info>\n", f);
	fputs("  </bookmark>\n", f);
}

static gint
href_match(gconstpointer data, gconstpointer user)
{
	const XbelBookmark *b = data;

	return (strcmp(b->href, user));
}

static gint
grp_match(gconstpointer data, gconstpointer user)
{
	return (strcmp(data, user));
}

static gint
app_match(gconstpointer data, gconstpointer user)
{
	const XbelApplication *a = data;

	return (strcmp(a->name, user));
}

static void
put_recent_applications_xbel(char *filename)
{
	FILE *f;
	int dummy;
	GList *item;
	struct stat st;
	char *file;
	XbelApplication *appl;
	GList *list = NULL;
	XbelBookmark *book = NULL;

	file = g_build_filename(g_get_user_data_dir(), filename, NULL);

	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		g_free(file);
		return;
	}
	if (!options.uri) {
		EPRINTF("cannot record %s without a uri\n", filename);
		g_free(file);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		g_free(file);
		return;
	}

	/* 1) read in the recently-used.xbel file (uri only) */
	if (!(f = fopen(file, "a+"))) {
		EPRINTF("cannot open %s file: '%s'\n", filename, file);
		g_free(file);
		return;
	}
	dummy = lockf(fileno(f), F_LOCK, 0);
	if (fstat(fileno(f), &st)) {
		EPRINTF("cannot state open file: '%s'\n", file);
		fclose(f);
		g_free(file);
		return;
	}
	g_free(file);
	if (st.st_size > 0) {
		GMarkupParseContext *ctx;

		GMarkupParser parser = {
			.start_element = xbel_start_element,
			.end_element = NULL,
			.text = xbel_text,
			.passthrough = NULL,
			.error = NULL,
		};
		gchar buf[BUFSIZ];
		gsize got;

		if (!(ctx = g_markup_parse_context_new(&parser,
						       G_MARKUP_TREAT_CDATA_AS_TEXT |
						       G_MARKUP_PREFIX_ERROR_POSITION,
						       &list, NULL))) {
			EPRINTF("cannot create XML parser\n");
			fclose(f);
			return;
		}
		while ((got = fread(buf, 1, BUFSIZ, f)) > 0) {
			if (!g_markup_parse_context_parse(ctx, buf, got, NULL)) {
				EPRINTF("could not parse buffer contents\n");
				g_markup_parse_context_unref(ctx);
				fclose(f);
				return;
			}
		}
		if (!g_markup_parse_context_end_parse(ctx, NULL)) {
			EPRINTF("could not end parsing\n");
			g_markup_parse_context_unref(ctx);
			fclose(f);
			return;
		}
		g_markup_parse_context_unref(ctx);
	}

	/* 2) append new information (uri only) */
	if (!(item = g_list_find_custom(list, options.uri, href_match))) {
		book = calloc(1, sizeof(*book));
		book->href = strdup(options.uri);
		book->mime = strdup("application/x-desktop");
		book->added = now;
		book->private = TRUE;
		list = g_list_append(list, book);
	} else
		book = item->data;
	book->modified = now;
	book->visited = now;
	if (!book->title && fields.name)
		book->title = strdup(fields.name);
	if (!book->desc && fields.description)
		book->desc = strdup(fields.description);
	if (!(item = g_list_find_custom(book->groups, "Application", grp_match)))
		book->groups = g_list_append(book->groups, strdup("Application"));
	if (!(item = g_list_find_custom(book->applications, "XDG Launcher", app_match))) {
		appl = calloc(1, sizeof(*appl));
		appl->name = strdup("XDG Launcher");
		appl->exec = strdup("'xdg-launch %f'");
		book->applications = g_list_append(book->applications, appl);
	} else
		appl = item->data;
	appl->modified = now;
	appl->count++;

	/* 3) write out the recently-used.xbel file (uri only) */
	dummy = ftruncate(fileno(f), 0);
	fseek(f, 0L, SEEK_SET);

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", f);
	fputs("<xbel version=\"1.0\"\n", f);
	fputs("      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n",
	      f);
	fputs("      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\"\n", f);
	fputs(">\n", f);
	g_list_foreach(list, xbel_write, f);
	fputs("</xbel>\n", f);
	fflush(f);
	dummy = lockf(fileno(f), F_ULOCK, 0);
	fclose(f);
	g_list_free_full(list, xbel_free);
	list = NULL;
	(void) dummy;
}

static void
put_recently_used_xbel(char *filename)
{
	FILE *f;
	int dummy;
	GList *item;
	struct stat st;
	char *file;
	XbelApplication *appl;
	GList *list = NULL;
	XbelBookmark *book = NULL;
	char *temp = NULL, *exec = NULL;
	int len;

	file = g_build_filename(g_get_user_data_dir(), filename, NULL);

	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		g_free(file);
		return;
	}
	if (!options.url) {
		EPRINTF("cannot record %s without a url\n", filename);
		g_free(file);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		g_free(file);
		return;
	}

	/* 1) read in the recently-used.xbel file (url only) */
	if (!(f = fopen(file, "a+"))) {
		EPRINTF("cannot open %s file: '%s'\n", filename, file);
		g_free(file);
		return;
	}
	dummy = lockf(fileno(f), F_LOCK, 0);
	if (fstat(fileno(f), &st)) {
		EPRINTF("cannot stat open file: '%s'\n", file);
		fclose(f);
		g_free(file);
		return;
	}
	g_free(file);
	if (st.st_size > 0) {
		GMarkupParseContext *ctx;

		GMarkupParser parser = {
			.start_element = xbel_start_element,
			.end_element = NULL,
			.text = xbel_text,
			.passthrough = NULL,
			.error = NULL,
		};
		gchar buf[BUFSIZ];
		gsize got;

		if (!(ctx = g_markup_parse_context_new(&parser,
						       G_MARKUP_TREAT_CDATA_AS_TEXT |
						       G_MARKUP_PREFIX_ERROR_POSITION,
						       &list, NULL))) {
			EPRINTF("cannot create XML parser\n");
			fclose(f);
			return;
		}
		while ((got = fread(buf, 1, BUFSIZ, f)) > 0) {
			if (!g_markup_parse_context_parse(ctx, buf, got, NULL)) {
				EPRINTF("could not parse buffer contents\n");
				g_markup_parse_context_unref(ctx);
				fclose(f);
				return;
			}
		}
		if (!g_markup_parse_context_end_parse(ctx, NULL)) {
			EPRINTF("could not end parsing\n");
			g_markup_parse_context_unref(ctx);
			fclose(f);
			return;
		}
		g_markup_parse_context_unref(ctx);
	}

	/* 2) append new information (url only) */
	if (!(item = g_list_find_custom(list, options.url, href_match))) {
		book = calloc(1, sizeof(*book));
		book->href = strdup(options.url);
		if (!(book->mime = get_mime_type(options.url))) {
			EPRINTF("could not get mime type for %s\n", options.url);
			if (entry.MimeType) {
				char *p;

				book->mime = strdup(entry.MimeType);
				if ((p = strchr(book->mime, ';')))
					*p = '\0';
			} else
				book->mime = strdup("application/octet-stream");
		}
		book->added = now;
		book->private = TRUE;
		list = g_list_append(list, book);
	} else
		book = item->data;
	book->modified = now;
	book->visited = now;
	if (entry.Categories) {
		char *p;

		temp = strdup(entry.Categories);
		if ((p = strchr(temp, ';')))
			*p = '\0';
		/* reserved for application launching */
		if (!strcmp(temp, "Application")) {
			free(temp);
			temp = NULL;
		}
	}
	if (temp && !(item = g_list_find_custom(book->groups, temp, grp_match)))
		book->groups = g_list_append(book->groups, strdup(temp));
	free(temp);
	if (fields.application_id) {
		len = strlen(NAME) + 1 + strlen(fields.application_id) + 1 + strlen("%u") + 1;
		exec = calloc(len, sizeof(*exec));
		snprintf(exec, len, "%s %s %%u", NAME, fields.application_id);
		temp = NAME;
	} else {
		exec = strdup(options.rawcmd);
		temp = fields.bin ? : entry.Name;
	}
	if (!(item = g_list_find_custom(book->applications, temp, app_match))) {
		appl = calloc(1, sizeof(*appl));
		appl->name = strdup(temp);
		appl->exec = exec;
		exec = NULL;
		book->applications = g_list_append(book->applications, appl);
	} else
		appl = item->data;
	free(exec);
	appl->modified = now;
	appl->count++;

	/* 3) write out the recently-used.xbel file (url only) */
	dummy = ftruncate(fileno(f), 0);
	fseek(f, 0L, SEEK_SET);

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", f);
	fputs("<xbel version=\"1.0\"\n", f);
	fputs("      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n",
	      f);
	fputs("      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\"\n", f);
	fputs(">\n", f);
	g_list_foreach(list, xbel_write, f);
	fputs("</xbel>\n", f);
	fflush(f);
	dummy = lockf(fileno(f), F_ULOCK, 0);
	fclose(f);
	g_list_free_full(list, xbel_free);
	list = NULL;
	(void) dummy;
}

#endif				/* RECENTLY_USED_XBEL */

typedef struct {
	gchar *uri;
	gchar *mime;
	time_t stamp;
	gboolean private;
	GList *groups;
} RecentItem;

static void
ru_xml_start_element(GMarkupParseContext *ctx, const gchar *name, const gchar **attrs,
		     const gchar **values, gpointer user, GError **err)
{
	GList **list = user;
	GList *item = g_list_first(*list);
	RecentItem *cur = item ? item->data : NULL;

	if (!strcmp(name, "RecentItem")) {
		if (!(cur = calloc(1, sizeof(RecentItem)))) {
			EPRINTF("could not allocate element\n");
			exit(1);
		}
		*list = g_list_prepend(*list, cur);
	} else if (!strcmp(name, "Private")) {
		if (cur)
			cur->private = TRUE;
	}
}

static void
ru_xml_text(GMarkupParseContext *ctx, const gchar *text, gsize len, gpointer user, GError **err)
{
	GList **list = user;
	GList *item = g_list_first(*list);
	RecentItem *cur = item ? item->data : NULL;
	const gchar *name;
	char *buf, *end = NULL;
	unsigned long int val;

	name = g_markup_parse_context_get_element(ctx);
	if (!strcmp(name, "URI")) {
		free(cur->uri);
		cur->uri = strndup(text, len);
	} else if (!strcmp(name, "Mime-Type")) {
		free(cur->mime);
		cur->mime = strndup(text, len);
	} else if (!strcmp(name, "Timestamp")) {
		cur->stamp = 0;
		buf = strndup(text, len);
		val = strtoul(buf, &end, 0);
		if (end && *end == '\0')
			cur->stamp = val;
		free(buf);
	} else if (!strcmp(name, "Group")) {
		buf = strndup(text, len);
		cur->groups = g_list_append(cur->groups, buf);
	}
}

gint
recent_sort(gconstpointer a, gconstpointer b)
{
	const RecentItem *A = a;
	const RecentItem *B = b;

	if (A->stamp > B->stamp)
		return (-1);
	if (A->stamp < B->stamp)
		return (1);
	return (0);
}

static void
groups_write(gpointer data, gpointer user)
{
	char *g = (typeof(g)) data;
	FILE *f = (typeof(f)) user;
	gchar *s;

	s = g_markup_printf_escaped("<Group>%s</Group>", g);
	fprintf(f, "      %s\n", s);
	g_free(s);
}

unsigned int itemcount = 0;

static void
recent_write(gpointer data, gpointer user)
{
	RecentItem *r = (typeof(r)) data;
	FILE *f = (typeof(f)) user;
	gchar *s;

	/* not really supposed to be bigger than 500 items */
	if (itemcount >= 500)
		return;
	fprintf(f, "  %s\n", "<RecentItem>");

	s = g_markup_printf_escaped("<URI>%s</URI>", r->uri);
	fprintf(f, "    %s\n", s);
	g_free(s);
	s = g_markup_printf_escaped("<Mime-Type>%s</Mime-Type>", r->mime);
	fprintf(f, "    %s\n", s);
	g_free(s);
	fprintf(f, "    <Timestamp>%lu</Timestamp>\n", r->stamp);
	if (r->private) {
		fprintf(f, "    %s\n", "<Private/>");
	}
	if (r->groups) {
		fprintf(f, "    %s\n", "<Groups>");
		g_list_foreach(r->groups, groups_write, f);
		fprintf(f, "    %s\n", "</Groups>");
	}

	fprintf(f, "  %s\n", "</RecentItem>");
	itemcount++;
}

static gint
uri_match(gconstpointer data, gconstpointer user)
{
	const RecentItem *r = data;

	DPRINTF("matching '%s' and '%s'\n", r->uri, (char *) user);
	return (strcmp(r->uri, user));
}

static gint
grps_match(gconstpointer a, gconstpointer b)
{
	return strcmp(a, b);
}

static void
group_free(gpointer data)
{
	free(data);
}

static void
recent_free(gpointer data)
{
	RecentItem *r = data;

	free(r->uri);
	r->uri = NULL;
	free(r->mime);
	r->mime = NULL;
	r->stamp = 0;
	r->private = FALSE;
	g_list_free_full(r->groups, group_free);
	r->groups = NULL;
	free(data);
}

static void
put_recent_applications(char *filename)
{
	FILE *f;
	int dummy;
	GList *item;
	struct stat st;
	char *file;
	GList *list = NULL;
	RecentItem *used;

	file = g_build_filename(g_get_home_dir(), filename, NULL);
	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		g_free(file);
		return;
	}
	if (!options.uri) {
		EPRINTF("cannot record %s without a uri\n", filename);
		g_free(file);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		g_free(file);
		return;
	}

	/* 1) read in the recently-used file (uri only) */
	if (!(f = fopen(file, "a+"))) {
		EPRINTF("cannot open %s file: '%s'\n", filename, file);
		g_free(file);
		return;
	}
	dummy = lockf(fileno(f), F_LOCK, 0);
	if (fstat(fileno(f), &st)) {
		EPRINTF("cannot state open file: '%s'\n", file);
		fclose(f);
		g_free(file);
		return;
	}
	g_free(file);
	if (st.st_size > 0) {
		GMarkupParseContext *ctx;

		GMarkupParser parser = {
			.start_element = ru_xml_start_element,
			.end_element = NULL,
			.text = ru_xml_text,
			.passthrough = NULL,
			.error = NULL,
		};
		gchar buf[BUFSIZ];
		gsize got;

		if (!(ctx = g_markup_parse_context_new(&parser,
						       G_MARKUP_TREAT_CDATA_AS_TEXT |
						       G_MARKUP_PREFIX_ERROR_POSITION,
						       &list, NULL))) {
			EPRINTF("cannot create XML parser\n");
			fclose(f);
			return;
		}
		while ((got = fread(buf, 1, BUFSIZ, f)) > 0) {
			if (!g_markup_parse_context_parse(ctx, buf, got, NULL)) {
				EPRINTF("could not parse buffer contents\n");
				g_markup_parse_context_unref(ctx);
				fclose(f);
				return;
			}
		}
		if (!g_markup_parse_context_end_parse(ctx, NULL)) {
			EPRINTF("could not end parsing\n");
			g_markup_parse_context_unref(ctx);
			fclose(f);
			return;
		}
		g_markup_parse_context_unref(ctx);
	}

	/* 2) append new information (uri only) */
	if (!(item = g_list_find_custom(list, options.uri, uri_match))) {
		used = calloc(1, sizeof(*used));
		used->uri = strdup(options.uri);
		used->private = TRUE;
		list = g_list_prepend(list, used);
	} else
		used = item->data;

	if (used->mime && strcmp(used->mime, "application/x-desktop")) {
		free(used->mime);
		used->mime = strdup("application/x-desktop");
		used->private = TRUE;
		g_list_free_full(used->groups, group_free);
	} else if (!used->mime) {
		used->mime = strdup("application/x-desktop");
	}
	if (!(item = g_list_find_custom(used->groups, "recently-used-apps", grps_match)))
		used->groups = g_list_append(used->groups, strdup("recently-used-apps"));
	if (!(item = g_list_find_custom(used->groups, NAME, grps_match)))
		used->groups = g_list_append(used->groups, strdup(NAME));
	used->stamp = now;

	/* 3) write out the recently-used file (uri only) */
	dummy = ftruncate(fileno(f), 0);
	fseek(f, 0L, SEEK_SET);

	list = g_list_sort(list, recent_sort);
	fprintf(f, "%s\n", "<?xml version=\"1.0\"?>");
	fprintf(f, "%s\n", "<RecentFiles>");
	itemcount = 0;
	g_list_foreach(list, recent_write, f);
	fprintf(f, "%s\n", "</RecentFiles>");
	fflush(f);
	dummy = lockf(fileno(f), F_ULOCK, 0);
	fclose(f);
	g_list_free_full(list, recent_free);
	list = NULL;
	(void) dummy;
}

static void
put_recently_used(char *filename)
{
	FILE *f;
	int dummy;
	GList *item;
	struct stat st;
	char *file;
	GList *list = NULL;
	RecentItem *used;

	file = g_build_filename(g_get_home_dir(), filename, NULL);
	if (!file) {
		EPRINTF("cannot record %s without a file\n", filename);
		g_free(file);
		return;
	}
	if (!options.url) {
		EPRINTF("cannot record %s without a url\n", filename);
		g_free(file);
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		g_free(file);
		return;
	}

	/* 1) read in the recently-used file (url only) */
	if (!(f = fopen(file, "a+"))) {
		EPRINTF("cannot open %s file: '%s'\n", filename, file);
		g_free(file);
		return;
	}
	dummy = lockf(fileno(f), F_LOCK, 0);
	if (fstat(fileno(f), &st)) {
		EPRINTF("cannot state open file: '%s'\n", file);
		fclose(f);
		g_free(file);
		return;
	}
	g_free(file);
	if (st.st_size > 0) {
		GMarkupParseContext *ctx;

		GMarkupParser parser = {
			.start_element = ru_xml_start_element,
			.end_element = NULL,
			.text = ru_xml_text,
			.passthrough = NULL,
			.error = NULL,
		};
		gchar buf[BUFSIZ];
		gsize got;

		if (!(ctx = g_markup_parse_context_new(&parser,
						       G_MARKUP_TREAT_CDATA_AS_TEXT |
						       G_MARKUP_PREFIX_ERROR_POSITION,
						       &list, NULL))) {
			EPRINTF("cannot create XML parser\n");
			fclose(f);
			return;
		}
		while ((got = fread(buf, 1, BUFSIZ, f)) > 0) {
			if (!g_markup_parse_context_parse(ctx, buf, got, NULL)) {
				EPRINTF("could not parse buffer contents\n");
				g_markup_parse_context_unref(ctx);
				fclose(f);
				return;
			}
		}
		if (!g_markup_parse_context_end_parse(ctx, NULL)) {
			EPRINTF("could not end parsing\n");
			g_markup_parse_context_unref(ctx);
			fclose(f);
			return;
		}
		g_markup_parse_context_unref(ctx);
	}

	/* 2) append new information (url only) */
	if (!(item = g_list_find_custom(list, options.url, uri_match))) {
		char *p;

		used = calloc(1, sizeof(*used));
		used->uri = strdup(options.url);
		used->private = TRUE;
		if (!(used->mime = get_mime_type(options.url))) {
			EPRINTF("could not get mime type for %s\n", options.url);
			if (entry.MimeType) {
				used->mime = strdup(entry.MimeType);
				if ((p = strchr(used->mime, ';')))
					*p = '\0';
			} else
				used->mime = strdup("application/octet-stream");
		}
		list = g_list_prepend(list, used);
	} else
		used = item->data;

	if (entry.Categories) {
		char *grp, *p;

		grp = strdup(entry.Categories);
		if ((p = strchr(grp, ';')))
			*p = '\0';
		if (!(item = g_list_find_custom(used->groups, grp, grps_match)))
			used->groups = g_list_append(used->groups, grp);
		else
			free(grp);
	}
	used->stamp = now;

	/* 3) write out the recently-used file (url only) */
	dummy = ftruncate(fileno(f), 0);
	fseek(f, 0L, SEEK_SET);

	list = g_list_sort(list, recent_sort);
	fprintf(f, "%s\n", "<?xml version=\"1.0\"?>");
	fprintf(f, "%s\n", "<RecentFiles>");
	itemcount = 0;
	g_list_foreach(list, recent_write, f);
	fprintf(f, "%s\n", "</RecentFiles>");
	fflush(f);
	dummy = lockf(fileno(f), F_ULOCK, 0);
	fclose(f);
	g_list_free_full(list, recent_free);
	list = NULL;
	(void) dummy;
}

#endif				/* RECENTLY_USED */

static void
line_free(gpointer data)
{
	free(data);
}

static gint
line_sort(gconstpointer a, gconstpointer b)
{
	return strcmp(a, b);
}

static void
line_write(gpointer data, gpointer user)
{
	FILE *f = user;

	fputs(data, f);
	fputc('\n', f);
}

static void
put_line_history(char *file, char *line)
{
	FILE *f;
	int dummy;
	char *buf, *p;
	int discarding = 0, n = 0, keep;
	GList *history = NULL;

	keep = defaults.keep;
	if (options.keep > 0)
		keep = options.keep;

	DPRINTF("maximum history entries '%d'\n", keep);

	if (!file) {
		EPRINTF("cannot record history without a file\n");
		return;
	}
	if (!line) {
		EPRINTF("cannot record history without a line\n");
		return;
	}
	if (options.autostart || options.xsession) {
		DPRINTF("do not record autostart or xsession invocations\n");
		return;
	}
	/* 1) read in the history file */
	if (!(f = fopen(file, "a+"))) {
		EPRINTF("cannot open history file: '%s'\n", file);
		return;
	}
	dummy = lockf(fileno(f), F_LOCK, 0);
	buf = calloc(PATH_MAX + 2, sizeof(*buf));
	while (fgets(buf, PATH_MAX, f)) {
		if ((p = strrchr(buf, '\n'))) {
			if (!discarding)
				*p = '\0';
			else {
				discarding = 0;
				continue;
			}
		} else {
			discarding = 1;
			continue;
		}
		p = buf + strspn(buf, " \t");
		if (!*p)
			continue;
		DPRINTF("read line from %s: '%s'\n", file, p);
		if (line_sort(p, line) == 0)
			continue;
		if (g_list_find_custom(history, p, line_sort))
			continue;
		history = g_list_append(history, strdup(p));
		if (++n >= keep)
			break;
	}

	/* 2) append new information */
	history = g_list_prepend(history, strdup(line));

	/* 3) write out the history file */
	dummy = ftruncate(fileno(f), 0);
	fseek(f, 0L, SEEK_SET);
	g_list_foreach(history, line_write, f);
	fflush(f);
	dummy = lockf(fileno(f), F_ULOCK, 0);
	fclose(f);
	g_list_free_full(history, line_free);
	history = NULL;
	(void) dummy;
}

static void
put_history()
{
	put_line_history(options.runhist, fields.command);
	put_line_history(options.recapps, fields.application_id);
#ifdef RECENTLY_USED
	if (options.uri) {
		if (options.url) {
			put_recently_used(".recently-used");
			put_recently_used_xbel("recently-used.xbel");
		}
		put_recent_applications(".recently-used");
		put_recent_applications(".recent-applications");
		put_recent_applications_xbel("recently-used.xbel");
		put_recent_applications_xbel("recent-applications.xbel");
	}
#endif				/* RECENTLY_USED */
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
Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016  Monavacon Limited.\n\
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
    %1$s [options] [APPID [FILE|URL]]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static const char *
show_bool(Bool truth)
{
	if (truth)
		return "true";
	return "false";
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] [APPID [FILE|URL]]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    APPID\n\
        the application identifier of XDG application to launch\n\
    FILE\n\
        the file name with which to launch the application\n\
    URL\n\
        the URL with which to launch the application\n\
Options:\n\
    -L, --launcher LAUNCHER\n\
        name of launcher for startup id, [default: '%2$s']\n\
    -l, --launchee LAUNCHEE\n\
        name of launchee for startup id, [default: APPID]\n\
    -n, --hostname HOSTNAME\n\
        hostname to use in startup id, [default: '%3$s']\n\
    -m, --monitor MONITOR\n\
        Xinerama monitor to specify in SCREEN tag, [default: %4$d]\n\
    -s, --screen SCREEN\n\
        screen to specify in SCREEN tag, [default: %5$d]\n\
    -w, --workspace DESKTOP\n\
        workspace to specify in DESKTOP tag, [default: %6$d]\n\
    -t, --timestamp TIMESTAMP\n\
        X server timestamp for startup id, [default: %7$lu]\n\
    -N, --name NAME\n\
        name of XDG application, [default: '%8$s']\n\
    -i, --icon ICON\n\
        icon name of the XDG application, [default: '%9$s']\n\
    -b, --binary BINARY\n\
        binary name of the XDG application, [default: '%10$s']\n\
    -D, --description DESCRIPTION\n\
        description of the XDG application, [default: '%11$s']\n\
    -W, --wmclass WMCLASS\n\
        resource name or class of the XDG application, [default: '%12$s']\n\
    -q, --silent SILENT\n\
        whether startup notification is silent (0/1), [default: '%13$s']\n\
    -p, --pid PID\n\
        process id of the XDG application, [default: '%14$d']\n\
    -a, --appid APPID\n\
        override application identifier\n\
    -x, --exec EXEC\n\
        override command to execute\n\
    -f, --file FILE\n\
        filename to use with application\n\
    -u, --url URL\n\
        URL to use with application\n\
    -K, --keyboard\n\
        determine screen (monitor) from keyboard focus, [default: '%15$s']\n\
    -P, --pointer\n\
        determine screen (monitor) from pointer location, [default: '%16$s']\n\
    -A, --action ACTION\n\
        specify a desktop action other than the default, [default: '%17$s']\n\
    -X, --xsession\n\
        interpret entry as xsession instead of application, [default: '%18$s']\n\
    -U, --autostart\n\
        interpret entry as autostart instead of application, [default: '%19$s']\n\
    -k, --keep NUMBER\n\
        specify NUMBER of recent applications to keep, [default: '%20$d']\n\
    -r, --recent FILENAME\n\
        specify FILENAME of recent apps file, [default: '%21$s']\n\
    -I, --info\n\
        print information about entry instead of launching, [default: '%22$s']\n\
    -T, --toolwait\n\
        wait for startup to complete and then exit, [default: '%23$s']\n\
    -timeout SECONDS\n\
        consider startup complete after SECONDS seconds, [default: '%24$d']\n\
    -mappings MAPPINGS\n\
        consider startup complete after MAPPINGS mappings, [default: '%25$d']\n\
    -withdrawn\n\
        consider withdrawn state mappings, [default: '%26$s']\n\
    -pid\n\
        print the pid of the process to standard out, [default: '%27$s']\n\
    -wid\n\
        print the window id to standard out, [default: '%28$s']\n\
    -noprop\n\
        use top-level creations instead of mappings, [default: '%29$s']\n\
    -M, --manager\n\
        wait for window manager before launching, [default: '%30$s']\n\
    -Y, --tray\n\
        wait for system tray before launching, [default: '%31$s']\n\
    -G, --pager\n\
        wait for desktop pager before launching, [default: '%32$s']\n\
    -O, --composite\n\
        wait for composite manager before launching, [default: '%33$s']\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: 0]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: 1]\n\
        this option may be repeated.\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
", argv[0]
	/* *INDENT-OFF* */
	, defaults.launcher
	, defaults.hostname
	, defaults.monitor
	, defaults.screen
	, defaults.desktop
	, defaults.timestamp
	, defaults.name
	, defaults.icon
	, defaults.binary
	, defaults.description
	, defaults.wmclass
	, defaults.silent
	, defaults.pid
	, show_bool(defaults.keyboard)
	, show_bool(defaults.pointer)
	, defaults.action
	, show_bool(defaults.xsession)
	, show_bool(defaults.autostart)
	, defaults.keep
	, defaults.recapps
	, show_bool(defaults.info)
	, show_bool(defaults.toolwait)
	, defaults.timeout
	, defaults.mappings
	, show_bool(defaults.withdrawn)
	, show_bool(defaults.printpid)
	, show_bool(defaults.printwid)
	, show_bool(defaults.noprop)
	, show_bool(defaults.manager)
	, show_bool(defaults.tray)
	, show_bool(defaults.pager)
	, show_bool(defaults.composite)
	/* *INDENT-ON* */
	);
}

static void
set_default_files()
{
	static const char *rsuffix = "/xde/run-history";
	static const char *asuffix = "/xde/recent-applications";
	static const char *xsuffix = "/recently-used";
	static const char *hsuffix = "/.recently-used";
	int len;
	char *env;

	if ((env = getenv("XDG_CONFIG_HOME"))) {
		len = strlen(env) + strlen(rsuffix) + 1;
		free(options.runhist);
		defaults.runhist = options.runhist = calloc(len, sizeof(*options.runhist));
		strcpy(options.runhist, env);
		strcat(options.runhist, rsuffix);

		len = strlen(env) + strlen(asuffix) + 1;
		free(options.recapps);
		defaults.recapps = options.recapps = calloc(len, sizeof(*options.recapps));
		strcpy(options.recapps, env);
		strcat(options.recapps, asuffix);

		len = strlen(env) + strlen(xsuffix) + 1;
		free(options.recently);
		defaults.recently = options.recently = calloc(len, sizeof(*options.recently));
		strcpy(options.recently, env);
		strcat(options.recently, xsuffix);
	} else {
		static const char *cfgdir = "/.config";
		static const char *datdir = "/.local/share";

		env = getenv("HOME") ? : ".";

		len = strlen(env) + strlen(cfgdir) + strlen(rsuffix) + 1;
		free(options.runhist);
		defaults.runhist = options.runhist = calloc(len, sizeof(*options.runhist));
		strcpy(options.runhist, env);
		strcat(options.runhist, cfgdir);
		strcat(options.runhist, rsuffix);

		len = strlen(env) + strlen(cfgdir) + strlen(asuffix) + 1;
		free(options.recapps);
		defaults.recapps = options.recapps = calloc(len, sizeof(*options.recapps));
		strcpy(options.recapps, env);
		strcat(options.recapps, cfgdir);
		strcat(options.recapps, asuffix);

		len = strlen(env) + strlen(datdir) + strlen(xsuffix) + 1;
		free(options.recently);
		defaults.recently = options.recently = calloc(len, sizeof(*options.recently));
		strcpy(options.recently, env);
		strcat(options.recently, datdir);
		strcat(options.recently, xsuffix);
	}
	if (access(options.recently, R_OK | W_OK)) {
		env = getenv("HOME") ? : ".";

		len = strlen(env) + strlen(hsuffix) + 1;
		free(options.recently);
		defaults.recently = options.recently = calloc(len, sizeof(*options.recently));
		strcpy(options.recently, env);
		strcat(options.recently, hsuffix);
	}
	return;
}

static void
set_defaults(int argc, char *argv[])
{
	char *buf, *disp, *p;

	free(options.launcher);
	buf = (p = strrchr(argv[0], '/')) ? p + 1 : argv[0];
	defaults.launcher = options.launcher = strdup(buf);
	if (!strcmp(buf, "xdg-xsession"))
		defaults.xsession = options.xsession = True;
	else if (!strcmp(buf, "xdg-autostart"))
		defaults.autostart = options.autostart = True;

	free(options.hostname);
	buf = defaults.hostname = options.hostname = calloc(64, sizeof(*buf));
	gethostname(buf, 64);

	defaults.pid = options.pid = getpid();

	if ((disp = getenv("DISPLAY")))
		if ((p = strrchr(disp, '.')) && strspn(p + 1, "0123456789") == strlen(p + 1))
			defaults.screen = options.screen = atoi(p + 1);

	set_default_files();
}

int
main(int argc, char *argv[])
{
	int exec_mode = 0;		/* application mode is default */
	char *p;

	if ((p = getenv("DESKTOP_STARTUP_ID")) && *p) {
		free(options.id);
		options.id = strdup(p);
	}
	unsetenv("DESKTOP_STARTUP_ID");

#ifdef RECENTLY_USED
	now = time(NULL);
#ifdef RECENTLY_USED_XBEL
	gtk_init(NULL, NULL);
#endif
#endif

	set_defaults(argc, argv);

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"launcher",	required_argument,	NULL, 'L'},
			{"launchee",	required_argument,	NULL, 'l'},
			{"sequence",	required_argument,	NULL, 'S'},
			{"hostname",	required_argument,	NULL, 'n'},
			{"monitor",	required_argument,	NULL, 'm'},
			{"screen",	required_argument,	NULL, 's'},
			{"workspace",	required_argument,	NULL, 'w'},
			{"timestamp",	required_argument,	NULL, 't'},
			{"name",	required_argument,	NULL, 'N'},
			{"icon",	required_argument,	NULL, 'i'},
			{"binary",	required_argument,	NULL, 'b'},
			{"description",	required_argument,	NULL, 'd'},
			{"wmclass",	required_argument,	NULL, 'W'},
			{"silent",	required_argument,	NULL, 'q'},
			{"pid",		optional_argument,	NULL, 'p'},
			{"appid",	required_argument,	NULL, 'a'},
			{"exec",	required_argument,	NULL, 'x'},
			{"file",	required_argument,	NULL, 'f'},
			{"url",		required_argument,	NULL, 'u'},
			{"keyboard",	no_argument,		NULL, 'K'},
			{"pointer",	no_argument,		NULL, 'P'},
			{"action",	required_argument,	NULL, 'A'},
			{"xsession",	no_argument,		NULL, 'X'},
			{"autostart",	no_argument,		NULL, 'U'},
			{"keep",	required_argument,	NULL, 'k'},
			{"recent",	required_argument,	NULL, 'r'},
			{"info",	no_argument,		NULL, 'I'},

			{"toolwait",	no_argument,		NULL, 'T'},
			{"timeout",	required_argument,	NULL,  1 },
			{"mappings",	required_argument,	NULL,  2 },
			{"withdrawn",	no_argument,		NULL,  3 },
			{"wid",		no_argument,		NULL,  4 },
			{"noprop",	no_argument,		NULL,  5 },

			{"assist",	no_argument,		NULL,  6 },

			{"manager",	no_argument,		NULL, 'M'},
			{"tray",	no_argument,		NULL, 'Y'},
			{"pager",	no_argument,		NULL, 'G'},
			{"composite",	no_argument,		NULL, 'O'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv,
				     "L:l:S:n:m:s:p::w:t:N:i:b:d:W:q:a:ex:f:u:KPA:XUk:r:ITMYGOD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "L:l:S:n:m:s:p:w:t:N:i:b:d:W:q:a:ex:f:u:KPA:XUk:r:ITMYGODvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1 || exec_mode) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'L':	/* -L, --lancher LAUNCHER */
			free(options.launcher);
			defaults.launcher = options.launcher = strdup(optarg);
			break;
		case 'l':	/* -l, --launchee LAUNCHEE */
			free(options.launchee);
			defaults.launchee = options.launchee = strdup(optarg);
			break;
		case 'S':	/* -S, --sequence SEQUENCE */
			free(options.sequence);
			defaults.sequence = options.sequence = strdup(optarg);
			break;
		case 'n':	/* -n, --hostname HOSTNAME */
			free(options.hostname);
			defaults.hostname = options.hostname = strdup(optarg);
			break;
		case 'm':	/* -m, --monitor MONITOR */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			defaults.monitor = options.monitor = val;
			break;
		case 's':	/* -s, --screen SCREEN */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			screen = defaults.screen = options.screen = val;
			break;
		case 'p':	/* -p, --pid PID */
			if (optarg) {
				if ((val = strtoul(optarg, &endptr, 0)) < 0)
					goto bad_option;
				if (endptr && *endptr)
					goto bad_option;
				defaults.pid = options.pid = val;
			} else {
				defaults.printpid = options.printpid = True;
			}
			break;
		case 'w':	/* -w, --workspace WORKSPACE */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			defaults.desktop = options.desktop = val;
			break;
		case 't':	/* -t, --timestamp TIMESTAMP */
			if ((val = strtoul(optarg, &endptr, 0)) <= 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			defaults.timestamp = options.timestamp = val;
			break;
		case 'N':	/* -N, --name NAME */
			free(options.name);
			defaults.name = options.name = strdup(optarg);
			break;
		case 'i':	/* -i, --icon ICON */
			free(options.icon);
			defaults.icon = options.icon = strdup(optarg);
			break;
		case 'b':	/* -b, --binary BINARY */
			free(options.binary);
			defaults.binary = options.binary = strdup(optarg);
			break;
		case 'd':	/* -d, --description DESCRIPTION */
			free(options.description);
			defaults.description = options.description = strdup(optarg);
			break;
		case 'W':	/* -W, --wmclass WMCLASS */
			free(options.wmclass);
			defaults.wmclass = options.wmclass = strdup(optarg);
			break;
		case 'q':	/* -q, --silent SILENT */
			free(options.silent);
			defaults.silent = options.silent = strdup(optarg);
			break;
		case 'a':	/* -a, --appid APPID */
			free(options.appid);
			defaults.appid = options.appid = strdup(optarg);
			break;
		case 'x':	/* -x, --exec EXEC */
			free(options.exec);
			defaults.exec = options.exec = strdup(optarg);
			break;
		case 'e':	/* -e command and options */
			exec_mode = 1;
			break;
		case 'f':	/* -f, --file FILE */
			free(options.file);
			defaults.file = options.file = strdup(optarg);
			break;
		case 'u':	/* -u, --url URL */
			free(options.url);
			defaults.url = options.url = strdup(optarg);
			break;
		case 'K':	/* -K, --keyboard */
			if (options.pointer)
				goto bad_option;
			defaults.keyboard = options.keyboard = True;
			break;
		case 'P':	/* -P, --pointer */
			if (options.keyboard)
				goto bad_option;
			defaults.keyboard = options.keyboard = True;
			break;
		case 'A':	/* -A, --action ACTION */
			free(options.action);
			defaults.action = options.action = strdup(optarg);
			break;
		case 'X':	/* -X, --xsession */
			if (options.autostart)
				goto bad_option;
			defaults.xsession = options.xsession = True;
			break;
		case 'U':	/* -S, --autostart */
			if (options.xsession)
				goto bad_option;
			defaults.autostart = options.autostart = True;
			break;
		case 'k':	/* -k, --keep NUMBER */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			if (val < 1 || val > 100)
				goto bad_option;
			defaults.keep = options.keep = val;
			break;
		case 'r':	/* -r, --recent FILENAME */
			free(options.recent);
			defaults.recent = options.recent = strdup(optarg);
			break;
		case 'I':	/* -I, --info */
			defaults.info = options.info = True;
			break;
		case 'T':	/* -T, --toolwait */
			defaults.toolwait = options.toolwait = True;
			break;
		case 1:		/* --timeout TIMEOUT */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			if (val < 1 || val > 120)
				goto bad_option;
			defaults.timeout = options.timeout = val;
			break;
		case 2:		/* --mappings MAPPINGS */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			if (val < 1 || val > 10)
				goto bad_option;
			defaults.mappings = options.mappings = val;
			break;
		case 3:		/* --withdrawn */
			defaults.withdrawn = options.withdrawn = True;
			break;
		case 4:		/* --wid */
			defaults.printwid = options.printwid = True;
			break;
		case 5:		/* --noprop */
			defaults.noprop = options.noprop = True;
			break;
		case 6:		/* --assist */
			defaults.assist = options.assist = True;
			break;
		case 'M':	/* -M, --manager */
			defaults.manager = options.manager = True;
			break;
		case 'Y':	/* -Y, --tray */
			defaults.tray = options.tray = True;
			break;
		case 'G':	/* -G, --pager */
			defaults.pager = options.pager = True;
			break;
		case 'O':	/* -O, --composite */
			defaults.composite = options.composite = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
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
				fprintf(stderr, "%s: printing version message\n", argv[0]);
			version(argc, argv);
			exit(EXIT_SUCCESS);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n", argv[0]);
			copying(argc, argv);
			exit(EXIT_SUCCESS);
		case '?':
		default:
		      bad_option:
			optind--;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '", argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (!options.recent) {
		char *recent = exec_mode ? defaults.runhist : defaults.recapps;

		free(options.recent);
		defaults.recent = options.recent = strdup(recent);
	}
	if (exec_mode) {
		int i;

		if (optind >= argc)
			goto bad_nonopt;
		eargv = calloc(argc - optind + 1, sizeof(*eargv));
		eargc = argc - optind;
		for (i = 0; optind < argc; optind++, i++)
			eargv[i] = strdup(argv[optind]);
	} else if (optind < argc) {
		if (options.appid)
			optind++;
		else
			options.appid = strdup(argv[optind++]);
		if (optind < argc) {
			if (strchr(argv[optind], ':')) {
				if (options.url)
					optind++;
				else
					options.url = strdup(argv[optind++]);
			} else {
				if (options.file)
					optind++;
				else
					options.file = strdup(argv[optind++]);
			}
			if (optind > argc)
				goto bad_nonopt;
		}
	}
	if (!eargv && !options.appid && !options.exec) {
		EPRINTF("APPID or EXEC must be specified\n");
		goto bad_usage;
	} else if (eargv) {
		p = strrchr(eargv[0], '/');
		p = p ? p + 1 : eargv[0];
		free(options.path);
		if ((options.path = lookup_file(p))) {
			if (!parse_file(options.path)) {
				free(options.path);
				options.path = NULL;
				entry.TryExec = strdup(eargv[0]);
			}
		}
	} else if (options.appid) {
		free(options.path);
		if (!(options.path = lookup_file(options.appid))) {
			EPRINTF("could not find file '%s'\n", options.appid);
			exit(EXIT_FAILURE);
		}
		if (!parse_file(options.path)) {
			EPRINTF("could not parse file '%s'\n", options.path);
			exit(EXIT_FAILURE);
		}
	}
	if (options.file && !options.url) {
		int size = strlen("file://") + strlen(options.file) + 1;

		options.url = calloc(size, sizeof(*options.url));
		strcat(options.url, "file://");
		strcat(options.url, options.file);
	} else if (options.url && !options.file &&
		   (p = strstr(options.url, "file://")) == options.url) {
		options.file = strdup(options.url + 7);
	}
	if (options.path) {
		free(options.uri);
		if ((options.uri =
		     calloc(strlen("file://") + strlen(options.path) + 1, sizeof(*options.uri)))) {
			strcpy(options.uri, "file://");
			strcat(options.uri, options.path);
		}
	}
	if (options.output > 1) {
		const char **lp;
		char **ep;

		OPRINTF("Entries from file:\n");
		for (lp = DesktopEntryFields, ep = &entry.Type; *lp; lp++, ep++)
			if (*ep)
				OPRINTF("%-24s = %s\n", *lp, *ep);
	}
	if (options.info) {
		const char **lp;
		char **ep;

		fputs("Entries from file:\n\n", stdout);
		for (lp = DesktopEntryFields, ep = &entry.Type; *lp; lp++, ep++)
			if (*ep)
				fprintf(stdout, "%-24s = %s\n", *lp, *ep);
		fputs("\n", stdout);
	}
	if (!eargv && !options.exec && !entry.Exec) {
		EPRINTF("no exec command\n");
		exit(EXIT_FAILURE);
	}
	/* populate some fields */
	if (options.id) {
		if (options.info)
			fprintf(stdout, "Setting desktop startup id to: %s\n", options.id);
		free(fields.id);
		fields.id = strdup(options.id);
	}
	/* open display now */
	get_display();
	/* fill out all fields */
	set_all();
	if (!options.info)
		put_history();
	launch();
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
