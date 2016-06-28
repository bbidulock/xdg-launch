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
#include <execinfo.h>

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
#ifdef DESKTOP_NOTIFICATIONS
#include <libnotify/notify.h>
#endif
#ifdef SYSTEM_TRAY_STATUS_ICON
#include <gtk/gtk.h>
#endif

#if defined DESKTOP_NOTIFICATIONS || defined SYSTEM_TRAY_STATUS_ICON
#undef HAVE_GLIB_EVENT_LOOP
#define HAVE_GLIB_EVENT_LOOP 1
#endif

#if defined SYSTEM_TRAY_STATUS_ICON
#undef HAVE_GTK
#define HAVE_GTK 1
#endif

#define XPRINTF(_args...) do { } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
		fprintf(stderr, "D: %12s: +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
		fprintf(stderr, "E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output >= _num) { \
		fprintf(stderr, "I: "); \
		fprintf(stderr, _args); fflush(stderr); } } while (0)

#define PTRACE(_num) do { if (options.debug >= _num || options.output >= _num) { \
		fprintf(stderr, "T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
		fflush(stderr); } } while (0)

#define CPRINTF(_num, c, _args...) do { if (options.output >= _num) { \
		fprintf(stdout, "C: client 0x%lx ", c->win); \
		fprintf(stdout, _args); fflush(stdout); } } while (0)

void
dumpstack(const char *file, const int line, const char *func)
{
	void *buffer[32];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 32)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, "E: %12s +%4d : %s() : \t%s\n", file, line, func, strings[i]);
}

const char *program = NAME;

typedef enum {
	CommandDefault = 0,
	CommandMonitor,
	CommandReplace,
	CommandQuit,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int debug;
	int output;
	Bool assist;
	Bool feedback;
	Bool notify;
	Bool foreground;
	unsigned long guard;
	unsigned long persist;
	Command command;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.assist = True,
	.feedback = False,
	.notify = False,
	.foreground = False,
	.guard = 15000,
	.persist = 1000,
	.command = CommandDefault,
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
#endif					/* DESKTOP_NOTIFICATIONS */
#ifdef SYSTEM_TRAY_STATUS_ICON
	GtkStatusIcon *status;
#endif					/* SYSTEM_TRAY_STATUS_ICON */
#ifdef HAVE_GLIB_EVENT_LOOP
	gint timer;
#endif					/* HAVE_GLIB_EVENT_LOOP */
} Sequence;

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

const char *DesktopEntryFields[] = {
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

typedef struct {
	int screen;			/* screen number */
	Window root;			/* root window of screen */
	Window owner;			/* _XDG_ASSIST_S%d selection owner (theirs) */
	Window selwin;			/* _XDG_ASSIST_S%d selection window (ours) */
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;		/* set to root when SubstructureRedirect */
	Window stray_owner;		/* _NET_SYSTEM_TRAY_S%d owner */
	Window pager_owner;		/* _NET_DESKTOP_LAYOUT_S%d owner */
	Window compm_owner;		/* _NET_WM_CM_S%d owner */
	Atom icccm_atom;		/* WM_S%d atom for this screen */
	Atom stray_atom;		/* _NET_SYSTEM_TRAY_S%d atom this screen */
	Atom pager_atom;		/* _NET_DESKTOP_LAYOUT_S%d atom this screen */
	Atom compm_atom;		/* _NET_WM_CM_S%d atom this screen */
	Atom slctn_atom;		/* _XDG_ASSIST_S%d atom this screen */
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
	XEMBED_MAPPED = (1 << 0),
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
	Bool listed;			/* listed by window manager */
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
	char *icon_name;		/* icon name */
	char *hostname;			/* client machine */
	char *client_id;		/* session management id */
	char *role;			/* session management role */
	XClassHint ch;			/* class hint */
	XWMHints *wmh;			/* window manager hints */
	XWMState *wms;			/* window manager state */
	XEmbedInfo *xei;		/* _XEMBED_INFO property */
};

Time current_time = CurrentTime;
Time last_user_time = CurrentTime;

Atom _XA_KDE_WM_CHANGE_STATE;
Atom _XA_MANAGER;
Atom _XA_MOTIF_WM_INFO;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_CLIENT_LIST_STACKING;
Atom _XA_NET_CLOSE_WINDOW;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_MOVERESIZE_WINDOW;
Atom _XA_NET_REQUEST_FRAME_EXTENTS;
Atom _XA_NET_RESTACK_WINDOW;
Atom _XA_NET_STARTUP_ID;
Atom _XA_NET_STARTUP_INFO;
Atom _XA_NET_STARTUP_INFO_BEGIN;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_VIRTUAL_ROOTS;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_NET_WM_ALLOWED_ACTIONS;
Atom _XA_NET_WM_FULLSCREEN_MONITORS;
Atom _XA_NET_WM_ICON_GEOMETRY;
Atom _XA_NET_WM_ICON_NAME;
Atom _XA_NET_WM_MOVERESIZE;
Atom _XA_NET_WM_NAME;
Atom _XA_NET_WM_PID;
Atom _XA_NET_WM_STATE;
Atom _XA_NET_WM_STATE_FOCUSED;
Atom _XA_NET_WM_STATE_HIDDEN;
Atom _XA_NET_WM_USER_TIME;
Atom _XA_NET_WM_USER_TIME_WINDOW;
Atom _XA_NET_WM_VISIBLE_ICON_NAME;
Atom _XA_NET_WM_VISIBLE_NAME;
Atom _XA_SM_CLIENT_ID;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_APP_STATE;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WIN_CLIENT_MOVING;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_FOCUS;
Atom _XA_WIN_HINTS;
Atom _XA_WIN_LAYER;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_STATE;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_CHANGE_STATE;
Atom _XA_WM_CLIENT_LEADER;
Atom _XA_WM_DELETE_WINDOW;
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_SAVE_YOURSELF;
Atom _XA_WM_STATE;
Atom _XA_WM_WINDOW_ROLE;
Atom _XA_XEMBED;
Atom _XA_XEMBED_INFO;
Atom _XA_UTF8_STRING;
Atom _XA_NET_SYSTEM_TRAY_ORIENTATION;
Atom _XA_NET_SYSTEM_TRAY_VISUAL;
Atom _XA_NET_DESKTOP_LAYOUT;
Atom _XA_XDG_ASSIST_CMD;
Atom _XA_XDG_ASSIST_CMD_QUIT;
Atom _XA_XDG_ASSIST_CMD_REPLACE;

static void pc_handle_MOTIF_WM_INFO(XPropertyEvent *, Client *);
static void pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *, Client *);
static void pc_handle_NET_STARTUP_ID(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTED(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_PID(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *, Client *);
static void pc_handle_SM_CLIENT_ID(XPropertyEvent *, Client *);
static void pc_handle_TIMESTAMP_PROP(XPropertyEvent *, Client *);
static void pc_handle_WIN_APP_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *, Client *);
static void pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *, Client *);
static void pc_handle_WIN_FOCUS(XPropertyEvent *, Client *);
static void pc_handle_WIN_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WIN_LAYER(XPropertyEvent *, Client *);
static void pc_handle_WIN_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WIN_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_WIN_WORKSPACE(XPropertyEvent *, Client *);
static void pc_handle_WM_CLASS(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_LEADER(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *, Client *);
static void pc_handle_WM_COMMAND(XPropertyEvent *, Client *);
static void pc_handle_WM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_SIZE(XPropertyEvent *, Client *);
static void pc_handle_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_NORMAL_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WM_SIZE_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *, Client *);
static void pc_handle_WM_WINDOW_ROLE(XPropertyEvent *, Client *);
static void pc_handle_WM_ZOOM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_XEMBED_INFO(XPropertyEvent *, Client *);
static void pc_handle_NET_SYSTEM_TRAY_ORIENTATION(XPropertyEvent *, Client *);
static void pc_handle_NET_SYSTEM_TRAY_VISUAL(XPropertyEvent *, Client *);
static void pc_handle_NET_DESKTOP_LAYOUT(XPropertyEvent *, Client *);

static void cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_MANAGER(XClientMessageEvent *, Client *);
static void cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *, Client *);
static void cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_LAYER(XClientMessageEvent *, Client *);
static void cm_handle_WIN_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_WORKSPACE(XClientMessageEvent *, Client *);
static void cm_handle_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WM_PROTOCOLS(XClientMessageEvent *, Client *);
static void cm_handle_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_XEMBED(XClientMessageEvent *, Client *);

typedef void (*pc_handler_t) (XPropertyEvent *, Client *);
typedef void (*cm_handler_t) (XClientMessageEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	pc_handler_t pc_handler;
	cm_handler_t cm_handler;
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,	NULL,					&cm_handle_KDE_WM_CHANGE_STATE,		None			},
	{ "MANAGER",				&_XA_MANAGER,			NULL,					&cm_handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,		&pc_handle_MOTIF_WM_INFO,		NULL,					None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,		&pc_handle_NET_ACTIVE_WINDOW,		&cm_handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,		&pc_handle_NET_CLIENT_LIST,		NULL,					None			},
	{ "_NET_CLIENT_LIST_STACKING",		&_XA_NET_CLIENT_LIST_STACKING,	&pc_handle_NET_CLIENT_LIST_STACKING,	NULL,					None			},
	{ "_NET_CLOSE_WINDOW",			&_XA_NET_CLOSE_WINDOW,		NULL,					&cm_handle_NET_CLOSE_WINDOW,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,	NULL,					NULL,					None			},
	{ "_NET_MOVERESIZE_WINDOW",		&_XA_NET_MOVERESIZE_WINDOW,	NULL,					&cm_handle_NET_MOVERESIZE_WINDOW,	None			},
	{ "_NET_REQUEST_FRAME_EXTENTS",		&_XA_NET_REQUEST_FRAME_EXTENTS,	NULL,					&cm_handle_NET_REQUEST_FRAME_EXTENTS,	None			},
	{ "_NET_RESTACK_WINDOW",		&_XA_NET_RESTACK_WINDOW,	NULL,					&cm_handle_NET_RESTACK_WINDOW,		None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,	NULL,					&cm_handle_NET_STARTUP_INFO_BEGIN,	None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,		NULL,					&cm_handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,		&pc_handle_NET_SUPPORTED,		NULL,					None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,	&pc_handle_NET_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,		NULL,					NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,	NULL,					NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_WM_FULLSCREEN_MONITORS",	&_XA_NET_WM_FULLSCREEN_MONITORS,&pc_handle_NET_WM_FULLSCREEN_MONITORS,	&cm_handle_NET_WM_FULLSCREEN_MONITORS,	None			},
	{ "_NET_WM_ICON_GEOMETRY",		&_XA_NET_WM_ICON_GEOMETRY,	&pc_handle_NET_WM_ICON_GEOMETRY,	NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ "_NET_WM_MOVERESIZE",			&_XA_NET_WM_MOVERESIZE,		NULL,					&cm_handle_NET_WM_MOVERESIZE,		None			},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE_FOCUSED",		&_XA_NET_WM_STATE_FOCUSED,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE_HIDDEN",		&_XA_NET_WM_STATE_HIDDEN,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "_NET_WM_VISIBLE_ICON_NAME",		&_XA_NET_WM_VISIBLE_ICON_NAME,	&pc_handle_NET_WM_VISIBLE_ICON_NAME,	NULL,					None			},
	{ "_NET_WM_VISIBLE_NAME",		&_XA_NET_WM_VISIBLE_NAME,	&pc_handle_NET_WM_VISIBLE_NAME,		NULL,					None			},
	{ "SM_CLIENT_ID",			&_XA_SM_CLIENT_ID,		&pc_handle_SM_CLIENT_ID,		NULL,					None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,		NULL,					NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,		&pc_handle_TIMESTAMP_PROP,		NULL,					None			},
	{ "_WIN_APP_STATE",			&_XA_WIN_APP_STATE,		&pc_handle_WIN_APP_STATE,		NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,		&pc_handle_WIN_CLIENT_LIST,		NULL,					None			},
	{ "_WIN_CLIENT_MOVING",			&_XA_WIN_CLIENT_MOVING,		&pc_handle_WIN_CLIENT_MOVING,		NULL,					None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,	&pc_handle_WINDOWMAKER_NOTICEBOARD,	NULL,					None			},
	{ "_WIN_FOCUS",				&_XA_WIN_FOCUS,			&pc_handle_WIN_FOCUS,			NULL,					None			},
	{ "_WIN_HINTS",				&_XA_WIN_HINTS,			&pc_handle_WIN_HINTS,			NULL,					None			},
	{ "_WIN_LAYER",				&_XA_WIN_LAYER,			&pc_handle_WIN_LAYER,			&cm_handle_WIN_LAYER,			None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,		&pc_handle_WIN_PROTOCOLS,		NULL,					None			},
	{ "_WIN_STATE",				&_XA_WIN_STATE,			&pc_handle_WIN_STATE,			&cm_handle_WIN_STATE,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,	&pc_handle_WIN_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,		&pc_handle_WIN_WORKSPACE,		&cm_handle_WIN_WORKSPACE,		None			},
	{ "WM_CHANGE_STATE",			&_XA_WM_CHANGE_STATE,		NULL,					&cm_handle_WM_CHANGE_STATE,		None			},
	{ "WM_CLASS",				NULL,				&pc_handle_WM_CLASS,			NULL,					XA_WM_CLASS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_DELETE_WINDOW",			&_XA_WM_DELETE_WINDOW,		NULL,					NULL,					None			},
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_ICON_NAME",			NULL,				&pc_handle_WM_ICON_NAME,		NULL,					XA_WM_ICON_NAME		},
	{ "WM_ICON_SIZE",			NULL,				&pc_handle_WM_ICON_SIZE,		NULL,					XA_WM_ICON_SIZE		},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "WM_NORMAL_HINTS",			NULL,				&pc_handle_WM_NORMAL_HINTS,		NULL,					XA_WM_NORMAL_HINTS	},
	{ "WM_PROTOCOLS",			&_XA_WM_PROTOCOLS,		&pc_handle_WM_PROTOCOLS,		&cm_handle_WM_PROTOCOLS,		None			},
	{ "WM_SAVE_YOURSELF",			&_XA_WM_SAVE_YOURSELF,		NULL,					NULL,					None			},
	{ "WM_SIZE_HINTS",			NULL,				&pc_handle_WM_SIZE_HINTS,		NULL,					XA_WM_SIZE_HINTS	},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "WM_TRANSIENT_FOR",			NULL,				&pc_handle_WM_TRANSIENT_FOR,		NULL,					XA_WM_TRANSIENT_FOR	},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "WM_ZOOM_HINTS",			NULL,				&pc_handle_WM_ZOOM_HINTS,		NULL,					XA_WM_ZOOM_HINTS	},
	{ "_XEMBED_INFO",			&_XA_XEMBED_INFO,		&pc_handle_XEMBED_INFO,			NULL,					None			},
	{ "_XEMBED",				&_XA_XEMBED,			NULL,					&cm_handle_XEMBED,			None			},
	{ "UTF8_STRING",			&_XA_UTF8_STRING,		NULL,					NULL,					None			},
	{ "_NET_SYSTEM_TRAY_ORIENTATION",	&_XA_NET_SYSTEM_TRAY_ORIENTATION,&pc_handle_NET_SYSTEM_TRAY_ORIENTATION,NULL,					None			},
	{ "_NET_SYSTEM_TRAY_VISUAL",		&_XA_NET_SYSTEM_TRAY_VISUAL,	&pc_handle_NET_SYSTEM_TRAY_VISUAL,	NULL,					None			},
	{ "_NET_DESKTOP_LAYOUT",		&_XA_NET_DESKTOP_LAYOUT,	&pc_handle_NET_DESKTOP_LAYOUT,		NULL,					None			},
	{ "_XDG_ASSIST_CMD_QUIT",		&_XA_XDG_ASSIST_CMD_QUIT,	NULL,					NULL,					None			},
	{ "_XDG_ASSIST_CMD_REPLACE",		&_XA_XDG_ASSIST_CMD_REPLACE,	NULL,					NULL,					None			},
	{ "_XDG_ASSIST_CMD",			&_XA_XDG_ASSIST_CMD,		NULL,					NULL,					None			},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

/* These are listed in the order that we want them evaluated. */

struct atoms wmprops[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_TRANSIENT_FOR",			NULL,				&pc_handle_WM_TRANSIENT_FOR,		NULL,					XA_WM_TRANSIENT_FOR	},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ "WM_CLASS",				NULL,				&pc_handle_WM_CLASS,			NULL,					XA_WM_CLASS		},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

XContext PropertyContext;		/* atom to property handler context */
XContext ClientMessageContext;		/* atom to client message handler context */
XContext ScreenContext;			/* window to screen context */
XContext ClientContext;			/* window to client context */
XContext MessageContext;		/* window to message context */

void
intern_atoms()
{
	int i, j, n;
	char **atom_names;
	Atom *atom_values;
	struct atoms *atom;

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
	for (atom = wmprops; atom->name; atom++)
		if (atom->atom)
			atom->value = *(atom->atom);
	for (atom = atoms; atom->name; atom++) {
		if (atom->pc_handler)
			XSaveContext(dpy, atom->value, PropertyContext,
				     (XPointer) atom->pc_handler);
		if (atom->cm_handler)
			XSaveContext(dpy, atom->value, ClientMessageContext,
				     (XPointer) atom->cm_handler);
	}
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

Bool
init_display()
{
	if (!dpy) {
		int s;
		char sel[64] = { 0, };

		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		XSetErrorHandler(handler);

		PropertyContext = XUniqueContext();
		ClientMessageContext = XUniqueContext();
		ScreenContext = XUniqueContext();
		ClientContext = XUniqueContext();
		MessageContext = XUniqueContext();

		intern_atoms();

		nscr = ScreenCount(dpy);
		if (!(screens = calloc(nscr, sizeof(*screens)))) {
			EPRINTF("no memory\n");
			exit(EXIT_FAILURE);
		}
		for (s = 0, scr = screens; s < nscr; s++, scr++) {
			scr->screen = s;
			scr->root = RootWindow(dpy, s);
			XSaveContext(dpy, scr->root, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, scr->root,
				     ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | SubstructureNotifyMask |
				     FocusChangeMask | PropertyChangeMask);
			snprintf(sel, sizeof(sel), "WM_S%d", s);
			scr->icccm_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_SYSTEM_TRAY_S%d", s);
			scr->stray_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_DESKTOP_LAYOUT_S%d", s);
			scr->pager_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_WM_CM_S%d", s);
			scr->compm_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_XDG_ASSIST_S%d", s);
			scr->slctn_atom = XInternAtom(dpy, sel, False);
		}
		s = DefaultScreen(dpy);
		scr = screens + s;
	}
	return (dpy ? True : False);
}

static char *
get_text(Window win, Atom prop)
{
	XTextProperty tp = { NULL, };

	XGetTextProperty(dpy, win, &tp, prop);
	return (char *) tp.value;
}

static long *
get_cardinals(Window win, Atom prop, Atom type, long *n)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	long *data = NULL;

      try_harder:
	if (XGetWindowProperty
	    (dpy, win, prop, 0L, num, False, type, &real, &format, &nitems, &after,
	     (unsigned char **) &data) == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			if (data) {
				XFree(data);
				data = NULL;
			}
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

	if (XGetWindowProperty
	    (d, win, _XA_WM_STATE, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &wms) == Success && format != 0) {
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

	if (XGetWindowProperty
	    (d, win, _XA_XEMBED_INFO, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &xei) == Success && format != 0) {
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

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
			if (data) {
				XFree(data);
				data = NULL;
			}
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty
		    (dpy, check, atom, 0L, 1L, False, type, &real, &format, &nitems,
		     &after, (unsigned char **) &data) == Success && format != 0) {
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
			if (data)
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

	OPRINTF(1, "non-recursive check for atom 0x%lx\n", atom);

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
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

	OPRINTF(1, "check for non-compliant NetWM\n");

      try_harder:
	if (XGetWindowProperty(dpy, scr->root, protocols, 0L, num, False, XA_ATOM, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success && format) {
		if (after) {
			num += ((after + 1) >> 2);
			if (data) {
				XFree(data);
				data = NULL;
			}
			DPRINTF(1, "trying harder with num = %lu\n", num);
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

/** @brief Check for a non-compliant EWMH/NetWM window manager.
  *
  * There are quite a few window managers that implement part of the EWMH/NetWM
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
		scr->netwm_check =
		    check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
	} while (i++ < 2 && !scr->netwm_check);

	if (scr->netwm_check && scr->netwm_check != scr->root) {
		XSaveContext(dpy, scr->netwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->netwm_check,
			     PropertyChangeMask | StructureNotifyMask);
	} else
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

	if (scr->winwm_check && scr->winwm_check != scr->root) {
		XSaveContext(dpy, scr->winwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->winwm_check,
			     PropertyChangeMask | StructureNotifyMask);
	} else
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
		scr->maker_check =
		    check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW);
	} while (i++ < 2 && !scr->maker_check);

	if (scr->maker_check && scr->maker_check != scr->root) {
		XSaveContext(dpy, scr->maker_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->maker_check,
			     PropertyChangeMask | StructureNotifyMask);
	}
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
	if (scr->motif_check && scr->motif_check != scr->root) {
		XSaveContext(dpy, scr->motif_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->motif_check,
			     PropertyChangeMask | StructureNotifyMask);
	}
	return scr->motif_check;
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	scr->icccm_check = XGetSelectionOwner(dpy, scr->icccm_atom);

	if (scr->icccm_check && scr->icccm_check != scr->root) {
		XSaveContext(dpy, scr->icccm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->icccm_check,
			     PropertyChangeMask | StructureNotifyMask);
	}
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

	OPRINTF(1, "checking direction for screen %d\n", scr->screen);

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

	OPRINTF(1, "checking wm compliance for screen %d\n", scr->screen);

	OPRINTF(1, "checking redirection\n");
	if (check_redir()) {
		have_wm = True;
		OPRINTF(1, "redirection on window 0x%lx\n", scr->redir_check);
	}
	OPRINTF(1, "checking ICCCM 2.0 compliance\n");
	if (check_icccm()) {
		have_wm = True;
		OPRINTF(1, "ICCCM 2.0 window 0x%lx\n", scr->icccm_check);
	}
	OPRINTF(1, "checking OSF/Motif compliance\n");
	if (check_motif()) {
		have_wm = True;
		OPRINTF(1, "OSF/Motif window 0x%lx\n", scr->motif_check);
	}
	OPRINTF(1, "checking WindowMaker compliance\n");
	if (check_maker()) {
		have_wm = True;
		OPRINTF(1, "WindowMaker window 0x%lx\n", scr->maker_check);
	}
	OPRINTF(1, "checking GNOME/WMH compliance\n");
	if (check_winwm()) {
		have_wm = True;
		OPRINTF(1, "GNOME/WMH window 0x%lx\n", scr->winwm_check);
	}
	OPRINTF(1, "checking NetWM/EWMH compliance\n");
	if (check_netwm()) {
		have_wm = True;
		OPRINTF(1, "NetWM/EWMH window 0x%lx\n", scr->netwm_check);
	}

	return have_wm;
}

/** @brief Check for a system tray.
  */
static Window
check_stray()
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->stray_atom))) {
		if (win != scr->stray_owner) {
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "system tray changed from 0x%08lx to 0x%08lx\n", scr->stray_owner, win);
			scr->stray_owner = win;
		}
	} else if (scr->stray_owner) {
		DPRINTF(1, "system tray removed from 0x%08lx\n", scr->stray_owner);
		scr->stray_owner = None;
	}
	return scr->stray_owner;
}

static Window
check_pager()
{
	Window win;
	long *cards, n = 0;

	if ((win = XGetSelectionOwner(dpy, scr->pager_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	/* selection only held while setting _NET_DESKTOP_LAYOUT */
	if (!win && (cards = get_cardinals(scr->root, _XA_NET_DESKTOP_LAYOUT, XA_CARDINAL, &n))
	    && n >= 4) {
		XFree(cards);
		win = scr->root;
	}
	if (win && win != scr->pager_owner)
		DPRINTF(1, "desktop pager changed from 0x%08lx to 0x%08lx\n", scr->pager_owner, win);
	if (!win && scr->pager_owner)
		DPRINTF(1, "desktop pager removed from 0x%08lx\n", scr->pager_owner);
	return (scr->pager_owner = win);
}

static Window
check_compm()
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->compm_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != scr->compm_owner)
		DPRINTF(1, "composite manager changed from 0x%08lx to 0x%08lx\n", scr->compm_owner, win);
	if (!win && scr->compm_owner)
		DPRINTF(1, "composite manager removed from 0x%08lx\n", scr->compm_owner);
	return (scr->compm_owner = win);
}

static void
handle_wmchange()
{
	if (!check_window_manager())
		check_window_manager();
}

static void
pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_MOTIF_WM_INFO(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

Bool
set_screen_of_root(Window sroot)
{
	int s;

	DPRINTF(1, "searching for screen with root window 0x%lx\n", sroot);
	for (s = 0; s < nscr; s++) {
		DPRINTF(2, "comparing 0x%lx and 0x%lx\n", sroot, screens[s].root);
		if (screens[s].root == sroot) {
			scr = screens + s;
			DPRINTF(2, "found screen %d\n", s);
			return True;
		}
	}
	EPRINTF("could not find screen for root 0x%lx!\n", sroot);
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
	Window frame = win, fparent, froot = None, *fchildren = NULL, *vroots, vroot = None;
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
	Window wroot = 0, dw = 0, *dwp = NULL;
	unsigned int du = 0;

	if (!XQueryTree(dpy, w, &wroot, &dw, &dwp, &du)) {
		DPRINTF(1, "could not query tree for window 0x%lx\n", w);
		return False;
	}
	if (dwp)
		XFree(dwp);

	DPRINTF(1, "window 0x%lx has root 0x%lx\n", w, wroot);
	return set_screen_of_root(wroot);
}

Bool
find_screen(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ScreenContext, (XPointer *) &scr))
		return True;
	DPRINTF(1, "no ScreenContext for window 0x%lx\n", w);
	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = screens + c->screen;
		return True;
	}
	DPRINTF(1, "no ClientContext for window 0x%lx\n", w);
	return find_window_screen(w);
}

Client *
find_client_noscreen(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c))
		scr = screens + c->screen;
	return (c);
}

Client *
find_client(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = screens + c->screen;
		return (c);
	}
	find_screen(w);
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
		DPRINTF(1, "Failed NetWM check!\n");
		return True;
	}
	if (!check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
		DPRINTF(1, "_NET_STARTUP_ID not supported\n");
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
	char *p;

	DPRINTF(1, "Message to 0x%lx is: '%s'\n", scr->root, msg);

	xev.type = ClientMessage;
	xev.xclient.message_type = _XA_NET_STARTUP_INFO_BEGIN;
	xev.xclient.display = dpy;
	xev.xclient.window = scr->selwin;
	xev.xclient.format = 8;

	l = strlen((p = msg)) + 1;

	while (l > 0) {
		strncpy(xev.xclient.data.b, p, 20);
		p += 20;
		l -= 20;
		/* just PropertyChange mask in the spec doesn't work :( */
		if (!XSendEvent(dpy, scr->root, False, StructureNotifyMask |
				SubstructureNotifyMask | SubstructureRedirectMask |
				PropertyChangeMask, &xev))
			EPRINTF("XSendEvent: failed!\n");
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}
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

/** @brief send a 'new' startup notification message
  * @param seq - sequence context for which to send new message
  *
  * We do not really use this in this program...
  */
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

/** @brief send a 'change' startup notification message
  * @param seq - sequence context for which to send change message
  *
  * We do not really use this in this program...  However, we could use it to
  * update information determined about the client (if a client is associated)
  * before closing or waiting for the closure of the sequence.
  */
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

/** @brief send a 'remove' startup notification message
  * @param seq - sequence context for which to send remove message
  *
  * We only need the ID= field to send a remove message.  We do this to close a
  * sequence that is awaiting the mapping of a window.
  */
static void
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

/** @brief - get a proc file and read it into a buffer
  * @param pid - process id for which to get the proc file
  * @param name - name of the proc file
  * @param size - return the size of the buffer
  * @return char* - the buffer or NULL if it does not exist or error
  *
  * Used to get a proc file for a pid by name and read the entire file into a
  * buffer.  Returns the buffer and the size of the buffer read.
  */
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

/** @brief get a process' startup id
  * @param pid - pid of the process
  * @return char* - startup id or NULL
  *
  * Gets the DESKTOP_STARTUP_ID environment variable for a given process from
  * the environment file for the corresponding pid.  This only works when the
  * pid is for a process on the same host as we are.
  *
  * /proc/%d/environ contains the evironment for the process.  The entries are
  * separated by null bytes ('\0'), and there may be a null byte at the end.
  */
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

/** @brief get the command of a process
  * @param pid - pid of the process
  * @return char* - the command or NULL
  *
  * Obtains the command line of a process.
  */
char *
get_proc_comm(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "comm", &size);
}

/** @brief get the executable of a process
  * @param pid - pid of the process
  * @return char* - the executable path or NULL
  *
  * Obtains the executable path (binary file) executed by a process, or NULL if
  * there is no executable for the specified pid.  This uses the /proc/%d/exe
  * symbolic link to the executable file.  The returned pointer belongs to the
  * caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/exec is a symblolic link containing the actual pathname of the
  * executed command.  This symbolic link can be dereference normally;
  * attempting to open it will open the executable.  You can even execute the
  * file to run another copy of the same executable as is being run by the
  * process.  In a multithreaded process, the contents of this symblic link are
  * not available if the main thread has already terminated.
  */
char *
get_proc_exec(pid_t pid)
{
	struct stat st;
	char *file, *buf;
	ssize_t size;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	snprintf(file, PATH_MAX, "/proc/%d/exe", pid);
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

/** @brief get the first argument of the command line for a process
  * @param pid - pid of the process
  * @return char* - the command line
  *
  * Obtains the arguments of the command line (argv) from the /proc/%d/cmdline
  * file.  This file contains a list of null-terminated strings.  The pointer
  * returned points to the first argument in the list.  The returned pointer
  * belongs to the caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/cmdline holds the complete command line for the process, unless the
  * process is a zombie.  In the later case there is nothing in this file: that
  * is, a readon on this file will return 0 characters.  The command-line
  * arguments appear in this file as a set of null ('\0') terminated strings.
  */
char *
get_proc_argv0(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "cmdline", &size);
}

/** @brief test a client against a startup notification sequence
  * @param c - client to test
  * @param seq - sequence against which to test
  * @return Bool - True if the client matches the sequence, False otherwise
  */
Bool
test_client(Client *c, Sequence *seq)
{
	pid_t pid;
	char *str;

	CPRINTF(1, c, "seq '%s': testing\n", seq->f.id);

	if (c->startup_id) {
		CPRINTF(1, c, "comparing '%s' and '%s'\n", c->startup_id, seq->f.id);
		if (!strcmp(c->startup_id, seq->f.id))
			return True;
		return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcmp(seq->f.hostname, c->hostname)))
		pid = 0;
	if (pid && (str = get_proc_startup_id(pid))) {
		CPRINTF(1, c, "comparing '%s' and '%s'\n", str, seq->f.id);
		if (strcmp(seq->f.id, str)) {
			free(str);
			return False;
		}
		free(str);
		return True;
	}
	/* correct wmclass */
	if (seq->f.wmclass) {
		CPRINTF(1, c, "comparing '%s' and '%s'\n", c->ch.res_name, seq->f.wmclass);
		if (c->ch.res_name && !strcmp(seq->f.wmclass, c->ch.res_name))
			return True;
		CPRINTF(1, c, "comparing '%s' and '%s'\n", c->ch.res_class, seq->f.wmclass);
		if (c->ch.res_class && !strcmp(seq->f.wmclass, c->ch.res_class))
			return True;
	}
	CPRINTF(1, c, "comparing [%d] and [%d]\n", (int) pid, (int) seq->n.pid);
	/* same process id */
	if (pid && seq->n.pid == pid)
		return True;
	CPRINTF(1, c, "comparing {%lu} and {%lu}\n", (ulong) c->user_time, (ulong) seq->n.timestamp);
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == seq->n.timestamp)
		return True;
	/* correct executable */
	if (pid && seq->f.bin) {
		if ((str = get_proc_comm(pid)))
			CPRINTF(1, c, "comparing '%s' and '%s'\n", str, seq->f.bin);
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			CPRINTF(1, c, "comparing '%s' and '%s'\n", str, seq->f.bin);
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			CPRINTF(1, c, "comparing '%s' and '%s'\n", str, seq->f.bin);
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

/** @brief assist some clients by adding information missing from window
  *
  * Some applications do not use a full toolkit and do not properly set all of
  * the EWMH properties.  Once we have identified the startup sequence
  * associated with a client, we can set infomration on the client from the
  * startup notification sequence.
  *
  * Also do things like use /proc/[pid]/cmdline to set up WM_COMMAND if not
  * present.
  */
void
setup_client(Client *c)
{
	Sequence *seq;
	long data;

	if (!(seq = c->seq))
		return;

	if (!options.assist)
		return;
	/* set up _NET_STARTUP_ID */
	if (!c->startup_id && seq->f.id) {
		CPRINTF(1, c, "setting _NET_STARTUP_ID to '%s'\n", seq->f.id);
		XChangeProperty(dpy, c->win, _XA_NET_STARTUP_ID, _XA_UTF8_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.id,
				strlen(seq->f.id));
	}

	/* set up _NET_WM_PID */
	if (!c->pid && seq->n.pid) {
		CPRINTF(1, c, "setting _NET_WM_PID to [%d]\n", seq->n.pid);
		data = seq->n.pid;
		XChangeProperty(dpy, c->win, _XA_NET_WM_PID, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
		c->pid = seq->n.pid;
	}

	/* set up WM_CLIENT_MACHINE */
	if (!c->hostname && seq->f.hostname) {
		CPRINTF(1, c, "setting WM_CLIENT_MACHINE to '%s'\n", seq->f.hostname);
		XChangeProperty(dpy, c->win, XA_WM_CLIENT_MACHINE, XA_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.hostname,
				strlen(seq->f.hostname));
	}

	/* set up WM_COMMAND */
	if (c->pid && !c->command) {
		char *string;
		char buf[65] = { 0, };
		size_t size;

		gethostname(buf, sizeof(buf) - 1);
		if ((c->hostname && !strcmp(buf, c->hostname)) ||
		    (seq->f.hostname && !strcmp(buf, seq->f.hostname))) {
			if ((string = get_proc_file(c->pid, "cmdline", &size))) {
				CPRINTF(1, c, "setting WM_COMMAND to '%s'\n", string);
				XChangeProperty(dpy, c->win, XA_WM_COMMAND,
						XA_STRING, 8, PropModeReplace,
						(unsigned char *) string, size);
				free(string);
			}
		}
	}
}

static Sequence *ref_sequence(Sequence *seq);

/** @brief find the startup sequence for a client
  * @param c - client to lookup startup sequence for
  * @return Sequence* - the found sequence or NULL if not found
  */
static Sequence *
find_startup_seq(Client *c)
{
	Sequence *seq;

	if ((seq = c->seq)) {
		DPRINTF(1, "sequence already found\n");
		return seq;
	}

	/* search by startup id */
	if (c->startup_id) {
		DPRINTF(1, "searching for startup id %s\n", c->startup_id);
		for (seq = scr->sequences; seq; seq = seq->next) {
			if (!seq->f.id) {
				EPRINTF("sequence with null id!\n");
				continue;
			}
			DPRINTF(2, "comparing '%s' and '%s'\n", c->startup_id, seq->f.id);
			if (!strcmp(c->startup_id, seq->f.id))
				break;
		}
		if (!seq) {
			DPRINTF(1, "cannot find startup id '%s'!\n", c->startup_id);
			return (seq);
		}
		DPRINTF(1, "Found sequence by _NET_STARTUP_ID\n");
		goto found_it;
	} else
		DPRINTF(1, "no startup id for search\n");

	/* search by wmclass */
	if (c->ch.res_name || c->ch.res_class) {
		DPRINTF(1, "searching for (%s,%s)\n", c->ch.res_name, c->ch.res_class);
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				DPRINTF(2, "comparing '%s' and '%s'\n", c->ch.res_name, wmclass);
				if (c->ch.res_name && !strcmp(wmclass, c->ch.res_name))
					break;
				DPRINTF(2, "comparing '%s' and '%s'\n", c->ch.res_class, wmclass);
				if (c->ch.res_class && !strcmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF(1, "Found sequence by res_name or res_class\n");
			goto found_it;
		}
		DPRINTF(1, "did not find sequence by res_name or res_class\n");
	} else
		DPRINTF(1, "no res_name or res_class for search\n");

	/* search by binary */
	if (c->command && c->command[0]) {
		DPRINTF(1, "searching for command %s\n", c->command[0]);
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *binary;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((binary = seq->f.bin)) {
				DPRINTF(2, "comparing '%s' and '%s'\n", c->command[0], binary);
				if (!strcmp(binary, c->command[0]))
					break;
			}

		}
		if (seq) {
			DPRINTF(1, "Found sequence by command\n");
			goto found_it;
		}
		DPRINTF(1, "did not find sequence by command\n");
	} else
		DPRINTF(1, "no command for search\n");

	/* search by wmclass (this time case insensitive) */
	if (c->ch.res_name || c->ch.res_class) {
		DPRINTF(1, "searching for (%s,%s)\n", c->ch.res_name, c->ch.res_class);
		for (seq = scr->sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				DPRINTF(2, "comparing '%s' and '%s'\n", c->ch.res_name, wmclass);
				if (c->ch.res_name
				    && !strcasecmp(wmclass, c->ch.res_name))
					break;
				DPRINTF(2, "comparing '%s' and '%s'\n", c->ch.res_class, wmclass);
				if (c->ch.res_class
				    && !strcasecmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF(1, "Found sequence by res_name or res_class\n");
			goto found_it;
		}
		DPRINTF(1, "did not find sequence by res_name or res_class\n");
	} else
		DPRINTF(1, "no res_name or res_class for search\n");

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
		if (seq) {
			DPRINTF(1, "Found sequence by pid and hostname\n");
			goto found_it;
		}
	}
	DPRINTF(1, "could not find startup ID for client\n");
	return NULL;
      found_it:
	seq->client = c;
	c->seq = ref_sequence(seq);
	setup_client(c);
	return (seq);
}

/** @brief detect whether a client is a dockapp
  * @param c - client the check
  * @return Bool - true if the client is a dockapp; false, otherwise.
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
#if 1
	Sequence *seq;
#if 1
	struct atoms *atom;
#endif

	XSaveContext(dpy, c->win, ScreenContext, (XPointer) scr);
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask |
		     StructureNotifyMask | SubstructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);

#if 1
	for (atom = wmprops; atom->name; atom++) {
		if (atom->pc_handler) {
			CPRINTF(1, c, "updating %s\n", atom->name);
			atom->pc_handler(NULL, c);
		}
	}

	if (!c->seq) {
		for (seq = scr->sequences; seq; seq = seq->next) {
			if (test_client(c, seq)) {
				c->seq = ref_sequence(seq);
				setup_client(c);
			}
		}
	}
#endif
	c->new = False;
#else
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
#if 0
	if (test_client(c))
		setup_client(c);
#endif
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);
	if (c->group) {
		XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->group,
			     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
			     FocusChangeMask | PropertyChangeMask);
	}
	c->new = False;
#endif
}

static Client *
add_client(Window win)
{
	Client *c;

	PTRACE(5);
	c = calloc(1, sizeof(*c));
	c->win = win;
	c->next = scr->clients;
	scr->clients = c;
	update_client(c);
	c->new = True;
	return (c);
}

static Sequence *unref_sequence(Sequence *seq);

static void
remove_client(Client *c)
{
	Window *winp;
	int i;

	PTRACE(5);
	if (c->startup_id) {
		DPRINTF(1, "Freeing: startup_id\n");
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (c->command) {
		DPRINTF(1, "Freeing: command\n");
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (c->name) {
		DPRINTF(1, "Freeing: name\n");
		XFree(c->name);
		c->name = NULL;
	}
	if (c->icon_name) {
		DPRINTF(1, "Freeing: icon_name\n");
		XFree(c->icon_name);
		c->icon_name = NULL;
	}
	if (c->hostname) {
		DPRINTF(1, "Freeing: hostname\n");
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (c->client_id) {
		DPRINTF(1, "Freeing: client_id\n");
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (c->role) {
		DPRINTF(1, "Freeing: role\n");
		XFree(c->role);
		c->role = NULL;
	}
	if (c->ch.res_name) {
		DPRINTF(1, "Freeing: ch.res_name\n");
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		DPRINTF(1, "Freeing: ch.res_class\n");
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (c->wmh) {
		DPRINTF(1, "Freeing: wmh\n");
		XFree(c->wmh);
		c->wmh = NULL;
	}
	if (c->wms) {
		DPRINTF(1, "Freeing: wms\n");
		XFree(c->wms);
		c->wms = NULL;
	}
	if (c->xei) {
		DPRINTF(1, "Freeing: xei\n");
		XFree(c->xei);
		c->xei = NULL;
	}
	for (i = 0, winp = &c->win; i < 5; i++, winp++) {
		if (*winp) {
			XDeleteContext(dpy, *winp, ScreenContext);
			XDeleteContext(dpy, *winp, ClientContext);
			XSelectInput(dpy, *winp, NoEventMask);
			*winp = None;
		}
	}
	if (c->seq && c->seq->client == c) {
		c->seq->client = NULL;
		unref_sequence(c->seq);
		c->seq = NULL;
	}
	free(c);
}

void
del_client(Client *r)
{
	Client *c, **cp;

	PTRACE(5);
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
show_sequence(Sequence *seq)
{
	char **label, **field;

	if (options.debug < 2 && options.output < 2)
		return;
	for (label = (char **)StartupNotifyFields, field = seq->fields; *label; label++, field++) {
		if (*field)
			fprintf(stderr, "%s=%s\n", *label, *field);
		else
			fprintf(stderr, "%s=\n", *label);
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
	OPRINTF(1, "Updated sequence fields: %s\n", old->f.id);
	show_sequence(old);
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

static gboolean
persist_timeout_callback(gpointer data)
{
	DPRINTF(1, "removing status icon or notification\n");
	g_object_unref(G_OBJECT(data));
	return FALSE;	/* remove timeout source */
}

static void
close_sequence(Sequence *seq)
{
#ifdef HAVE_GLIB_EVENT_LOOP
	if (seq->timer) {
		DPRINTF(1, "removing timer\n");
		g_source_remove(seq->timer);
		seq->timer = 0;
	}
#endif				/* HAVE_GLIB_EVENT_LOOP */
#ifdef SYSTEM_TRAY_STATUS_ICON
	if (seq->status) {
#if 1
		g_timeout_add(options.persist, persist_timeout_callback, seq->status);
#else
		DPRINTF(1, "removing statusicon\n");
		g_object_unref(G_OBJECT(seq->status));
#endif
		seq->status = NULL;
	}
#endif				/* SYSTEM_TRAY_STATUS_ICON */
#ifdef DESKTOP_NOTIFICATIONS
	if (seq->notification) {
#if 1
		g_timeout_add(options.persist, persist_timeout_callback, seq->notification);
#else
		DPRINTF(1, "removing notificiation\n");
		g_object_unref(G_OBJECT(seq->notification));
#endif
		seq->notification = NULL;
	}
#endif				/* DESKTOP_NOTIFICATIONS */
}

static Sequence *
unref_sequence(Sequence *seq)
{
	if (seq) {
		if (--seq->refs <= 0) {
			Sequence *s, **prev;

			DPRINTF(1, "deleting sequence\n");
			for (prev = &scr->sequences, s = *prev; s && s != seq;
			     prev = &s->next, s = *prev) ;
			if (s) {
				*prev = s->next;
				s->next = NULL;
			}
			close_sequence(seq);
			free_sequence_fields(seq);
			free(seq);
			return (NULL);
		} else
			DPRINTF(1, "sequence still has %d references\n", seq->refs);
	} else
		EPRINTF("called with null pointer\n");
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

	OPRINTF(1, "Removing sequence: %s\n", del->f.id);
	show_sequence(del);
	for (prev = &scr->sequences, seq = *prev; seq && seq != del;
	     prev = &seq->next, seq = *prev) ;
	if (seq) {
		*prev = seq->next;
		seq->next = NULL;
		close_sequence(seq);
		unref_sequence(seq);
	} else
		DPRINTF(1, "could not find sequence\n");
	return (seq);
}

#ifdef DESKTOP_NOTIFICATIONS
static NotifyNotification *create_notification(Sequence *seq);
#endif				/* DESKTOP_NOTIFICATIONS */

#ifdef HAVE_GLIB_EVENT_LOOP

static gboolean
sequence_timeout_callback(gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	if (seq->state == StartupNotifyComplete) {
		remove_sequence(seq);
		seq->timer = 0;
		return FALSE;	/* stop timeout interval */
	}
	/* for now, just generate a remove message after the guard time */
	if (1) {
		send_remove(seq);
		seq->timer = 0;
		return FALSE;	/* remove timeout source */
	} else {
#ifdef DESKTOP_NOTIFICATIONS
#if 0
		create_notification(seq);
#else
		(void) create_notification;
#endif
#endif				/* DESKTOP_NOTIFICATIONS */
		return TRUE;	/* continue timeout interval */
	}
}

#endif				/* HAVE_GLIB_EVENT_LOOP */

#ifdef SYSTEM_TRAY_STATUS_ICON

static GtkStatusIcon *create_statusicon(Sequence *seq);

#endif				/* SYSTEM_TRAY_STATUS_ICON */

/** @brief add a new startup notification sequence to list for screen
  *
  * When a startup notification sequence is added, it is because we have
  * received a 'new:' or 'remove:' message.  What we want to do is to generate a
  * status icon at this point that will be updated and removed as the startup
  * notification sequence is changed or completed.  We add StartupNotifyComplete
  * sequences that start with a 'remove:' message in case the 'new:' message is
  * late.  We always add a guard timer so that the StartupNotifyComplete
  * sequence will always be removed at some point.
  */
static void
add_sequence(Sequence *seq)
{
	seq->refs = 1;
	seq->client = NULL;
	seq->next = scr->sequences;
	scr->sequences = seq;
	if (seq->state == StartupNotifyNew) {
#ifdef SYSTEM_TRAY_STATUS_ICON
#if 1
		if (options.feedback)
			create_statusicon(seq);
#else
		(void)create_statusicon;
#endif
#endif				/* SYSTEM_TRAY_STATUS_ICON */
	}
#if 1
#ifdef HAVE_GLIB_EVENT_LOOP
	if (options.feedback)
		seq->timer =
		    g_timeout_add(options.guard, sequence_timeout_callback, (gpointer) seq);
#endif				/* HAVE_GLIB_EVENT_LOOP */
#else
	(void) sequence_timeout_callback;
#endif
	OPRINTF(1, "Added sequence: %s\n", seq->f.id);
	show_sequence(seq);
}

static void
process_startup_msg(Message *m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q, *copy, *b;
	const char **f;
	int i;
	int escaped, quoted;

	DPRINTF(1, "Got message from 0x%lx: %s\n", m->origin, m->data);
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
			DPRINTF(1, "mangled message\n");
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
							DPRINTF(1, "mangled message\n");
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
		DPRINTF(1, "message with no ID= field\n");
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
		switch (cmd.state) {
		default:
			free_sequence_fields(&cmd);
			DPRINTF(1, "message out of sequence\n");
			return;
		case StartupNotifyNew:
		case StartupNotifyComplete:
			break;
		}
		if (!(seq = calloc(1, sizeof(*seq)))) {
			free_sequence_fields(&cmd);
			DPRINTF(1, "no memory\n");
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
			DPRINTF(1, "message state error\n");
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
			DPRINTF(1, "message state error\n");
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
			DPRINTF(1, "message state error\n");
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
cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	PTRACE(5);
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m) {
		m = calloc(1, sizeof(*m));
		XSaveContext(dpy, from, MessageContext, (XPointer) m);
	}
	free(m->data);
	m->origin = from;
	m->data = calloc(21, sizeof(*m->data));
	m->len = 0;
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

static void
cm_handle_NET_STARTUP_INFO(XClientMessageEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	PTRACE(5);
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m)
		return;
	m->data = realloc(m->data, m->len + 21);
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
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

/** @brief handle a client list of windows
  * @param e - PropertyNotify event that trigger the change
  * @param atom - the client list atom
  * @param type - the type of the list (XA_WINDOW or XA_CARDINAL)
  *
  * Process a changed client list.  Basically any client window that is on this
  * list is managed if it was not previously managed.  Also, any client that was
  * previously on the list is no longer managed when it does not appear on the
  * list any more (unless it is a dockapp or a statusicon).
  */
static void
pc_handle_CLIENT_LIST(XPropertyEvent *e, Atom atom, Atom type)
{
	Window *list;
	long i, n;
	Client *c, *cn;

	PTRACE(5);
	if ((list = get_windows(scr->root, atom, type, &n))) {
		for (c = scr->clients; c;
		     c->breadcrumb = False, c->new = False, c = c->next) ;
		for (i = 0; i < n; i++) {
			if (XFindContext(dpy, list[i], ClientContext, (XPointer *) &c) ==
			    Success) {
				c->breadcrumb = True;
				c->listed = True;
				managed_client(c, e ? e->time : CurrentTime);
			}
		}
		XFree(list);
		for (cn = scr->clients; (c = cn);) {
			cn = c->next;
			if (!c->breadcrumb && c->listed) {
				c->listed = False;
				unmanaged_client(c);
			}
		}
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
pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *e, Client *c)
{
	Window active = None;

	PTRACE(5);
	if (c || !e || e->window != scr->root || e->state == PropertyDelete)
		return;
	if (get_window(scr->root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW, &active) && active) {
		if (XFindContext(dpy, active, ClientContext, (XPointer *) &c) == Success) {
			if (e)
				pushtime(&c->active_time, e->time);
			managed_client(c, e ? e->time : CurrentTime);
		}
	}
}

static void
cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c)
		return;
	pushtime(&current_time, e->data.l[1]);
	pushtime(&c->active_time, e->data.l[1]);
	if (c->managed)
		return;
	DPRINTF(1, "_NET_ACTIVE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, e->data.l[1]);
}

static void
pc_handle_NET_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static void
pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST_STACKING, XA_WINDOW);
}

static void
cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent *e, Client *c)
{
	Time time;

	PTRACE(5);
	if (!c)
		return;
	time = e ? e->data.l[0] : CurrentTime;
	pushtime(&current_time, time);
	pushtime(&c->active_time, time);

	if (c->managed)
		return;
	DPRINTF(1, "_NET_CLOSE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, time);
}

static void
cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	DPRINTF(1, "_NET_MOVERESIZE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	/* This message, unlike others, is sent before a window is initially mapped
	   (managed). */
	if (!c || !c->managed)
		return;
	DPRINTF(1, "_NET_REQUEST_FRAME_EXTENTS for managed window 0x%lx\n", e->window);
}

static void
cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	DPRINTF(1, "_NET_RESTACK_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_STARTUP_ID(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->startup_id) {
		CPRINTF(1, c, "freeing old _NET_STARTUP_ID\n");
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted _NET_STARTUP_ID\n");
		return;
	}
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);
	if (c->startup_id)
		CPRINTF(1, c, "_NET_STARTUP_ID = %s\n", c->startup_id);
	else
		CPRINTF(1, c, "could not retrieve _NET_STARTUP_ID\n");
}

static void
pc_handle_NET_SUPPORTED(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_NET_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->icon_name) {
		XFree(c->icon_name);
		c->icon_name = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!c->icon_name && c->win)
		c->icon_name = get_text(c->win, _XA_NET_WM_ICON_NAME);
	if (!c->icon_name && c->leader)
		c->icon_name = get_text(c->leader, _XA_NET_WM_ICON_NAME);
	if (!c->icon_name && c->group)
		c->icon_name = get_text(c->group, _XA_NET_WM_ICON_NAME);
	if (c->icon_name)
		CPRINTF(1, c, "_NET_WM_ICON_NAME %s\n", c->icon_name);
}

static void
cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c)
		return;
	DPRINTF(1, "_NET_WM_MOVERSIZE for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->name) {
		CPRINTF(1, c, "freeing _NET_WM_NAME\n");
		XFree(c->name);
		c->name = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted _NET_WM_NAME\n");
		return;
	}
	if (!c->name && c->win)
		c->name = get_text(c->win, XA_WM_NAME);
	if (!c->name && c->leader)
		c->name = get_text(c->leader, XA_WM_NAME);
	if (!c->name && c->group)
		c->name = get_text(c->group, XA_WM_NAME);
	if (c->name)
		CPRINTF(1, c, "_NET_WM_NAME %s\n", c->name);
}

static void
pc_handle_NET_WM_PID(XPropertyEvent *e, Client *c)
{
	long pid = 0;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->pid) {
		CPRINTF(1, c, "forgetting _NET_WM_PID\n");
		c->pid = 0;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted _NET_WM_PID\n");
		return;
	}
	if (!pid && c->win)
		get_cardinal(c->win, _XA_NET_WM_PID, AnyPropertyType, &pid);
	if (!pid && c->leader)
		get_cardinal(c->leader, _XA_NET_WM_PID, AnyPropertyType, &pid);
	if (!pid && c->group)
		get_cardinal(c->leader, _XA_NET_WM_PID, AnyPropertyType, &pid);
	if (pid)
		CPRINTF(1, c, "_NET_WM_PID = %ld\n", pid);
	c->pid = pid;
}

/** @brief handle _NET_WM_STATE property change
  *
  * Unlike WM_STATE, it is ok for the client to set this property before mapping
  * a window.  After the window is mapped it must be changd with the
  * _NET_WM_STATE client message.  There are, however, two atoms in the state:
  * _NET_WM_STATE_HIDDEN and _NET_WM_STATE_FOCUSED, that should be treated as
  * read-only by clients and will, therefore, only be set by the window manager.
  * When either of these atoms are set (or removed [TODO]), we should treat the
  * window as managed if it has not already been managed.
  *
  * The window manager is the only one responsible for deleting the property,
  * and WM-SPEC-1.5 says that it should be deleted whenever a window is
  * withdrawn but left in place when the window manager shuts down, is replaced,
  * or restarts.
  */
static void
pc_handle_NET_WM_STATE(XPropertyEvent *e, Client *c)
{
	Atom *atoms = NULL;
	long i, n;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete) {
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	CPRINTF(1, c, "_NET_WM_STATE ");
	if ((atoms = get_atoms(c->win, _XA_NET_WM_STATE, AnyPropertyType, &n))) {
		for (i = 0; i < n; i++) {
			if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
				fprintf(stdout, " %s", "_NET_WM_STATE_FOCUSED");
				if (e)
					pushtime(&c->focus_time, e->time);
				if (!c->managed)
					managed_client(c, e ? e->time : CurrentTime);
			} else if (atoms[i] == _XA_NET_WM_STATE_HIDDEN) {
				fprintf(stdout, " %s", "_NET_WM_STATE_HIDDEN");
				if (!c->managed)
					managed_client(c, e ? e->time : CurrentTime);
			}
		}
		XFree(atoms);
		if (!c->managed)
			managed_client(c, e ? e->time : CurrentTime);
	}
	fprintf(stdout, "\n");
}

/** @brief handle _NET_WM_STATE client message
  *
  * If the client sends _NET_WM_STATE client messages, the client thinks that
  * the window is managed by the window manager because otherwise it would
  * simply set the property itself.  Therefore, this client message indicates
  * that the window should be managed if it has not already.
  *
  * Basically a client message is a client request to alter state while the
  * window is mapped, so mark window managed if it is not already.
  */
static void
cm_handle_NET_WM_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	DPRINTF(1, "_NET_WM_STATE sent for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	CPRINTF(1, c, "forgetting _NET_WM_USER_TIME_WINDOW\n");
	c->time_win = None;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted _NET_WM_USER_TIME_WINDOW\n");
		return;
	}
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		Time time;

		XSaveContext(dpy, c->time_win, ScreenContext, (XPointer) scr);
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);

		if (get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
			pushtime(&c->user_time, time);
			pushtime(&last_user_time, time);
			pushtime(&current_time, time);
		}
		CPRINTF(1, c, "_NET_WM_USER_TIME_WINDOW 0x%lx\n", c->time_win);
	}
	CPRINTF(1, c, "_NET_WM_USER_TIME_WINDOW = 0x%lx\n", c->time_win);
}

static void
pc_handle_NET_WM_USER_TIME(XPropertyEvent *e, Client *c)
{
	Time time;

	PTRACE(5);
	if (!c || (e && e->state == PropertyDelete))
		return;
	if (get_time(c->time_win ? : c->win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
		pushtime(&c->user_time, time);
		pushtime(&last_user_time, time);
		pushtime(&current_time, time);
		CPRINTF(1, c, "_NET_WM_USER_TIME %ld\n", time);
	}
}

/** @brief handle _NET_WM_VISIBLE_ICON_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	PTRACE(5);
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_ICON_NAME)))
		return;
	DPRINTF(1, "_NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

/** @brief handle _NET_WM_VISIBLE_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	PTRACE(5);
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_NAME)))
		return;
	DPRINTF(1, "_NET_WM_VISIBLE_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

static void
pc_handle_WIN_APP_STATE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_WIN_CLIENT_LIST, XA_CARDINAL);
}

static void
pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_FOCUS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_LAYER(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_WIN_LAYER(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_WIN_STATE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete)
		return;
	/* Not like WM_STATE and _NET_WM_STATE */
}

static void
cm_handle_WIN_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_WIN_WORKSPACE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_WIN_WORKSPACE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_SM_CLIENT_ID(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->client_id = get_text(c->win, _XA_SM_CLIENT_ID)) && c->leader)
		c->client_id = get_text(c->leader, _XA_SM_CLIENT_ID);
	if (!(c->client_id) && c->group)
		c->client_id = get_text(c->group, _XA_SM_CLIENT_ID);
}

static void
pc_handle_WM_CLASS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->ch.res_name) {
		CPRINTF(2, c, "freeing existing res_name %s\n", c->ch.res_name);
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		CPRINTF(2, c, "freeing existing res_class %s\n", c->ch.res_class);
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);
	if (!c->ch.res_name)
		CPRINTF(1, c, "no res_name available\n");
	else
		CPRINTF(2, c, "new res_name is %s\n", c->ch.res_name);
	if (!c->ch.res_class)
		CPRINTF(1, c, "no res_class available\n");
	else
		CPRINTF(2, c, "new res_class is %s\n", c->ch.res_class);
	if (c->ch.res_name || c->ch.res_class)
		CPRINTF(1, c, "WM_CLASS '%s','%s'\n", c->ch.res_name, c->ch.res_class);
	if ((c->dockapp = is_dockapp(c)))
		CPRINTF(2, c, "is a dock app\n");
}

static void
cm_handle_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_CLIENT_LEADER(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	CPRINTF(1, c, "forgetting WM_CLIENT_LEADER\n");
	c->leader = None;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "removed WM_CLIENT_LEADER\n");
		return;
	}
	get_window(c->win, _XA_WM_CLIENT_LEADER, AnyPropertyType, &c->leader);
	CPRINTF(1, c, "WM_CLIENT_LEADER = 0x%lx\n", c->leader);
	if (c->leader)
		CPRINTF(1, c, "WM_CLIENT_LEADER 0x%lx\n", c->leader);
}

static void
pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->hostname) {
		CPRINTF(1, c, "freeing old WM_CLIENT_MACHINE\n");
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted WM_CLIENT_MACHINE\n");
		return;
	}
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->leader)
		c->hostname = get_text(c->leader, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->transient_for)
		c->hostname = get_text(c->transient_for, XA_WM_CLIENT_MACHINE);
	CPRINTF(1, c, "WM_CLIENT_MACHINE = '%s'\n", c->hostname);
	if (c->hostname)
		CPRINTF(1, c, "WM_CLIENT_MACHINE %s\n", c->hostname);
}

static void
pc_handle_WM_COMMAND(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	int count = 0, i;

	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->command) {
		CPRINTF(1, c, "freeing old WM_COMMAND\n");
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted WM_COMMAND\n");
		return;
	}
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);
	if (!c->command && c->leader)
		XGetCommand(dpy, c->leader, &c->command, &count);
	if (!c->command && c->transient_for)
		XGetCommand(dpy, c->transient_for, &c->command, &count);
	if (c->command) {
		if (options.output >= 1) {
			CPRINTF(1, c, "WM_COMMAND");
			for (i = 0; i < count; i++)
				fprintf(stdout, " '%s'", c->command[i]);
			fprintf(stdout, "\n");
		}
	}
	if (c->command && c->command[0])
		CPRINTF(1, c, "WM_COMMAND = '%s'\n", c->command[0]);
}

static void
pc_handle_WM_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wmh) {
		CPRINTF(1, c, "freeing old WM_HINTS\n");
		XFree(c->wmh);
		c->wmh = NULL;
		c->group = None;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted WM_HINTS\n");
		return;
	}
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
		if (c->group && c->group != scr->root) {
			XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->group,
				     ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | SubstructureNotifyMask |
				     FocusChangeMask | PropertyChangeMask);
		}
		if (c->group)
			CPRINTF(1, c, "WM_HINTS group 0x%lx\n", c->group);
		if ((c->dockapp = is_dockapp(c)))
			CPRINTF(1, c, "WM_HINTS is dock app\n");
	} else
		CPRINTF(1, c, "cannot retrieve WM_HINTS\n");
}

static void
pc_handle_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_ICON_SIZE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->name) {
		CPRINTF(1, c, "freeing old WM_NAME\n");
		XFree(c->name);
		c->name = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted WM_NAME\n");
		return;
	}
	if (!c->name && c->win)
		c->name = get_text(c->win, XA_WM_NAME);
	if (!c->name && c->leader)
		c->name = get_text(c->leader, XA_WM_NAME);
	if (!c->name && c->group)
		c->name = get_text(c->group, XA_WM_NAME);
	if (c->name)
		CPRINTF(1, c, "WM_NAME %s\n", c->name);
}

static void
pc_handle_WM_NORMAL_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_WM_PROTOCOLS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_SIZE_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_STATE(XPropertyEvent *e, Client *c)
{
	const char *state;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wms) {
		DPRINTF(1, "freeing old WM_STATE\n");
		XFree(c->wms);
		c->wms = NULL;
	}
	if (e && e->state == PropertyDelete) {
		DPRINTF(1, "WM_STATE was removed, unmanaging client\n");
		unmanaged_client(c);
		return;
	}
	if (!(c->wms = XGetWMState(dpy, c->win))) {
		DPRINTF(1, "could not obtain WM_STATE\n");
		/* XXX: about the only time this should happed would be if the window was 
		   destroyed out from under us: check that.  We should get a
		   DestroyNotify soon anyway. */
		return;
	}
	switch (c->wms->state) {
	case WithdrawnState:
		state = "WithdrawnState";
		break;
	case NormalState:
		state = "NormalState";
		break;
	case IconicState:
		state = "IconicState";
		break;
	case ZoomState:
		state = "ZoomState";
		break;
	case InactiveState:
		state = "InactiveState";
		break;
	default:
		state = "UnknownState";
		break;
	}
	CPRINTF(1, c, "WM_STATE is %s\n", state);
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
			   just managed, but only for WindowMaker work-alikes. Otherwise, 
			   per ICCCM, placing withdrawn state on the window means that it 
			   is unmanaged. */
			if ((c->dockapp = is_dockapp(c)) && scr->maker_check) {
				managed_client(c, e ? e->time : CurrentTime);
				break;
			}
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
			managed_client(c, e ? e->time : CurrentTime);
			break;
		}
	}
}

static void
cm_handle_WM_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->transient_for = None;
	if (e && e->state == PropertyDelete)
		return;
	get_window(c->win, XA_WM_TRANSIENT_FOR, AnyPropertyType, &c->transient_for);
}

static void
pc_handle_WM_WINDOW_ROLE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->role) {
		CPRINTF(1, c, "freeing old WM_WINDOW_ROLE\n");
		XFree(c->role);
		c->role = NULL;
	}
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "deleted WM_WINDOW_ROLE\n");
		return;
	}
	c->role = get_text(c->win, _XA_WM_WINDOW_ROLE);
	if (c->role)
		CPRINTF(1, c, "WM_WINDOW_ROLE %s\n", c->role);
}

static void
pc_handle_WM_ZOOM_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
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
	XEMBED_ACCELERATOR_OVERLOADED = (1 << 0),
};

static void
pc_handle_XEMBED_INFO(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	if (e && e->state == PropertyDelete) {
		c->statusicon = False;
		return;
	}
	if ((c->xei = XGetEmbedInfo(dpy, c->win))) {
		c->dockapp = False;
		c->statusicon = True;
		return;
	}

}

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
cm_handle_XEMBED(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || !e || e->type != ClientMessage)
		return;
	pushtime(&current_time, e->data.l[0]);
	if (c->managed)
		return;
	switch (e->data.l[1]) {
	case XEMBED_EMBEDDED_NOTIFY:
		/* Sent from the embedder to the client on embedding, after reparenting
		   and mapping the client's X window.  A client that receives this
		   message knows that its window was embedded by an XEmbed site and not
		   simply reparented by a window manager. The embedder's window handle is 
		   in data1.  data2 is the protocol version in use. */
		managed_client(c, e->data.l[0]);
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
	DPRINTF(1, "_XEMBED message for unmanaged client 0x%lx\n", e->window);
	managed_client(c, e->data.l[0]);
}

/** @brief handle MANAGER client message
  *
  * We are expecting MANAGER messages for two selections: WM_S%d and
  * _NET_SYSTEM_TRAY_S%d which correspond to window manager and system tray,
  * respectively.
  */
static void
cm_handle_MANAGER(XClientMessageEvent *e, Client *c)
{
	Window owner;
	Atom selection;
	Time time;

	PTRACE(5);
	if (!e || e->format != 32)
		return;
	time = e->data.l[0];
	selection = e->data.l[1];
	owner = e->data.l[2];

	(void) time; /* FIXME: use this somehow */

	if (selection == scr->icccm_atom) {
		if (owner && owner != scr->icccm_check) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "window manager changed from 0x%lx to 0x%lx\n",
				scr->icccm_check, owner);
			scr->icccm_check = owner;
			handle_wmchange();
		}
	} else if (selection == scr->stray_atom) {
		if (owner && owner != scr->stray_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "system tray changed from 0x%lx to 0x%lx\n", scr->stray_owner,
				owner);
			scr->stray_owner = owner;
			check_stray();
		}
	} else if (selection == scr->pager_atom) {
		if (owner && owner != scr->pager_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "pager changed from 0x%lx to 0x%lx\n", scr->pager_owner,
				owner);
			scr->pager_owner = owner;
			check_pager();
		}
	} else if (selection == scr->compm_atom) {
		if (owner && owner != scr->compm_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "composite manager changed from 0x%lx to 0x%lx\n", scr->compm_owner,
				owner);
			scr->compm_owner = owner;
			check_compm();
		}
	}
}

static void
pc_handle_TIMESTAMP_PROP(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_NET_SYSTEM_TRAY_ORIENTATION(XPropertyEvent *e, Client *c)
{
	return;
}

static void
pc_handle_NET_SYSTEM_TRAY_VISUAL(XPropertyEvent *e, Client *c)
{
	return;
}

static void
pc_handle_NET_DESKTOP_LAYOUT(XPropertyEvent *e, Client *c)
{
	if (!e || e->type != PropertyNotify)
		return;
	switch (e->state) {
	case PropertyNewValue:
		if (!scr->pager_owner)
			scr->pager_owner = scr->root;
		break;
	case PropertyDelete:
		if (scr->pager_owner &&  scr->pager_owner == scr->root)
			scr->pager_owner = None;
		break;
	}
	return;
}

void
pc_handle_atom(XPropertyEvent *e, Client *c)
{
	pc_handler_t handle = NULL;

	if (XFindContext(dpy, e->atom, PropertyContext, (XPointer *) &handle) == Success)
		(*handle) (e, c);
	else
		DPRINTF(1, "no PropertyNotify handler for %s\n", XGetAtomName(dpy, e->atom));
}

void
cm_handle_atom(XClientMessageEvent *e, Client *c)
{
	cm_handler_t handle = NULL;

	if (XFindContext(dpy, e->message_type, ClientMessageContext, (XPointer *) &handle)
	    == Success)
		(*handle) (e, c);
	else
		DPRINTF(1, "no ClientMessage handler for %s\n", XGetAtomName(dpy, e->message_type));
}

void
handle_property_event(XPropertyEvent *e)
{
	Client *c = NULL;

	if (e)
		pushtime(&current_time, e->time);
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && e)
		pushtime(&c->last_time, e->time);
	pc_handle_atom(e, c);
}

void
handle_focus_change_event(XFocusChangeEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed)
		/* We missed a management event, so treat the window as managed
		   now... */
		managed_client(c, CurrentTime);
}

void
handle_expose_event(XExposeEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed)
		/* We missed a management event, so treat the window as managed
		   now... */
		managed_client(c, CurrentTime);
}

void
handle_visibility_event(XVisibilityEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed)
		/* We missed a management event, so treat the window as managed
		   now. */
		managed_client(c, CurrentTime);
}

void
handle_create_window_event(XCreateWindowEvent *e)
{
	Client *c = NULL;

	/* only interested in top-level windows that are not override redirect */
	if (e->override_redirect)
		return;
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if (!(c = find_client_noscreen(e->window)))
		if (e->parent == scr->root)
			c = add_client(e->window);
}

void
handle_destroy_window_event(XDestroyWindowEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->event)) /* NOTE: e->window is destroyed */
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)))
		del_client(c);
	if (e->window == scr->stray_owner) {
		DPRINTF(1, "system tray removed from 0x%08lx\n", scr->stray_owner);
		scr->stray_owner = None;
	} else if (e->window == scr->pager_owner) {
		DPRINTF(1, "pager removed from 0x%08lx\n", scr->pager_owner);
		scr->pager_owner = None;
	} else if (e->window == scr->compm_owner) {
		DPRINTF(1, "composite manager removed from 0x%08lx\n", scr->compm_owner);
		scr->compm_owner = None;
	}
	clean_msgs(e->window);
}

void
handle_map_event(XMapEvent *e)
{
	Client *c = NULL;

	if (e->override_redirect)
		return;
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed)
		/* Not sure we should do this for anything but dockapps here...
		   Window managers may map normal windows before setting the
		   WM_STATE property, and system trays invariably map window
		   before sending the _XEMBED message. */
		if ((c->dockapp = is_dockapp(c)))
			/* We missed a management event, so treat the window as
			   managed now */
			managed_client(c, CurrentTime);
}

void
handle_unmap_event(XUnmapEvent *e)
{
	/* we can't tell much from a simple unmap event */
}

void
handle_reparent_event(XReparentEvent *e)
{
	Client *c = NULL;

	/* any top-level window that is reparented by the window manager should
	   have WM_STATE set eventually (either before or after the reparenting), 
	   or receive an _XEMBED client message it they are a status icon.  The
	   exception is dock apps: some window managers reparent the dock app and 
	   never set WM_STATE on it (even though WindowMaker does). */
	if (e->override_redirect)
		return;
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed) {
		if (e->parent == scr->root)
			/* reparented the wrong way */
			return;
		if ((c->statusicon = is_statusicon(c))) {
			/* This is a status icon. */
			if (scr->stray_owner)
				/* Status icons will receive an _XEMBED message
				   from the system tray when managed. */
				return;
			/* It is questionable whether status icons will be
			   reparented at all when there is no system tray... */
			DPRINTF(1, "status icon 0x%lx reparented to 0x%lx\n",
				e->window, e->parent);
			return;
		} else if (!(c->dockapp = is_dockapp(c))) {
			/* This is a normal window. */
			if (scr->redir_check)
				/* We will receive WM_STATE property change when
				   managed (if there is a window manager
				   present). */
				return;
			/* It is questionable whether regular windows will be
			   reparented at all when there is no window manager... */
			DPRINTF(1, "normal window 0x%lx reparented to 0x%lx\n",
				e->window, e->parent);
			return;
		} else {
			/* This is a dock app. */
			if (scr->maker_check)
				/* We will receive a WM_STATE property change
				   when managed when a WindowMaker work-alike is
				   present. */
				return;
			/* Non-WindowMaker window managers reparent dock apps
			   without any further indication that the dock app has
			   been managed. */
			managed_client(c, CurrentTime);
		}
	}
}

void
handle_configure_event(XConfigureEvent *e)
{
}

void
handle_client_message_event(XClientMessageEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
	c = find_client(e->window);
	cm_handle_atom(e, c);
}

void
handle_mapping_event(XMappingEvent *e)
{
}

void
handle_selection_clear(XSelectionClearEvent *e)
{
#if 0
		int s;
#endif

		if (!find_screen(e->window)) {
			DPRINTF(1, "could not find screen for window 0x%lx\n", e->window);
			return;
		}
		if (e->selection != scr->slctn_atom)
			return;
		if (e->window != scr->selwin)
			return;
		XDestroyWindow(dpy, scr->selwin);
		scr->selwin = None;
#if 0
		for (s = 0; s < nscr; s++)
			if (screens[s].selwin)
				break;
		if (s < nscr)
			break;
#endif
		DPRINTF(1, "lost _XDG_ASSIST_S%d selection: exiting\n", scr->screen);
		exit(EXIT_SUCCESS);
}

/** @brief handle monitoring events
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
	switch (e->type) {
	case PropertyNotify:
		XPRINTF("got PropertyNotify event\n");
		handle_property_event(&e->xproperty);
		break;
	case FocusIn:
	case FocusOut:
		XPRINTF("got FocusIn/FocusOut event\n");
		handle_focus_change_event(&e->xfocus);
		break;
	case Expose:
		XPRINTF("got Expose event\n");
		handle_expose_event(&e->xexpose);
		break;
	case VisibilityNotify:
		XPRINTF("got VisibilityNotify event\n");
		handle_visibility_event(&e->xvisibility);
		break;
	case CreateNotify:
		XPRINTF("got CreateNotify event\n");
		handle_create_window_event(&e->xcreatewindow);
		break;
	case DestroyNotify:
		XPRINTF("got DestroyNotify event\n");
		handle_destroy_window_event(&e->xdestroywindow);
		break;
	case MapNotify:
		XPRINTF("got MapNotify event\n");
		handle_map_event(&e->xmap);
		break;
	case UnmapNotify:
		XPRINTF("got UnmapNotify event\n");
		handle_unmap_event(&e->xunmap);
		break;
	case ReparentNotify:
		XPRINTF("got ReparentNotify event\n");
		handle_reparent_event(&e->xreparent);
		break;
	case ConfigureNotify:
		XPRINTF("got ConfigureNotify event\n");
		handle_configure_event(&e->xconfigure);
		break;
	case ClientMessage:
		XPRINTF("got ClientMessage event\n");
		handle_client_message_event(&e->xclient);
		break;
	case MappingNotify:
		XPRINTF("got MappingNotify event\n");
		handle_mapping_event(&e->xmapping);
		break;
	default:
		DPRINTF(1, "unexpected xevent %d\n", (int) e->type);
		break;
	case SelectionClear:
		XPRINTF("got SelectionClear event\n");
		handle_selection_clear(&e->xselectionclear);
		break;
	}
}

#ifdef SYSTEM_TRAY_STATUS_ICON

static void
icon_activate_cb(GtkStatusIcon *status, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;
}

static gboolean
icon_button_press_cb(GtkStatusIcon *status, GdkEvent *event, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

static gboolean
icon_button_release_cb(GtkStatusIcon *status, GdkEvent *event, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

static gboolean
icon_popup_menu_cb(GtkStatusIcon *status, guint button, guint time, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* propagate event */
}

static gboolean
icon_query_tooltip_cb(GtkStatusIcon *status, gint x, gint y, gboolean keyboard_mode,
		      GtkTooltip *tooltip, gpointer data)
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
static gboolean
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
static gboolean
icon_size_changed_cb(GtkStatusIcon *status, gint size, gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	return FALSE;		/* did we scale the icon? otherwise GTK+ will scale */
}

/** @brief create a status icon for the startup sequence
  * @param seq - the sequence for which to create the status icon
  */
static GtkStatusIcon *
create_statusicon(Sequence *seq)
{
	GtkStatusIcon *status;

	if ((status = seq->status))
		return (status);
	DPRINTF(1, "creating status icon...\n");
	if (!(status = gtk_status_icon_new())) {
		DPRINTF(1, "could not create status icon\n");
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
		char *markup, *p;
		int len, size = BUFSIZ;
		char **label, **field;

		p = markup = calloc(size + 1, sizeof(*markup));
		for (label = (char **)StartupNotifyFields, field = seq->fields; *label; label++, field++) {
			if (!*field)
				continue;
			len = snprintf(p, size, "<b>%s</b>=%s\n", *label, *field);
			if (len > size)
				break;
			p += len;
			size -= len;
		}

		/* Set the tooltip text as markup */
		if (markup) {
			gtk_status_icon_set_tooltip_markup(status, markup);
			free(markup);
		}
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
	g_signal_connect(G_OBJECT(status), "activate", G_CALLBACK(icon_activate_cb),
			 (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "button-press-event",
			 G_CALLBACK(icon_button_press_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "button-release-event",
			 G_CALLBACK(icon_button_release_cb), (gpointer) seq);
	g_signal_connect(G_OBJECT(status), "popup-menu", G_CALLBACK(icon_popup_menu_cb),
			 (gpointer) seq);
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
static NotifyNotification *
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
			DPRINTF(1, "could not create notification\n");
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
		DPRINTF(1, "could not show notification\n");
		g_object_unref(G_OBJECT(notify));
		seq->notification = NULL;
		return (NULL);
	}
	seq->notification = notify;
	return (notify);
}

#endif				/* DESKTOP_NOTIFICATIONS */

int signum;

void
sighandler(int sig)
{
	signum = sig;
}

#ifdef HAVE_GLIB_EVENT_LOOP
gboolean
on_xfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		XEvent ev;

		while (XPending(dpy) && running) {
			XNextEvent(dpy, &ev);
			XPRINTF("Handling Event\n");
			handle_event(&ev);
		}
	}
	return TRUE;		/* keep event source */

}
#endif				/* HAVE_GLIB_EVENT_LOOP */

static void
main_loop(int argc, char *argv[])
{
#ifdef HAVE_GLIB_EVENT_LOOP
	int xfd;
	GIOChannel *chan;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	guint srce;

#ifdef HAVE_GTK
	gtk_init(NULL, NULL);
	gdk_error_trap_push();
#endif

	running = True;
	XSync(dpy, False);
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
#else				/* HAVE_GLIB_EVENT_LOOP */
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
#endif				/* HAVE_GLIB_EVENT_LOOP */
}

/** @brief monitor startup and assist the window manager.
  */
static void
do_monitor(int argc, char *argv[])
{
	if (!options.foreground) {
		pid_t pid;

		if ((pid = fork()) < 0) {
			EPRINTF("%s\n", strerror(errno));
			exit(EXIT_FAILURE);
		} else if (pid != 0) {
			/* parent exits */
			exit(EXIT_SUCCESS);
		}
		setsid();	/* become a session leader */
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
		/* release current directory */
		if (chdir("/") < 0) {
			EPRINTF("chdir: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		/* clear file creation mask */
		umask(0);
	}
	handle_wmchange();
	check_stray();
	check_pager();
	check_compm();
	/* continue on monitoring */
	main_loop(argc, argv);
}

static void
announce_selection(void)
{
	XEvent ev;

	ev.xclient.type = ClientMessage;
	ev.xclient.serial = 0;
	ev.xclient.send_event = False;
	ev.xclient.display = dpy;
	ev.xclient.window = scr->root;
	ev.xclient.message_type = _XA_MANAGER;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;	/* FIXME */
	ev.xclient.data.l[1] = scr->slctn_atom;
	ev.xclient.data.l[2] = scr->selwin;
	ev.xclient.data.l[3] = 2;
	ev.xclient.data.l[4] = 0;

	XSendEvent(dpy, scr->root, False, StructureNotifyMask, (XEvent *) &ev);
	XSync(dpy, False);
}

static Bool
selectionreleased(Display *d, XEvent *e, XPointer arg)
{
	int *n = (typeof(n)) arg;

	if (e->type != DestroyNotify)
		return False;
	if (XFindContext(d, e->xdestroywindow.window, ScreenContext, (XPointer *) &scr))
		return False;
	if (!scr->owner || scr->owner != e->xdestroywindow.window)
		return False;
	scr->owner = None;
	*n = *n - 1;
	return True;
}

/** @brief run without replacing a running instance
  *
  * This is performed by detecting owners of the _XDG_ASSIST_S%d selection for
  * any screen and aborting when one exists.
  */
static void
do_run(int argc, char *argv[])
{
	int s;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy,
										   s),
					BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if (!(scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin,
					   CurrentTime);
			XSync(dpy, False);
		}
		XUngrabServer(dpy);

		if (scr->owner) {
			EPRINTF("another instance is running on screen %d\n", s);
			exit(EXIT_FAILURE);
		}
		announce_selection();
	}
	do_monitor(argc, argv);
}

/** @brief replace running instance with this one
  *
  * This is performed by detecting owners of the _XDG_ASSIST_S%d selection for
  * each screen and setting the selection to our own window.  When the runing
  * instance detects the selection clear event for a managed screen, it will
  * destroy the selection window and exit when no more screens are managed.
  */
static void
do_replace(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy,
										   s),
					BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			selcount++;
		}
		XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
		XSync(dpy, False);
		XUngrabServer(dpy);

		if (!scr->owner)
			announce_selection();
	}
	while (selcount) {
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
		announce_selection();
	}
	do_monitor(argc, argv);
}

/** @brief ask running instance to quit
  *
  * This is performed by detecting owners of the _XDG_ASSIST_S%d selection for
  * each screen and clearing the selection to None.  When the running instance
  * detects the selection clear event for a managed screen, it will destroy
  * the selection window and exit when no more screens are managed.
  */
static void
do_quit(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			XSetSelectionOwner(dpy, scr->slctn_atom, None, CurrentTime);
			XSync(dpy, False);
			selcount++;
		}
		XUngrabServer(dpy);
	}
	/* sure, wait for them all to destroy */
	while (selcount)
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
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
        goes to the terminal) [default: false]\n\
    -g, --guard TIMEOUT\n\
        amount of time, TIMEOUT, in milliseconds to wait for a\n\
        desktop application to launch [default: 15000]\n\
    -p, --persist TIMEOUT\n\
        amount of time, TIMEOUT, in milliseconds to persist in\n\
        displaying feedback popup [default: 1000]\n\
    -A, --assist\n\
        assist window managers by settting properties on\n\
        launched windows [default: true]\n\
    -F, --feedback\n\
        provide visual feedback to the user while applications\n\
        are being launched [default: false]\n\
    -N, --notify\n\
        notify the user with notifications of excessive deslays\n\
        in launching [default: false]\n\
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
	Command command = CommandDefault;

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"monitor",	no_argument,		NULL, 'm'},
			{"replace",	no_argument,		NULL, 'r'},
			{"quit",	no_argument,		NULL, 'q'},
			{"foreground",	no_argument,		NULL, 'f'},
			{"guard",	required_argument,	NULL, 'g'},
			{"persist",	required_argument,	NULL, 'p'},
			{"assist",	no_argument,		NULL, 'A'},
			{"feedback",	no_argument,		NULL, 'F'},
			{"notify",	no_argument,		NULL, 'N'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "mrqfg:p:AFND::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getop(argc, argv, "mrqfg:p:AFNDvhVC?");
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
			if (options.command != CommandDefault)
				goto bad_command;
			options.command = CommandMonitor;
			if (command == CommandDefault)
				command = CommandMonitor;
			break;
		case 'r':	/* -r, --replace */
			if (options.command != CommandDefault)
				goto bad_command;
			options.command = CommandReplace;
			if (command == CommandDefault)
				command = CommandReplace;
			break;
		case 'q':	/* -q, --quit */
			if (options.command != CommandDefault)
				goto bad_command;
			options.command = CommandQuit;
			if (command == CommandDefault)
				command = CommandQuit;
			break;
		case 'f':	/* -f, --foreground */
			options.foreground = options.foreground ? False : True;
			break;
		case 'g':	/* -g, --guard TIMEOUT */
			if (!optarg)
				options.guard = 15000;
			else {
				if ((val = strtol(optarg, &endptr, 0)) < 0)
					goto bad_option;
				if (!endptr || *endptr)
					goto bad_option;
				options.guard = val;
			}
			break;
		case 'p':	/* -p, --persist TIMEOUT */
			if (!optarg)
				options.persist = 1000;
			else {
				if ((val = strtol(optarg, &endptr, 0)) < 0)
					goto bad_option;
				if (!endptr || *endptr)
					goto bad_option;
				options.persist = val;
			}
			break;
		case 'A':	/* -A, --assist */
			options.assist = options.assist ? False : True;
			break;
		case 'F':	/* -F, --feedback */
			options.feedback = options.feedback ? False : True;
			break;
		case 'N':	/* -N, --notify */
			options.notify = options.notify ? False : True;
			break;

		case 'D':	/* -D, --debug [level] */
			DPRINTF(1, "%s: increasing debug verbosity\n", argv[0]);
			if (!optarg) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, &endptr, 0)) < 0)
					goto bad_option;
				if (!endptr || *endptr)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			DPRINTF(1, "%s: increasing output verbosity\n", argv[0]);
			if (!optarg) {
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (!endptr || *endptr)
				goto bad_option;
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			command = CommandCopying;
			break;
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
	switch (command) {
	case CommandDefault:
	case CommandMonitor:
		if (!init_display())
			exit(EXIT_FAILURE);
		do_run(argc, argv);
		break;
	case CommandReplace:
		if (!init_display())
			exit(EXIT_FAILURE);
		do_replace(argc, argv);
		break;
	case CommandQuit:
		if (!init_display())
			exit(EXIT_FAILURE);
		do_quit(argc, argv);
		break;
	case CommandHelp:
		DPRINTF(1, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF(1, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF(1, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	default:
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
