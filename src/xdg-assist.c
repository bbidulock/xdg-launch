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
	Window origin;	    /* origin of message sequence */
	char *data;	    /* message bytes */
	int len;	    /* number of message bytes */
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
	int screen;
	Window win;
	Window time_win;
	Client *next;
	Bool breadcrumb;
	Bool new;
	Time active_time;
	Time focus_time;
	Time user_time;
	pid_t pid;
	char *startup_id;
	char **command;
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
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_STATE;
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
static Bool handle_WIN_CLIENT_LIST(XEvent *, Client *);
static Bool handle_WINDOWMAKER_NOTICEBOARD(XEvent *, Client *);
static Bool handle_WIN_PROTOCOLS(XEvent *, Client *);
static Bool handle_WIN_SUPPORTING_WM_CHECK(XEvent *, Client *);
static Bool handle_WM_CLIENT_MACHINE(XEvent *, Client *);
static Bool handle_WM_COMMAND(XEvent *, Client *);
static Bool handle_WM_HINTS(XEvent *, Client *);
static Bool handle_WM_STATE(XEvent *, Client *);
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
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,			&handle_NET_WM_USER_TIME,		None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,			NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,			NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,			&handle_WIN_CLIENT_LIST,		None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,		&handle_WINDOWMAKER_NOTICEBOARD,	None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,			&handle_WIN_PROTOCOLS,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,		&handle_WIN_SUPPORTING_WM_CHECK,	None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,			NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,					&handle_WM_CLIENT_MACHINE,		XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,					&handle_WM_COMMAND,			XA_WM_COMMAND		},
	{ "WM_HINTS",				NULL,					&handle_WM_HINTS,			XA_WM_HINTS		},
	{ "WM_STATE",				&_XA_WM_STATE,				&handle_WM_STATE,			None			},
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

XContext ClientContext;	/* window to client context */
XContext MessageContext; /* window to message context */

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
		/* next look in fallback subdirectory of XDG_DATA_HOME and then
		   each of the subdirectories in XDG_DATA_DIRS */
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

	from =
	    XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, ParentRelative, ParentRelative);

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
		XSendEvent(dpy, root, False, PropertyChangeMask, &xev);
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}

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
push_time(Time * accum, Time now)
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
	pid_t pid;
	char *str;

	if (c->startup_id) {
		if (!strcmp(c->startup_id, fields.id))
			return True;
		else
			return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcmp(fields.hostname, c->hostname)))
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
		if (c->ch.res_name && !strcmp(fields.wmclass, c->ch.res_name))
			return True;
		if (c->ch.res_class && !strcmp(fields.wmclass, c->ch.res_class))
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
	return False;
}

void
setup_client(Client *c)
{
	/* use /proc/[pid]/cmdline to set up WM_COMMAND if not present */
}

static void
update_client(Client *c)
{
	long card;

	XGetClassHint(dpy, c->win, &c->ch);
	XFetchName(dpy, c->win, &c->name);
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);
	}
	c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE);
	c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID);
	if (get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card))
		c->pid = card;

	if (test_client(c))
		setup_client(c);

	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win, ExposureMask | VisibilityChangeMask |
		     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
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
	if (c->startup_id)
		XFree(c->startup_id);
	XDeleteContext(dpy, c->win, ClientContext);
	if (c->time_win)
		XDeleteContext(dpy, c->time_win, ClientContext);
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

static Bool
handle_WM_COMMAND(XEvent *e, Client *c)
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

static Bool
handle_WM_STATE(XEvent *e, Client *c)
{
	long data;

	if (!c || e->type != PropertyNotify)
		return False;
	if (get_cardinal(e->xany.window, _XA_WM_STATE, AnyPropertyType, &data)
	    && data != WithdrawnState && !c)
		c = add_client(e->xany.window);
	return True;
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
remove_sequence(Sequence * del)
{
	Sequence *seq, **prev;

	for (prev = &sequences, seq = *prev; seq && seq != del;
	     prev = &seq->next, seq = *prev) ;
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
	if (cmd.f.timestamp == NULL
		&& (p = strstr(cmd.f.id, "_TIME")) != NULL)
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
	case FocusOut:
	case Expose:
	case VisibilityNotify:
		break;
	case CreateNotify:
		if (!(c = find_client(e->xcreatewindow.window)))
			c = add_client(e->xcreatewindow.window);
		break;
	case DestroyNotify:
		if ((c = find_client(e->xdestroywindow.window)))
			del_client(c);
		else
			clean_msgs(e->xdestroywindow.window);
		break;
	case UnmapNotify:
	case MapNotify:
	case ReparentNotify:
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
	handle_NET_CLIENT_LIST(NULL, NULL);
	handle_WIN_CLIENT_LIST(NULL, NULL);
	handle_NET_ACTIVE_WINDOW(NULL, NULL);
	if (fields.timestamp)
		push_time(&launch_time, (Time) strtoul(fields.timestamp, NULL, 0));
}

int signum;

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
  */
void
assist()
{
	pid_t pid;

	setup_to_assist();
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
		exit(EXIT_FAILURE);
	} else if (pid != 0) {
		/* parent exits */
		exit(EXIT_SUCCESS);
	}
	/* release current directory */
	if (chdir("/") < 0) {
		EPRINTF("%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	umask(0);		/* clear file creation mask */
	/* continue on monitoring */
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
			if (options.exec_mode != EXEC_MODE_MONITOR && options.exec_mode != EXEC_MODE_REPLACE)
				goto bad_command;
			options.exec_mode = EXEC_MODE_REPLACE;
			break;
		case 'q':	/* -q, --quit */
			if (options.exec_mode != EXEC_MODE_MONITOR && options.exec_mode != EXEC_MODE_QUIT)
				goto bad_command;
			options.exec_mode = EXEC_MODE_QUIT;
			break;
		case 'f':	/* -f, --foreground */
			options.foreground = 1;
			break;
		case 'g':	/* -g, --guard-time TIMEOUT */
			if (optarg == NULL)
				options.guardtime = 15000;
			else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.guardtime = val;
			}
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
	if (options.output > 1) {
		const char **lp;
		char **ep;

		OPRINTF("Entries from file:\n");
		for (lp = DesktopEntryFields, ep = &entry.Type; *lp; lp++, ep++)
			if (*ep)
				OPRINTF("%-24s = %s\n", *lp, *ep);
	}
	get_display();
	assist();
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
