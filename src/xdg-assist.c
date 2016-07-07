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
#ifdef GIO_GLIB2_SUPPORT
#include <glib.h>
#include <gio/gio.h>
#endif

#define XPRINTF(_args...) do { } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
		fprintf(stderr, NAME ": D: %12s: +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
		fprintf(stderr, NAME ": E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output > _num) { \
		fprintf(stdout, NAME ": I: "); \
		fprintf(stdout, _args); fflush(stdout); } } while (0)

#define PTRACE(_num) do { if (options.debug >= _num || options.output >= _num) { \
		fprintf(stderr, NAME ": T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
		fflush(stderr); } } while (0)

#define CPRINTF(_num, c, _args...) do { if (options.output > _num) { \
		fprintf(stdout, NAME ": C: 0x%08lx client (%c) ", c->win, c->managed ?  'M' : 'U'); \
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
			fprintf(stderr, NAME ": E: %12s +%4d : %s() : \t%s\n", file, line, func, strings[i]);
}

const char *program = NAME;

#define SELECTION_ATOM	"_XDG_ASSIST_S%d"

char **eargv = NULL;
int eargc = 0;

typedef enum {
	CommandDefault = 0,
	CommandAssist,
	CommandReplace,
	CommandQuit,
	CommandMonitor,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int debug;
	int output;
	char *action;
	Bool xsession;
	Bool autostart;
	Bool info;
	Bool toolwait;
	Bool assist;
	unsigned long guard;
	Command command;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.action = NULL,
	.xsession = False,
	.autostart = False,
	.info = False,
	.toolwait = False,
	.assist = True,
	.guard = 15000,
	.command = CommandDefault,
};

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

typedef struct _Entry {
	char *path;
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
} Entry;

Entry myent = { NULL, };

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
	Bool removed;
	Window from;
	Window remover;
	Window client;
	Entry *e;
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
#ifdef GIO_GLIB2_SUPPORT
	gint timer;
#endif					/* GIO_GLIB2_SUPPORT */
} Sequence;

Sequence *sequences;
Sequence myseq = { NULL, };

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

typedef struct {
	int screen;			/* screen number */
	Window root;			/* root window of screen */
	Window owner;			/* SELECTION_ATOM selection owner (theirs) */
	Window selwin;			/* SELECTION_ATOM selection window (ours) */
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;		/* set to root when SubstructureRedirect */
	Window stray_owner;		/* _NET_SYSTEM_TRAY_S%d owner */
	Window pager_owner;		/* _NET_DESKTOP_LAYOUT_S%d owner */
	Window compm_owner;		/* _NET_WM_CM_S%d owner */
	Window audio_owner;		/* PULSE_SERVER owner or root */
	Window shelp_owner;		/* _XDG_ASSIST_S%d owner */
	Atom icccm_atom;		/* WM_S%d atom for this screen */
	Atom stray_atom;		/* _NET_SYSTEM_TRAY_S%d atom this screen */
	Atom pager_atom;		/* _NET_DESKTOP_LAYOUT_S%d atom this screen */
	Atom compm_atom;		/* _NET_WM_CM_S%d atom this screen */
	Atom shelp_atom;		/* _XDG_ASSIST_S%d atom this screen */
	Atom slctn_atom;		/* SELECTION_ATOM atom this screen */
	Bool net_wm_user_time;		/* _NET_WM_USER_TIME is supported */
	Bool net_startup_id;		/* _NET_STARTUP_ID is supported */
	Bool net_startup_info;		/* _NET_STARTUP_INFO is supported */
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

int need_updates = 0;			/* numb of clients needing update */
int need_retests = 0;			/* numb of clients needing retest */
int need_changes = 0;			/* numb of clients needing change */

typedef enum _Groups {
	TransiGroup,
	WindowGroup,
	ClientGroup,
	NumberGroup,
} Groups;

typedef struct {
	Client *leader;			/* group leader window */
	Groups group;			/* which group */
	int screen;			/* screen number of group */
	int numb;			/* number of members of group */
	Client **members;		/* array of members (NULL terminated) */
} Group;

struct _Client {
	Client *next;			/* next client in list */
	Sequence *seq;			/* associated startup sequence */
	int screen;			/* screen number */
	Window win;			/* client window */
	Window icon_win;		/* icon window */
	Window time_win;		/* client time window */
	union {
		Client *leads[NumberGroup];
		struct {
			Client *g;	/* group leader */
			Client *l;	/* client leader */
			Client *t;	/* transient_for leader */
		} lead;
	};
	union {
		Group *grps[NumberGroup];
		struct {
			Group *g;	/* group leader group */
			Group *l;	/* client leader group */
			Group *t;	/* transient group */
		} grp;
	};
	Bool dockapp;			/* client is a dock app */
	Bool statusicon;		/* client is a status icon */
	Bool managed;			/* managed by window manager */
	Bool wasmanaged;		/* was managed (but not now) */
	Bool mapped;			/* set when client mapped */
	Bool listed;			/* listed by window manager */
	Bool request;			/* frame extents requested */
	Bool need_update;		/* needs to be updated */
	Bool need_retest;		/* needs to be retested */
	Bool need_change;		/* needs to be changed */
	Bool counted;			/* set when client counted */
	Time active_time;		/* last time active */
	Time focus_time;		/* last time focused */
	Time user_time;			/* last time used */
	Time map_time;			/* last time window was mapped */
	Time list_time;			/* last time window was listed */
	Time last_time;			/* last time something happened to this window */
	pid_t pid;			/* process id */
	int desktop;			/* desktop (+1) of window */
	char *startup_id;		/* startup id (property) */
	char **command;			/* words in WM_COMMAND */
	int count;			/* count of words in WM_COMMAND */
	char *name;			/* window name */
	char *hostname;			/* client machine */
	char *client_id;		/* session management id */
	char *role;			/* session management role */
	XClassHint ch;			/* class hint */
	XWMHints *wmh;			/* window manager hints */
	XWMState *wms;			/* window manager state */
	XEmbedInfo *xei;		/* _XEMBED_INFO property */
};

Client *clients = NULL;

Time current_time = CurrentTime;
Time last_user_time = CurrentTime;
Time launch_time = CurrentTime;

static Atom _XA_KDE_WM_CHANGE_STATE;
static Atom _XA_MANAGER;
static Atom _XA_MOTIF_WM_INFO;
static Atom _XA_NET_ACTIVE_WINDOW;
static Atom _XA_NET_CLIENT_LIST;
static Atom _XA_NET_CLIENT_LIST_STACKING;
static Atom _XA_NET_CLOSE_WINDOW;
static Atom _XA_NET_CURRENT_DESKTOP;
static Atom _XA_NET_DESKTOP_LAYOUT;
static Atom _XA_NET_FRAME_EXTENTS;
static Atom _XA_NET_MOVERESIZE_WINDOW;
static Atom _XA_NET_REQUEST_FRAME_EXTENTS;
static Atom _XA_NET_RESTACK_WINDOW;
static Atom _XA_NET_STARTUP_ID;
static Atom _XA_NET_STARTUP_INFO;
static Atom _XA_NET_STARTUP_INFO_BEGIN;
static Atom _XA_NET_SUPPORTED;
static Atom _XA_NET_SUPPORTING_WM_CHECK;
static Atom _XA_NET_SYSTEM_TRAY_ORIENTATION;
static Atom _XA_NET_SYSTEM_TRAY_VISUAL;
static Atom _XA_NET_VIRTUAL_ROOTS;
static Atom _XA_NET_VISIBLE_DESKTOPS;
static Atom _XA_NET_WM_ALLOWED_ACTIONS;
static Atom _XA_NET_WM_DESKTOP;
static Atom _XA_NET_WM_FULLSCREEN_MONITORS;
static Atom _XA_NET_WM_ICON_GEOMETRY;
static Atom _XA_NET_WM_ICON_NAME;
static Atom _XA_NET_WM_MOVERESIZE;
static Atom _XA_NET_WM_NAME;
static Atom _XA_NET_WM_PID;
static Atom _XA_NET_WM_STATE;
static Atom _XA_NET_WM_STATE_FOCUSED;
static Atom _XA_NET_WM_STATE_HIDDEN;
static Atom _XA_NET_WM_USER_TIME;
static Atom _XA_NET_WM_USER_TIME_WINDOW;
static Atom _XA_NET_WM_VISIBLE_ICON_NAME;
static Atom _XA_NET_WM_VISIBLE_NAME;
static Atom _XA_PULSE_COOKIE;
static Atom _XA_PULSE_ID;
static Atom _XA_PULSE_SERVER;
static Atom _XA_SM_CLIENT_ID;
static Atom _XA__SWM_VROOT;
static Atom _XA_TIMESTAMP_PROP;
static Atom _XA_UTF8_STRING;
static Atom _XA_WIN_APP_STATE;
static Atom _XA_WIN_CLIENT_LIST;
static Atom _XA_WIN_CLIENT_MOVING;
static Atom _XA_WINDOWMAKER_NOTICEBOARD;
static Atom _XA_WIN_FOCUS;
static Atom _XA_WIN_HINTS;
static Atom _XA_WIN_LAYER;
static Atom _XA_WIN_PROTOCOLS;
static Atom _XA_WIN_STATE;
static Atom _XA_WIN_SUPPORTING_WM_CHECK;
static Atom _XA_WIN_WORKSPACE;
static Atom _XA_WM_CHANGE_STATE;
static Atom _XA_WM_CLIENT_LEADER;
static Atom _XA_WM_DELETE_WINDOW;
static Atom _XA_WM_PROTOCOLS;
static Atom _XA_WM_SAVE_YOURSELF;
static Atom _XA_WM_STATE;
static Atom _XA_WM_WINDOW_ROLE;
static Atom _XA_XDG_ASSIST_CMD;
static Atom _XA_XDG_ASSIST_CMD_QUIT;
static Atom _XA_XDG_ASSIST_CMD_REPLACE;
static Atom _XA_XEMBED;
static Atom _XA_XEMBED_INFO;

static void pc_handle_MOTIF_WM_INFO(XPropertyEvent *, Client *);
static void pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_NET_DESKTOP_LAYOUT(XPropertyEvent *, Client *);
static void pc_handle_NET_FRAME_EXTENTS(XPropertyEvent *, Client *);
static void pc_handle_NET_STARTUP_ID(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTED(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_NET_SYSTEM_TRAY_ORIENTATION(XPropertyEvent *, Client *);
static void pc_handle_NET_SYSTEM_TRAY_VISUAL(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_DESKTOP(XPropertyEvent *, Client *);
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
static void cm_handle_NET_WM_DESKTOP(XClientMessageEvent *, Client *);
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
	{ "_NET_CLIENT_LIST_STACKING",		&_XA_NET_CLIENT_LIST_STACKING,	&pc_handle_NET_CLIENT_LIST_STACKING,	NULL,					None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,		&pc_handle_NET_CLIENT_LIST,		NULL,					None			},
	{ "_NET_CLOSE_WINDOW",			&_XA_NET_CLOSE_WINDOW,		NULL,					&cm_handle_NET_CLOSE_WINDOW,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,	NULL,					NULL,					None			},
	{ "_NET_DESKTOP_LAYOUT",		&_XA_NET_DESKTOP_LAYOUT,	&pc_handle_NET_DESKTOP_LAYOUT,		NULL,					None			},
	{ "_NET_FRAME_EXTENTS",			&_XA_NET_FRAME_EXTENTS,		&pc_handle_NET_FRAME_EXTENTS,		NULL,					None			},
	{ "_NET_MOVERESIZE_WINDOW",		&_XA_NET_MOVERESIZE_WINDOW,	NULL,					&cm_handle_NET_MOVERESIZE_WINDOW,	None			},
	{ "_NET_REQUEST_FRAME_EXTENTS",		&_XA_NET_REQUEST_FRAME_EXTENTS,	NULL,					&cm_handle_NET_REQUEST_FRAME_EXTENTS,	None			},
	{ "_NET_RESTACK_WINDOW",		&_XA_NET_RESTACK_WINDOW,	NULL,					&cm_handle_NET_RESTACK_WINDOW,		None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,	NULL,					&cm_handle_NET_STARTUP_INFO_BEGIN,	None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,		NULL,					&cm_handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,		&pc_handle_NET_SUPPORTED,		NULL,					None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,	&pc_handle_NET_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_NET_SYSTEM_TRAY_ORIENTATION",	&_XA_NET_SYSTEM_TRAY_ORIENTATION,&pc_handle_NET_SYSTEM_TRAY_ORIENTATION,NULL,					None			},
	{ "_NET_SYSTEM_TRAY_VISUAL",		&_XA_NET_SYSTEM_TRAY_VISUAL,	&pc_handle_NET_SYSTEM_TRAY_VISUAL,	NULL,					None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,		NULL,					NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,	NULL,					NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_WM_DESKTOP",			&_XA_NET_WM_DESKTOP,		&pc_handle_NET_WM_DESKTOP,		&cm_handle_NET_WM_DESKTOP,		None			},
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
	{ "PULSE_COOKIE",			&_XA_PULSE_COOKIE,		NULL,					NULL,					None			},
	{ "PULSE_ID",				&_XA_PULSE_ID,			NULL,					NULL,					None			},
	{ "PULSE_SERVER",			&_XA_PULSE_SERVER,		NULL,					NULL,					None			},
	{ "SM_CLIENT_ID",			&_XA_SM_CLIENT_ID,		&pc_handle_SM_CLIENT_ID,		NULL,					None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,		NULL,					NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,		&pc_handle_TIMESTAMP_PROP,		NULL,					None			},
	{ "UTF8_STRING",			&_XA_UTF8_STRING,		NULL,					NULL,					None			},
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
	{ "_XDG_ASSIST_CMD_QUIT",		&_XA_XDG_ASSIST_CMD_QUIT,	NULL,					NULL,					None			},
	{ "_XDG_ASSIST_CMD_REPLACE",		&_XA_XDG_ASSIST_CMD_REPLACE,	NULL,					NULL,					None			},
	{ "_XDG_ASSIST_CMD",			&_XA_XDG_ASSIST_CMD,		NULL,					NULL,					None			},
	{ "_XEMBED_INFO",			&_XA_XEMBED_INFO,		&pc_handle_XEMBED_INFO,			NULL,					None			},
	{ "_XEMBED",				&_XA_XEMBED,			NULL,					&cm_handle_XEMBED,			None			},
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
		fprintf(stderr, "X error %s(0x%08lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

int
iohandler(Display *display)
{
	void *buffer[1024];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 1023)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, "backtrace> %s\n", strings[i]);
	exit(EXIT_FAILURE);
}

int (*oldhandler) (Display *, XErrorEvent *) = NULL;
int (*oldiohandler) (Display *) = NULL;

static void init_screen();

Bool
get_display()
{
	PTRACE(5);
	if (!dpy) {
		int s;
		char sel[64] = { 0, };

		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		oldhandler = XSetErrorHandler(handler);
		oldiohandler = XSetIOErrorHandler(iohandler);

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
			scr->shelp_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), SELECTION_ATOM, s);
			scr->slctn_atom = XInternAtom(dpy, sel, False);
			init_screen();
		}
		s = DefaultScreen(dpy);
		scr = screens + s;
	}
	return (dpy ? True : False);
}

void
put_display()
{
	PTRACE(5);
	if (dpy) {
		XSetErrorHandler(oldhandler);
		XSetIOErrorHandler(oldiohandler);
		XCloseDisplay(dpy);
		dpy = NULL;
	}
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
	if (XGetWindowProperty(dpy, win, prop, 0L, num, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
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
				XSelectInput(dpy, check, StructureNotifyMask | PropertyChangeMask);
			if (data) {
				XFree(data);
				data = NULL;
			}
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty(dpy, check, atom, 0L, 1L, False, type, &real,
				       &format, &nitems, &after, (unsigned char **) &data)
		    == Success && format != 0) {
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

	OPRINTF(1, "non-recursive check for atom 0x%08lx\n", atom);

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
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

	OPRINTF(1, "check for non-compliant NetWM\n");

      try_harder:
	if (XGetWindowProperty(dpy, scr->root, protocols, 0L, num, False, XA_ATOM, &real,
			       &format, &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
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
	Window win;

	do {
		win = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
	} while (i++ < 2 && !win);

	if (win && win != scr->root) {
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} else
		win = check_netwm_supported();

	if (win && win != scr->netwm_check)
		DPRINTF(1, "NetWM/EWMH changed from 0x%08lx to 0x%08lx\n", scr->netwm_check, win);
	if (!win && scr->netwm_check)
		DPRINTF(1, "NetWM/EWMH removed from 0x%08lx\n", scr->netwm_check);

	return (scr->netwm_check = win);
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
	Window win;

	do {
		win = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
	} while (i++ < 2 && !win);

	if (win && win != scr->root) {
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} else
		win = check_winwm_supported();

	if (win && win != scr->winwm_check)
		DPRINTF(1, "WinWM/WMH changed from 0x%08lx to 0x%08lx\n", scr->winwm_check, win);
	if (!win && scr->winwm_check)
		DPRINTF(1, "WinWM/WMH removed from 0x%08lx\n", scr->winwm_check);

	return (scr->winwm_check = win);
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker()
{
	int i = 0;
	Window win;

	do {
		win = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW);
	} while (i++ < 2 && !win);

	if (win && win != scr->root) {
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	}

	if (win && win != scr->maker_check)
		DPRINTF(1, "WindowMaker changed from 0x%08lx to 0x%08lx\n", scr->maker_check, win);
	if (!win && scr->maker_check)
		DPRINTF(1, "WindowMaker removed from 0x%08lx\n", scr->maker_check);

	return (scr->maker_check = win);
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
		data = get_cardinals(scr->root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n);
	} while (i++ < 2 && !data);

	if (data && n >= 2)
		win = data[1];
	if (data)
		XFree(data);
	if (win && win != scr->root) {
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	}

	if (win && win != scr->motif_check)
		DPRINTF(1, "OSF/MOTIF changed from 0x%08lx to 0x%08lx\n", scr->motif_check, win);
	if (!win && scr->motif_check)
		DPRINTF(1, "OSF/MOTIF removed from 0x%08lx\n", scr->motif_check);

	return (scr->motif_check = win);
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	Window win;

	win = XGetSelectionOwner(dpy, scr->icccm_atom);

	if (win && win != scr->root) {
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	}
	if (win && win != scr->icccm_check)
		DPRINTF(1, "ICCCM 2.0 WM changed from 0x%08lx to 0x%08lx\n", scr->icccm_check, win);
	if (!win && scr->icccm_check)
		DPRINTF(1, "ICCCM 2.0 WM removed from 0x%08lx\n", scr->icccm_check);
	return (scr->icccm_check = win);
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

	OPRINTF(1, "checking redirection for screen %d\n", scr->screen);
	if (XGetWindowAttributes(dpy, scr->root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			win = scr->root;
	if (win && win != scr->redir_check)
		DPRINTF(1, "WM redirection changed from 0x%08lx to 0x%08lx\n", scr->redir_check, win);
	if (!win && scr->redir_check)
		DPRINTF(1, "WM redirection removed from 0x%08lx\n", scr->redir_check);
	return (scr->redir_check = win);
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
		OPRINTF(1, "redirection on window 0x%08lx\n", scr->redir_check);
	}
	OPRINTF(1, "checking ICCCM 2.0 compliance\n");
	if (check_icccm()) {
		have_wm = True;
		OPRINTF(1, "ICCCM 2.0 window 0x%08lx\n", scr->icccm_check);
	}
	OPRINTF(1, "checking OSF/Motif compliance\n");
	if (check_motif()) {
		have_wm = True;
		OPRINTF(1, "OSF/Motif window 0x%08lx\n", scr->motif_check);
	}
	OPRINTF(1, "checking WindowMaker compliance\n");
	if (check_maker()) {
		have_wm = True;
		OPRINTF(1, "WindowMaker window 0x%08lx\n", scr->maker_check);
	}
	OPRINTF(1, "checking GNOME/WMH compliance\n");
	if (check_winwm()) {
		have_wm = True;
		OPRINTF(1, "GNOME/WMH window 0x%08lx\n", scr->winwm_check);
	}
	scr->net_wm_user_time = False;
	scr->net_startup_id = False;
	scr->net_startup_info = False;
	OPRINTF(1, "checking NetWM/EWMH compliance\n");
	if (check_netwm()) {
		have_wm = True;
		OPRINTF(1, "NetWM/EWMH window 0x%08lx\n", scr->netwm_check);
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_WM_USER_TIME)) {
			OPRINTF(1, "_NET_WM_USER_TIME supported\n");
			scr->net_wm_user_time = True;
		}
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
			OPRINTF(1, "_NET_STARTUP_ID supported\n");
			scr->net_startup_id = True;
		}
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_INFO)) {
			OPRINTF(1, "_NET_STARTUP_INFO supported\n");
			scr->net_startup_info = True;
		}
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

Window
check_audio()
{
	char *text;

	if (!(text = get_text(scr->root, _XA_PULSE_COOKIE)))
		return (scr->audio_owner = None);
	XFree(text);
	if (!(text = get_text(scr->root, _XA_PULSE_SERVER)))
		return (scr->audio_owner = None);
	XFree(text);
	if (!(text = get_text(scr->root, _XA_PULSE_ID)))
		return (scr->audio_owner = None);
	XFree(text);
	return (scr->audio_owner = scr->root);
}

static Window
check_shelp()
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->shelp_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != scr->shelp_owner)
		DPRINTF(1, "startup helper changed from 0x%08lx to 0x%08lx\n", scr->shelp_owner, win);
	if (!win && scr->shelp_owner)
		DPRINTF(1, "startup helper removed from 0x%08lx\n", scr->shelp_owner);
	return (scr->shelp_owner = win);
}

static void
handle_wmchange()
{
	if (!check_window_manager())
		check_window_manager();
}

static void
init_screen()
{
	handle_wmchange();
	check_stray();
	check_pager();
	check_compm();
	check_audio();
	check_shelp();
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
pc_handle_WIN_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

static void
pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	handle_wmchange();
}

Bool
set_screen_of_root(Window sroot)
{
	int s;

	DPRINTF(4, "searching for screen with root window 0x%08lx\n", sroot);
	for (s = 0; s < nscr; s++) {
		DPRINTF(5, "comparing 0x%08lx and 0x%08lx\n", sroot, screens[s].root);
		if (screens[s].root == sroot) {
			scr = screens + s;
			DPRINTF(4, "found screen %d\n", s);
			return True;
		}
	}
	EPRINTF("could not find screen for root 0x%08lx!\n", sroot);
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

	if (set_screen_of_root(froot)) {
		XSaveContext(dpy, frame, ScreenContext, (XPointer) scr);
		return True;
	}
	return False;
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
		DPRINTF(4, "could not query tree for window 0x%08lx\n", w);
		return False;
	}
	if (dwp)
		XFree(dwp);

	DPRINTF(4, "window 0x%08lx has root 0x%08lx\n", w, wroot);
	if (set_screen_of_root(wroot)) {
		XSaveContext(dpy, w, ScreenContext, (XPointer) scr);
		return True;
	}
	return False;
}

Bool
find_screen(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ScreenContext, (XPointer *) &scr))
		return True;
	DPRINTF(4, "no ScreenContext for window 0x%08lx\n", w);
	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = screens + c->screen;
		return True;
	}
	DPRINTF(4, "no ClientContext for window 0x%08lx\n", w);
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

void
del_group(Client *c, Groups group)
{
	Client *l, **m;
	Group *g;

	if (!c)
		return;
	if (!(l = c->leads[group]))
		return;
	if (!(g = l->grps[group]))
		return;
	for (m = g->members; *m; m++)
		if (*m == c) {
			g->numb--;
			break;
		}
	for (; *m; m++)
		*m = *(m + 1);
	if (!g->numb) {
		free(g->members);
		g->members = NULL;
		free(g);
		l->grps[group] = NULL;
	}
	c->leads[group] = NULL;
}

static Client *get_client(Window win);

const char *
show_group(Groups group)
{
	switch (group) {
	case TransiGroup:
		return ("WM_TRANSIENT_FOR");
	case WindowGroup:
		return ("WM_HINTS(group)");
	case ClientGroup:
		return ("WM_CLIENT_LEADER");
	default:
		return ("???");
	}
}

void
add_group(Client *c, Window leader, Groups group)
{
	Client *l;
	Group *g;

	if (!c)
		return;
	if ((l = c->leads[group])) {
		if (l->win == leader)
			return;
		del_group(c, group);
	}
	if (!leader || leader == c->win || leader == scr->root)
		return;
	if (!(l = get_client(leader)))
		return;
	if (!(g = l->grps[group])) {
		CPRINTF(1, c, "[ag] %s = 0x%08lx (new)\n", show_group(group), leader);
		g = calloc(1, sizeof(*g));
		g->leader = l;
		g->group = group;
		g->screen = scr->screen;
		g->numb = 1;
		g->members = realloc(g->members, (g->numb + 1) * sizeof(*g->members));
		g->members[0] = c;
		g->members[1] = NULL;
		l->grps[group] = g;
	} else {
		Client **m;

		for (m = g->members; *m; m++)
			if (*m == c)
				break;
		if (!*m) {
			g->members = realloc(g->members, (g->numb + 2) * sizeof(*g->members));
			g->members[g->numb] = c;
			g->numb++;
			g->members[g->numb] = NULL;
		}
	}
	return;
}

/*
 * Check whether the window manager needs assitance: return True when it needs assistance
 * and False otherwise.
 *
 * When the WM supports _NET_STARTUP_ID and _NET_WM_USER_TIME, it should be able
 * to get its own timestamps out of _NET_STARTUP_ID.  When the WM supports
 * _NET_STARTUP_INFO(_BEGIN), it should be able to intercept its own startup
 * notification messages to at least determine the desktop/workspace upon which
 * the window should start in addition to the above; also, it should be able to
 * determine from WMCLASS= which messages belong to which newly mapped window.
 */
Bool
need_assistance()
{
	if (check_shelp()) {
		DPRINTF(1, "No assistance needed: Startup notification helper running\n");
		return False;
	}
	if (!check_netwm()) {
		DPRINTF(1, "Failed NetWM check!\n");
		if (options.info)
			fputs("Assistance required: window manager failed NetWM check\n", stdout);
		return True;
	}
	if (!check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
		DPRINTF(1, "_NET_STARTUP_ID not supported\n");
		if (options.info)
			fputs("Assistance required: window manager does not support _NET_STARTUP_ID\n",
			     stdout);
		return True;
	}
	return False;
}

int
parse_file(char *path, Entry *e)
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
	DPRINTF(1, "language is '%s'\n", llang);

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

	OPRINTF(1, "long  language string is '%s'\n", llang);
	OPRINTF(1, "short language string is '%s'\n", slang);

	if (!(file = fopen(path, "r"))) {
		EPRINTF("cannot open file '%s' for reading\n", path);
		exit(EXIT_FAILURE);
	}
	free(e->path);
	e->path = strdup(path);
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
				free(e->Type);
				e->Type = strdup(val);
				/* autodetect XSession */
				if (!strcmp(val, "XSession") && !options.xsession)
					options.xsession = True;
				ok = 1;
				continue;
			}
		} else if (strstr(key, "Name") == key) {
			if (strcmp(key, "Name") == 0) {
				free(e->Name);
				e->Name = strdup(val);
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
					free(e->Name);
					e->Name = strdup(val);
					ok = 1;
				}
			}
		} else if (strstr(key, "Comment") == key) {
			if (strcmp(key, "Comment") == 0) {
				free(e->Comment);
				e->Comment = strdup(val);
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
					free(e->Comment);
					e->Comment = strdup(val);
				}
			}
		} else if (strcmp(key, "Icon") == 0) {
			free(e->Icon);
			e->Icon = strdup(val);
			ok = 1;
		} else if (strcmp(key, "TryExec") == 0) {
			if (!e->TryExec)
				e->TryExec = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Exec") == 0) {
			free(e->Exec);
			e->Exec = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Terminal") == 0) {
			if (!e->Terminal)
				e->Terminal = strdup(val);
			ok = 1;
		} else if (strcmp(key, "StartupNotify") == 0) {
			if (!e->StartupNotify)
				e->StartupNotify = strdup(val);
			ok = 1;
		} else if (strcmp(key, "StartupWMClass") == 0) {
			if (!e->StartupWMClass)
				e->StartupWMClass = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Categories") == 0) {
			if (!e->Categories)
				e->Categories = strdup(val);
			ok = 1;
		} else if (strcmp(key, "MimeType") == 0) {
			if (!e->MimeType)
				e->MimeType = strdup(val);
			ok = 1;
		} else if (strcmp(key, "AsRoot") == 0) {
			if (!e->AsRoot)
				e->AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-KDE-RootOnly") == 0) {
			if (!e->AsRoot)
				e->AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-SuSE-YaST-RootOnly") == 0) {
			if (!e->AsRoot)
				e->AsRoot = strdup(val);
			ok = 1;
		} else if (strcmp(key, "X-GNOME-Autostart-Phase") == 0) {
			if (!e->AutostartPhase) {
				if (strcmp(val, "Applications") == 0) {
					e->AutostartPhase = strdup("Application");
					OPRINTF(0, "change \"%s=%s\" to \"%s=Application\" in %s\n", key, val, key, path);
				} else
					e->AutostartPhase = strdup(val);
			}
		} else if (strcmp(key, "X-KDE-autostart-phase") == 0) {
			if (!e->AutostartPhase) {
				switch (atoi(val)) {
				case 0:
				case 1:
					e->AutostartPhase = strdup("Initializing");
					break;
				case 2:
					e->AutostartPhase = strdup("Application");
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

	appid = calloc(PATH_MAX + 1, sizeof(*appid));
	strncat(appid, name, PATH_MAX);

	if (strstr(appid, ".desktop") != appid + strlen(appid) - 8)
		strncat(appid, ".desktop", PATH_MAX);
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
			path = realloc(path, PATH_MAX + 1);
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
			path = realloc(path, PATH_MAX + 1);
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
			path = realloc(path, PATH_MAX + 1);
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

volatile Bool running = False;

void
send_msg(char *msg)
{
	XEvent xev;
	int l;
	Window from;
	char *p;

	DPRINTF(1, "Message to 0x%08lx is: '%s'\n", scr->root, msg);

	if (options.info) {
		fputs("Would send the following startup notification message:\n\n", stdout);
		fprintf(stdout, "%s\n\n", msg);
		return;
	}

	if (!(from = scr->selwin)) {
		EPRINTF("no selection window, creating one!\n");
		from = XCreateSimpleWindow(dpy, scr->root, 0, 0, 1, 1, 0, ParentRelative, ParentRelative);
	}

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
		if (!XSendEvent(dpy, scr->root, False, StructureNotifyMask |
				SubstructureNotifyMask | SubstructureRedirectMask |
				PropertyChangeMask, &xev))
			EPRINTF("XSendEvent: failed!\n");
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}

	XSync(dpy, False);
	if (from != scr->selwin) {
		EPRINTF("no selection window, destroying one!\n");
		XDestroyWindow(dpy, from);
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

	file = calloc(PATH_MAX + 1, sizeof(*file));
	snprintf(file, PATH_MAX, "/proc/%d/%s", pid, name);

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
	char *file, *buf;
	ssize_t size;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	snprintf(file, PATH_MAX, "/proc/%d/exe", pid);
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

void
need_update(Client *c)
{
	if (!c)
		return;
	if (!c->need_update) {
		CPRINTF(1, c, "[nu] need update\n");
		c->need_update = True;
		need_updates++;
	}
}

void
need_retest(Client *c)
{
	if (!c)
		return;
	if (c->seq) {
		if (c->need_retest) {
			CPRINTF(1, c, "[nr] false alarm\n");
			c->need_retest = False;
			need_retests--;
		}
		if (!c->counted) {
			if (!c->need_change) {
				CPRINTF(1, c, "[nr] need change\n");
				c->need_change = True;
				need_changes++;
			}
		}
	} else {
		if (!c->need_update && !c->need_retest) {
			CPRINTF(1, c, "[nr] need retest\n");
			c->need_retest = True;
			need_retests++;
		}
	}
}

void
group_needs_retest(Client *c, Groups group)
{
	Group *g;
	Client **m;

	if (!c)
		return;
	if ((g = c->grps[group]))
		for (m = g->members; *m; m++)
			need_retest(*m);
}

void
groups_need_retest(Client *c)
{
	group_needs_retest(c, WindowGroup);
	group_needs_retest(c, ClientGroup);
	group_needs_retest(c, TransiGroup);
	need_retest(c);
}

Bool
samehost(const char *dom1, const char *dom2)
{
	const char *str;
	char c;

	if (!dom1 || !dom2)
		return False;
	if (!strcasecmp(dom1, dom2))
		return True;
	if (((str = strcasestr(dom1, dom2)) == dom1) && (!(c = *(str + strlen(dom2))) || c == '.'))
		return True;
	if (((str = strcasestr(dom2, dom1)) == dom2) && (!(c = *(str + strlen(dom1))) || c == '.'))
		return True;
	return False;
}

Bool
islocalhost(const char *host)
{
	char buf[65] = { 0, };

	if (!host)
		return False;
	if (gethostname(buf, sizeof(buf) - 1))
		return False;
	return samehost(buf, host);
}

/** @brief test a client against a startup notification sequence
  * @param c - client to test
  * @param seq - sequence against which to test
  * @return Bool - True if the client matches the sequence, False otherwise
  *
  * We should only test windows that are considered managed and that have not
  * yet been counted as belonging to the application.  Returns True if the
  * properties of the window match the application launch sequence.
  */
static Bool
test_client(Client *c, Sequence *seq)
{
	pid_t pid;
	Client *l;
	char *str, *startup_id, *hostname, *res_name, *res_class;
	char **command;
	int count;

	CPRINTF(1, c, "[tc] seq '%s': testing\n", seq->f.id);
	/* correct _NET_WM_STARTUP_ID */
	startup_id = c->startup_id;
	if (!startup_id && (l = c->leads[TransiGroup]))
		startup_id = l->startup_id;
	if (!startup_id && (l = c->leads[WindowGroup]))
		startup_id = l->startup_id;
	if (!startup_id && (l = c->leads[ClientGroup]))
		startup_id = l->startup_id;
	if (seq->f.id && startup_id) {
		if (strcmp(startup_id, seq->f.id)) {
			CPRINTF(1, c, "[tc] has different startup id %s\n", startup_id);
			return False;
		} else {
			CPRINTF(1, c, "[tc] has same startup id %s\n", startup_id);
			return True;
		}
	} else
		CPRINTF(1, c, "[tc] cannot test startup id\n");
	/* correct hostname */
	hostname = c->hostname;
	if (!hostname && (l = c->leads[TransiGroup]))
		hostname = l->hostname;
	if (!hostname && (l = c->leads[WindowGroup]))
		hostname = l->hostname;
	if (!hostname && (l = c->leads[ClientGroup]))
		hostname = l->hostname;
	pid = c->pid;
	if (!pid && (l = c->leads[TransiGroup]))
		pid = l->pid;
	if (!pid && (l = c->leads[WindowGroup]))
		pid = l->pid;
	if (!pid && (l = c->leads[ClientGroup]))
		pid = l->pid;
	if (seq->f.hostname && hostname) {
		/* check exact and subdomain mismatch */
		if (!samehost(seq->f.hostname, hostname)) {
			CPRINTF(1, c, "[tc] has different hostname %s !~ %s\n", hostname, seq->f.hostname);
			return False;
		}
		CPRINTF(1, c, "[tc] has comparable hostname %s ~ %s\n", hostname, seq->f.hostname);
		/* same process id (on same host) */
		if (pid && seq->n.pid && (pid == seq->n.pid)) {
			CPRINTF(1, c, "[tc] has correct pid %d\n", pid);
			return True;
		}
		if (pid)
			CPRINTF(1, c, "[tc] has different pid %d\n", pid);
		/* incorrect pid is not conclusive */
	} else
		CPRINTF(1, c, "[tc] cannot test hostname\n");
	/* is pid on localhost? */
	if (pid && !islocalhost(hostname)) {
		CPRINTF(1, c, "[tc] cannot test local pid\n");
		pid = 0;
	} else
		CPRINTF(1, c, "[tc] can test local pid\n");
	/* is environment DESKTOP_STARTUP_ID correct? */
	if (pid && (str = get_proc_startup_id(pid))) {
		CPRINTF(1, c, "[tc] comparing '%s' and '%s'\n", str, seq->f.id);
		if (strcmp(seq->f.id, str)) {
			CPRINTF(1, c, "[tc] has different startup id %s\n", str);
			free(str);
			return False;
		} else {
			CPRINTF(1, c, "[tc] has same startup id %s\n", str);
			free(str);
			return True;
		}
	} else
		CPRINTF(1, c, "[tc] cannot test process startup_id\n");
	/* correct wmclass */
	res_name = c->ch.res_name;
	res_class = c->ch.res_class;
	if (!res_name && !res_class && (l = c->leads[TransiGroup])) {
		res_name = l->ch.res_name;
		res_class = l->ch.res_class;
	}
	if (!res_name && !res_class && (l = c->leads[WindowGroup])) {
		res_name = l->ch.res_name;
		res_class = l->ch.res_class;
	}
	if (!res_name && !res_class && (l = c->leads[ClientGroup])) {
		res_name = l->ch.res_name;
		res_class = l->ch.res_class;
	}
	if (seq->f.wmclass) {
		if (res_name && !strcasecmp(seq->f.wmclass, res_name)) {
			CPRINTF(1, c, "[tc] has comparable resource name: %s ~ %s\n", seq->f.wmclass,
				res_name);
			return True;
		}
		if (res_class && !strcasecmp(seq->f.wmclass, res_class)) {
			CPRINTF(1, c, "[tc] has comparable resource class: %s ~ %s\n", seq->f.wmclass,
				res_class);
			return True;
		}
	} else
		CPRINTF(1, c, "[tc] cannot test WM_CLASS\n");
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == seq->n.timestamp) {
		CPRINTF(1, c, "[tc] has identical user time %lu\n", c->user_time);
		return True;
	}
	/* correct command */
	command = c->command;
	count = c->count;
	if (!command && (l = c->leads[TransiGroup])) {
		command = l->command;
		count = l->count;
	}
	if (!command && (l = c->leads[WindowGroup])) {
		command = l->command;
		count = l->count;
	}
	if (!command && (l = c->leads[ClientGroup])) {
		command = l->command;
		count = l->count;
	}
	if (command && (eargv || seq->f.command)) {
		int i;

		if (eargv) {
			if (c->count == eargc) {
				for (i = 0; i < c->count; i++)
					if (strcmp(eargv[i], c->command[i]))
						break;
				if (i == c->count) {
					CPRINTF(1, c, "has same command\n");
					return True;
				}
			}
		} else if (seq->f.command) {
			wordexp_t we = { 0, };

			if (wordexp(seq->f.command, &we, 0) == 0) {
				if (we.we_wordc == count) {
					for (i = 0; i < count; i++)
						if (strcmp(we.we_wordv[i], command[i]))
							break;
					if (i == count) {
						CPRINTF(1, c, "[tc] has same command\n");
						wordfree(&we);
						return True;
					}
				}
				wordfree(&we);
			}
		}
		CPRINTF(1, c, "[tc] has different command\n");
		/* NOTE: there are cases where the command associated with a window is
		   different from the launch command: in particular, where the launch
		   command is a script or program that 'exec's another.  Therefore, the
		   command being different may be a false negative. */
	}
	/* correct executable */
	if (pid && seq->f.bin) {
		if ((str = get_proc_comm(pid)))
			if (!strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[tc] has same binary %s\n", str);
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			if (!strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[tc] has same binary %s\n", str);
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			if (!strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[tc] has same binary %s\n", str);
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

/** @brief perform a quick assist on the client and window manager
  * @param c - the client to assist
  * @param seq - the startup notification sequence associated with the client
  *
  * Some client window properties need to be set before the window is mapped;
  * otherwise, the window manager may not do the right thing.  We set them in
  * order of priority (valuable before mapping), with ones that do not really
  * need to be set before the window is mapped last.  If the window manager does
  * not require assistance, it is most important to set up _NET_STARTUP_ID;
  * otherwise, it is most important to set up _NET_WM_USER_TIME and
  * _NET_WM_DESKTOP, as these are required before the window is mapped; the most
  * important of these being _NET_WM_USER_TIME.  If _NET_WM_DESKTOP is set and a
  * WM_STATE property exists afterward, a client message should be sent to the
  * root for the window manager that switches the window to that desktop.
  */
void
assist_client(Client *c, Sequence *seq)
{
	long data;

	if (c->seq) {
		EPRINTF("client already set up!\n");
		return;
	}
	c->seq = ref_sequence(seq);
	seq->client = c->win;

	/* set up _NET_STARTUP_ID */
	if (seq->f.id && !c->startup_id) {
		XTextProperty xtp = { 0, };
		char *list[2] = { NULL, };
		int count = 1;

		CPRINTF(1, c, "[ac] _NET_STARTUP_ID <== %s\n", seq->f.id);
		if (options.assist) {
			list[0] = seq->f.id;
			Xutf8TextListToTextProperty(dpy, list, count, XUTF8StringStyle, &xtp);
			XSetTextProperty(dpy, c->win, &xtp, _XA_NET_STARTUP_ID);
			c->startup_id = (char *) xtp.value;
		}
	}
	/* set up _NET_WM_USER_TIME */
	if (scr->netwm_check && seq->f.timestamp && (data = seq->n.timestamp)) {
		CPRINTF(1, c, "[ac] _NET_WM_USER_TIME <== %ld\n", data);
		if (options.assist) {
			XChangeProperty(dpy, c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *) &data, 1);
		}
	}
	/* set up _NET_WM_DESKTOP and _WIN_WORKSPACE */
	if (seq->f.desktop && scr->netwm_check) {
		data = seq->n.desktop;
		CPRINTF(1, c, "[ac] _NET_WM_DESKTOP <== %ld\n", data);
		if (options.assist) {
			XChangeProperty(dpy, c->win, _XA_NET_WM_DESKTOP, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *) &data, 1);
		}
	} else if (seq->f.desktop && scr->winwm_check) {
		data = seq->n.desktop;
		CPRINTF(1, c, "[ac] _WIN_WORKSPACE <== %ld\n", data);
		if (options.assist) {
			XChangeProperty(dpy, c->win, _XA_WIN_WORKSPACE, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *) &data, 1);
		}
	}
}

static Bool is_dockapp(Client *c);
static Bool is_trayicon(Client *c);

static Bool
same_client(Window a, Window b)
{
	if (!a || !b)
		return False;
	if (a == b)
		return True;
	if ((a&0xffff0000) == (b&0xffff0000))
		return True;
	return False;
}

static Bool
msg_from_wm(Window w)
{
	if (same_client(w, scr->netwm_check))
		goto yes;
	if (same_client(w, scr->winwm_check))
		goto yes;
	if (same_client(w, scr->motif_check))
		goto yes;
	if (same_client(w, scr->icccm_check))
		goto yes;
	return False;
yes:
	return True;
}

static Bool
msg_from_la(Window w)
{
	if (!same_client(w, scr->shelp_owner))
		return False;
	return True;
}

static Bool
msg_from_me(Window w)
{
	if (!same_client(w, scr->selwin))
		return False;
	return True;
}

static Bool
msg_from_ap(Window w, Window c)
{
	if (!same_client(w, c))
		return False;
	return True;
}

/** @brief assist some clients by adding information missing from window
  *
  * Setting up the client consists of setting some EWMH and other properties on
  * the (group or leader) window.  Because we can get here for tool wait, we
  * must check whether assistance is desired.
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
	Entry *e;
	Sequence *seq;
	Bool need_change = False;

	PTRACE(5);
	if (!(seq = c->seq))
		return;
	seq->client = c->win;
	/* set up _NET_STARTUP_ID */
	if (!c->startup_id && seq->f.id) {
		XTextProperty xtp = { 0, };
		char *list[2] = { NULL, };
		int count = 1;

		CPRINTF(1, c, "[sc] _NET_STARTUP_ID <== %s\n", seq->f.id);
		if (options.assist) {
			list[0] = seq->f.id;
			Xutf8TextListToTextProperty(dpy, list, count, XUTF8StringStyle, &xtp);
			XSetTextProperty(dpy, c->win, &xtp, _XA_NET_STARTUP_ID);
			if (xtp.value)
				XFree(xtp.value);
		}
	}
	/* set up _NET_WM_USER_TIME */
	if (seq->f.timestamp && seq->n.timestamp &&
	    (!c->user_time || !latertime(c->user_time, seq->n.timestamp))) {
		long data = seq->n.timestamp;

		CPRINTF(1, c, "[sc] _NET_WM_USER_TIME <== %ld\n", data);
		if (options.assist) {
			XChangeProperty(dpy, c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *) &data, 1);
			c->user_time = seq->n.timestamp;
		}
	}
	/* set up _NET_WM_DESKTOP */
	if (seq->f.desktop) {
		long data = seq->n.desktop;

		CPRINTF(1, c, "[sc] _NET_WM_DESKTOP <== %ld\n", data);
		if (options.assist) {
			XChangeProperty(dpy, c->win, _XA_NET_WM_DESKTOP, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *) &data, 1);
		}
	}
	/* set up WM_CLIENT_MACHINE */
	if (!c->hostname && seq->f.hostname) {
		XTextProperty xtp = { 0, };
		char *list[2] = { NULL, };
		int count = 1;

		CPRINTF(1, c, "[sc] WM_CLIENT_MACHINE <== %s\n", seq->f.hostname);
		if (options.assist) {
			list[0] = seq->f.hostname;
			XStringListToTextProperty(list, count, &xtp);
			if (xtp.value) {
				XSetWMClientMachine(dpy, c->win, &xtp);
				XFree(xtp.value);
			}
		}
	}
	/* set up WM_COMMAND */
	if (!c->command && (eargv || seq->f.command)) {
		if (eargv)
			XSetCommand(dpy, c->win, eargv, eargc);
		else if (seq->f.command) {
			wordexp_t we = { 0, };

			CPRINTF(1, c, "[sc] WM_COMMAND <== %s\n", seq->f.command);
			if (options.assist) {
				if (wordexp(seq->f.command, &we, 0) == 0) {
					XSetCommand(dpy, c->win, we.we_wordv, we.we_wordc);
					wordfree(&we);
				}
			}
		}
	} else
		/* use /proc/[pid]/cmdline to set up WM_COMMAND if not present */
	if (!c->command && c->pid) {
		char *string;
		size_t size;

		if (islocalhost(c->hostname) || islocalhost(seq->f.hostname)) {
			if ((string = get_proc_file(c->pid, "cmdline", &size))) {
				CPRINTF(1, c, "[sc] WM_COMMAND <== %s\n", string);
				if (options.assist) {
					XChangeProperty(dpy, c->win, XA_WM_COMMAND,
							XA_STRING, 8, PropModeReplace,
							(unsigned char *) string, size);
				}
				free(string);
			}
		}
	}
	/* change BIN= */
	if (!seq->f.bin && c->command && c->command[0]) {
		seq->f.bin = strdup(c->command[0]);
		CPRINTF(1, c, "[sc] %s BIN=%s\n", seq->f.launcher, seq->f.bin);
		need_change = True;
	}
	/* change DESCRIPTION= */
	if (!seq->f.description && c->name) {
		seq->f.description = strdup(c->name);
		CPRINTF(1, c, "[sc] %s DESCRIPTION=%s\n", seq->f.launcher, seq->f.description);
		if (seq->f.application_id)
			CPRINTF(1, c, "[sc] add Comment= to %s\n", seq->f.application_id);
		need_change = True;
	}
	/* change WMCLASS= */
	if (!seq->f.wmclass && (c->ch.res_name || c->ch.res_class)) {
		if (c->ch.res_class) {
			seq->f.wmclass = strdup(c->ch.res_class);
			CPRINTF(1, c, "[sc] %s WMCLASS=%s\n", seq->f.launcher, seq->f.wmclass);
			need_change = True;
		} else if (c->ch.res_name) {
			seq->f.wmclass = strdup(c->ch.res_name);
			CPRINTF(1, c, "[sc] %s WMCLASS=%s\n", seq->f.launcher, seq->f.wmclass);
			need_change = True;
		}
	}
	/* change DESKTOP= */
	if (!seq->f.desktop && c->desktop) {
		char buf[65] = { 0, };

		snprintf(buf, sizeof(buf), "%d", c->desktop - 1);
		seq->f.desktop = strdup(buf);
		seq->n.desktop = c->desktop - 1;
		CPRINTF(1, c, "[sc] %s DESKTOP=%s\n", seq->f.launcher, seq->f.desktop);
		need_change = True;
	}
	/* change SCREEN= */
	if (!seq->f.screen) {
		char buf[65] = { 0, };

		snprintf(buf, sizeof(buf), "%d", c->screen);
		seq->f.screen = strdup(buf);
		seq->n.screen = c->screen;
		CPRINTF(1, c, "[sc] %s SCREEN=%s\n", seq->f.launcher, seq->f.screen);
		need_change = True;
	}
	/* change PID= */
	if (!seq->f.pid && c->pid) {
		char buf[65] = { 0, };

		snprintf(buf, sizeof(buf), "%d", c->pid);
		seq->f.pid = strdup(buf);
		seq->n.pid = c->pid;
		CPRINTF(1, c, "[sc] %s PID=%s\n", seq->f.launcher, seq->f.pid);
		need_change = True;
	}
	/* change HOSTNAME= */
	if (!seq->f.hostname && c->hostname) {
		seq->f.hostname = strdup(c->hostname);
		CPRINTF(1, c, "[sc] %s HOSTNAME=%s\n", seq->f.launcher, seq->f.hostname);
		need_change = True;
	}
	/* FIXME: need to do MONITOR= and COMMAND= */
	if (need_change) {
		CPRINTF(1, c, "[sc] change required to %s\n", seq->f.id);
		if (options.assist)
			send_change(seq);
	}

	/* Perform some checks on the information available in .desktop file vs. that
	   which was discovered when the window was launched. */

	if (!(e = seq->e))
		return;

	if (seq->state == StartupNotifyComplete) {
		if (msg_from_wm(seq->remover)) {
			CPRINTF(1, c, "[sc] window manager completed sequence %s\n", seq->f.id);
			if (e->StartupNotify && !strcmp(e->StartupNotify, "true")) {
				OPRINTF(0, "remove \"StartupNotify=%s\" from %s\n", e->StartupNotify, e->path);
			}
			if (c->ch.res_class && (!e->StartupWMClass || strcmp(e->StartupWMClass, c->ch.res_class))) {
				OPRINTF(0, "add \"StartupWMClass=%s\" to %s\n", c->ch.res_class, e->path);
			}
		} else if (msg_from_la(seq->remover)) {
			CPRINTF(1, c, "[sc] launch assistant completed sequence %s\n", seq->f.id);
			if (e->StartupNotify && !strcmp(e->StartupNotify, "true")) {
				OPRINTF(0, "remove \"StartupNotify=%s\" from %s\n", e->StartupNotify, e->path);
			}
			if (c->ch.res_class && (!e->StartupWMClass || strcmp(e->StartupWMClass, c->ch.res_class))) {
				OPRINTF(0, "add \"StartupWMClass=%s\" to %s\n", c->ch.res_class, e->path);
			}
		} else if (msg_from_me(seq->remover)) {
			CPRINTF(1, c, "[sc] completed sequence %s\n", seq->f.id);
			if (e->StartupNotify && !strcmp(e->StartupNotify, "true")) {
				OPRINTF(0, "remove \"StartupNotify=%s\" from %s\n", e->StartupNotify, e->path);
			}
			if (c->ch.res_class && (!e->StartupWMClass || strcmp(e->StartupWMClass, c->ch.res_class))) {
				OPRINTF(0, "add \"StartupWMClass=%s\" to %s\n", c->ch.res_class, e->path);
			}
		} else if (msg_from_ap(seq->remover, c->win) || msg_from_ap(seq->from, c->win)) {
			CPRINTF(1, c, "[sc] application completed sequence %s\n", seq->f.id);
			if (!e->StartupNotify || strcmp(e->StartupNotify, "true")) {
				OPRINTF(0, "add \"StartupNotify=true\" to %s\n", e->path);
			}
		} else {
			CPRINTF(1, c, "[sc] unknown window 0x%08lx completed sequence %s\n", seq->from, seq->f.id);
			if (c->ch.res_class && (!e->StartupWMClass || strcmp(e->StartupWMClass, c->ch.res_class))) {
				OPRINTF(0, "add \"StartupWMClass=%s\" to %s\n", c->ch.res_class, e->path);
			}
		}
	}

	if (is_dockapp(c)) {
		if (!e->Categories || !strstr(e->Categories, "DockApp")) {
			OPRINTF(0, "add \"Categories=DockApp\" to %s\n", e->path);
		}
		if (options.autostart) {
			if (e->AutostartPhase
			    && strncmp(e->AutostartPhase, "Applications",
				       strlen(e->AutostartPhase))) {
				OPRINTF(0, "remove \"AutostartPhase=%s\" from %s\n",
					e->AutostartPhase, e->path);
			}
		}
	} else {
		if (e->Categories && strstr(e->Categories, "DockApp")) {
			OPRINTF(0, "remove \"Categories=DockApp\" from %s\n", e->path);

		}
	}
	if (is_trayicon(c)) {
		if (!e->Categories || !strstr(e->Categories, "TrayIcon")) {
			OPRINTF(0, "add \"Categories=TrayIcon\" to %s\n", e->path);
		}
		if (options.autostart) {
			if (e->AutostartPhase
			    && strncmp(e->AutostartPhase, "Applications",
				       strlen(e->AutostartPhase))) {
				OPRINTF(0, "remove \"AutostartPhase=%s\" from %s\n",
					e->AutostartPhase, e->path);
			}
		}
	} else {
		if (e->Categories && strstr(e->Categories, "TrayIcon")) {
			OPRINTF(0, "remove \"Categories=TrayIcon\" from %s\n", e->path);
		}
	}
}

static void
change_client(Client *c)
{
	Sequence *seq;

	if (!(seq = c->seq)) {
		CPRINTF(1, c, "[cc] called too soon!\n");
		return;
	}
	if (!c->counted) {
		if (c->managed && c->mapped) {
			c->counted = True;
#if 0
			if (options.toolwait) {
				CPRINTF(1, c, "[cc] testing toolwait mappings=%d\n",
					options.mappings);
				if (--options.mappings == 0) {
					CPRINTF(1, c, "[sc] mappings done!\n");
					running = False;
				} else
					CPRINTF(1, c, "[sc] toolwait more mappings=%d\n",
						options.mappings);
			} else if (options.assist) {
				CPRINTF(1, c, "[cc] assistance done!\n");
				running = False;
			}
			if (!running) {
#endif
				switch (seq->state) {
				case StartupNotifyIdle:
				case StartupNotifyComplete:
					break;
				case StartupNotifyNew:
				case StartupNotifyChanged:
					if (seq->f.wmclass || seq->n.silent) {
						/* We are not expecting that the client
						   will generate startup notification
						   completion on its own.  Either we
						   generate the completion or wait for a
						   supporting window manager to do so. */
						if (options.assist)
							send_remove(seq);
					} else {
						/* We are expecting that the client will
						   generate startup notification
						   completion on its own, so let the
						   timers run and wait for completion. */
					}
					break;
				}
#if 0
				CPRINTF(1, c, "[cc] --- SHUT IT DOWN ---\n");
			}
#endif
		} else
			setup_client(c);
	}
	if (c->need_change) {
		c->need_change = False;
		need_changes--;
	}
}

/** @brief detect whether a client is a dockapp
  * @param c - client the check
  * @return Bool - true if the client is a dockapp; false, otherwise.
  */
static Bool
is_dockapp(Client *c)
{
	if (!c->wmh)
		return False;
	/* Many libxpm based dockapps use xlib to set the state hint correctly. */
	if ((c->wmh->flags & StateHint) && c->wmh->initial_state == WithdrawnState)
		return True;
	/* Many dockapps that were based on GTK+ < 2.4.0 are having their initial_state
	   changed to NormalState by GTK+ >= 2.4.0, so when the other flags are set,
	   accept anyway (note that IconPositionHint is optional). */
	if ((c->wmh->flags & ~IconPositionHint) == (StateHint | IconWindowHint | WindowGroupHint))
		return True;
	if (!c->ch.res_class)
		return False;
	/* In an attempt to get around GTK+ >= 2.4.0 limitations, some GTK+ dock apps
	   simply set the res_class to "DockApp". */
	if (!strcmp(c->ch.res_class, "DockApp"))
		return True;
	return False;
}

/** @brief detect whether a client is a status icon
  */
static Bool
is_trayicon(Client *c)
{
	if (c->xei)
		return True;
	return False;
}

static void
retest_client(Client *c)
{
	Sequence *seq;

	PTRACE(5);
	if (c->seq) {
		if (c->need_retest) {
			CPRINTF(1, c, "[rc] false alarm!\n");
			need_retests--;
			c->need_retest = False;
		}
		return;
	}
	if (!c->need_retest) {
		CPRINTF(1, c, "[rc] false alarm!\n");
		return;
	}
	for (seq = sequences; seq; seq = seq->next) {
		if (test_client(c, seq)) {
			CPRINTF(1, c, "[rc] --------------!\n");
			CPRINTF(1, c, "[rc] IS THE ONE(tm)!\n");
			CPRINTF(1, c, "[rc] --------------!\n");
			c->seq = ref_sequence(seq);
			seq->client = c->win;
			change_client(c);
			break;
		}
	}
	if (c->need_retest) {
		need_retests--;
		c->need_retest = False;
	}
}

const char *
show_state(int state)
{
	switch (state) {
	case WithdrawnState:
		return ("WithdrawnState");
	case NormalState:
		return ("NormalState");
	case IconicState:
		return ("IconicState");
	case ZoomState:
		return ("ZoomState");
	case InactiveState:
		return ("InactiveState");
	}
	return ("UnknownState");
}

const char *
show_atoms(Atom *atoms, int n)
{
	static char buf[BUFSIZ + 1] = { 0, };
	char *name;
	int i;

	buf[0] = '\0';
	for (i = 0; i < n; i++) {
		if (i)
			strncat(buf, ",", BUFSIZ);
		strncat(buf, " ", BUFSIZ);
		if ((name = XGetAtomName(dpy, atoms[i]))) {
			strncat(buf, name, BUFSIZ);
			XFree(name);
		} else
			strncat(buf, "???", BUFSIZ);
	}
	return (buf);
}

const char *
show_command(char *argv[], int argc)
{
	static char buf[BUFSIZ + 1] = { 0, };
	int i;

	buf[0] = '\0';
	for (i = 0; i < argc; i++) {
		strncat(buf, " '", BUFSIZ);
		strncat(buf, argv[i], BUFSIZ);
		strncat(buf, "'", BUFSIZ);
	}
	return (buf);
}

/** @brief check a newly created client (top-level) window
  * @param c - client structure pointer
  *
  * Updates the client from information and properties maintained by the X
  * server.  Care should be taken that information that should be obtained from
  * a group window is obtained from that window first and then overwritten by
  * any information contained in the specific window.
  *
  * Client structures are added when a new top-level window is created.  The
  * window may or may not have anything to do with the client we in which we are
  * interested.  The purpose of this procedure is to acquire information on
  * properties that may have been set after the window was created, but before
  * we had a chance to select input on the window.  This way we do not have to
  * grab the server.
  *
  * Because we may want to alter properties before the window is managed by the
  * window manager, we collect only the information that is necessary to
  * identify the window structure and determine the startup sequence to which
  * the window belongs.  We interleave the first check of sequence with updating
  * so that we can hit it fastest.
  */
static void
update_client(Client *c)
{
	long card = 0, n = -1, i;
	Window win = None;
	Time time = CurrentTime;
	Atom *atoms = NULL;
	long mask =
	    ExposureMask | VisibilityChangeMask | StructureNotifyMask |
	    SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask;
	Client *tmp = NULL;
	Sequence *seq;

	if (!c->need_update) {
		CPRINTF(1, c, "[uc] false alarm!\n");
		return;
	}
	need_retest(c);

	CPRINTF(1, c, "[uc] updating client\n");
	/* WM_HINTS: two things in which we are intrested is the group window (if any)
	   and whether the client is a dock app or not. */
	if (!c->wmh && (c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint) {
			win = c->wmh->window_group;
			CPRINTF(1, c, "[uc] WM_HINTS window_group = 0x%08lx\n", win);
			add_group(c, win, WindowGroup);
		}
		c->icon_win = c->win;
		if (c->wmh->flags & IconWindowHint) {
			if ((win = c->wmh->icon_window)) {
				c->icon_win = win;
				CPRINTF(1, c, "[uc] WM_HINTS icon_window = 0x%08lx\n", c->icon_win);
			}
			if (XFindContext(dpy, c->icon_win, ClientContext, (XPointer *) &tmp)) {
				CPRINTF(1, c, "[uc] WM_HINTS icon_window = 0x%08lx (new)\n", c->icon_win);
				XSaveContext(dpy, c->icon_win, ScreenContext, (XPointer) scr);
				XSaveContext(dpy, c->icon_win, ClientContext, (XPointer) c);
				XSelectInput(dpy, c->icon_win, mask);
			}
		}
		if ((c->dockapp = is_dockapp(c)))
			CPRINTF(1, c, "[uc] is a dock app\n");
		else
			CPRINTF(1, c, "[uc] is not a dock app\n");
	}
	/* WM_CLIENT_LEADER: determine whether there is a client leader window. */
	if (get_window((win = c->win), _XA_WM_CLIENT_LEADER, XA_WINDOW, &win)) {
		CPRINTF(1, c, "[uc] WM_CLIENT_LEADER = 0x%08lx\n", win);
		add_group(c, win, ClientGroup);
	}
	/* WM_TRANSIENT_FOR: determine the window for which this one is transient */
	if (get_window((win = c->win), XA_WM_TRANSIENT_FOR, AnyPropertyType, &win)) {
		CPRINTF(1, c, "[uc] WM_TRANSIENT_FOR = 0x%08lx\n", win);
		add_group(c, win, TransiGroup);
	}
	/* _NET_WM_USER_TIME_WINDOW: determine if there is a separate user time window. */
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)) {
		CPRINTF(1, c, "[uc] _NET_WM_USER_TIME_WINDOW = 0x%08lx\n", c->time_win);
		if (XFindContext(dpy, c->time_win, ClientContext, (XPointer *) &tmp)) {
			CPRINTF(1, c, "[uc] _NET_WM_USER_TIME_WINDOW = 0x%08lx (new)\n", c->time_win);
			XSaveContext(dpy, c->time_win, ScreenContext, (XPointer) scr);
			XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->time_win, mask);
		}
	}
	/* _NET_STARTUP_ID */
	if ((c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID))) {
		CPRINTF(1, c, "[uc] _NET_STARTUP_ID = \"%s\"\n", c->startup_id);
		if (!c->seq) {
			for (seq = sequences; seq; seq = seq->next) {
				if (!seq->f.id)
					continue;	/* safety */
				if (!strcmp(c->startup_id, seq->f.id)) {
					CPRINTF(1, c, "[uc] found sequence by _NET_STARTUP_ID\n");
					assist_client(c, seq);
					break;
				}
			}
		}
	}
	/* WM_CLIENT_MACHINE: determine the host on which the client runs */
	if ((c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)))
		CPRINTF(1, c, "[uc] WM_CLIENT_MACHINE = \"%s\"\n", c->hostname);
	/* _NET_WM_PID: determine whether a pid is associated with the client. */
	if (get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card) && card) {
		char *str;

		c->pid = card;
		CPRINTF(1, c, "[uc] _NET_WM_PID = %d\n", c->pid);
		if (!c->seq && c->hostname) {
			for (seq = sequences; seq; seq = seq->next) {
				if (!seq->f.pid || c->pid != seq->n.pid)
					continue;
				if (!seq->f.hostname)
					continue;
				if (samehost(seq->f.hostname, c->hostname)) {
					CPRINTF(1, c, "[uc] found sequence by _NET_WM_PID\n");
					assist_client(c, seq);
					break;
				}
			}
		}
		if (!c->seq && islocalhost(c->hostname) && (str = get_proc_startup_id(c->pid))) {
			for (seq = sequences; seq; seq = seq->next) {
				if (!seq->f.id)
					continue;
				if (!strcmp(seq->f.id, str)) {
					CPRINTF(1, c, "[uc] found sequence by _NET_WM_PID\n");
					assist_client(c, seq);
					break;
				}
			}
		}
	}
	/* WM_CLASS: determine the resource name and class */
	if (XGetClassHint(dpy, c->win, &c->ch)) {
		CPRINTF(1, c, "[uc] WM_CLASS = (%s,%s)\n", c->ch.res_name, c->ch.res_class);
		if (!c->seq && (c->ch.res_name || c->ch.res_class)) {
			for (seq = sequences; seq; seq = seq->next) {
				if (!seq->f.wmclass)
					continue;
				if ((c->ch.res_name &&
				     !strcasecmp(seq->f.wmclass, c->ch.res_name)) ||
				    (c->ch.res_class &&
				     !strcasecmp(seq->f.wmclass, c->ch.res_class))) {
					CPRINTF(1, c, "[uc] found sequence by WM_CLASS\n");
					assist_client(c, seq);
					break;
				}
			}
		}
	}
	/* _NET_WM_USER_TIME */
	if (get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
		CPRINTF(1, c, "[uc] _NET_WM_USER_TIME = %lu\n", time);
		pushtime(&c->user_time, time);
		pushtime(&last_user_time, c->user_time);
		if (!c->seq && c->user_time) {
			for (seq = sequences; seq; seq = seq->next) {
				if (!seq->f.timestamp)
					continue;
				if (c->user_time == seq->n.timestamp) {
					CPRINTF(1, c, "[uc] found sequence by _NET_WM_USER_TIME\n");
					assist_client(c, seq);
					break;
				}
			}
		}
	}
	/* WM_COMMAND */
	if (XGetCommand(dpy, c->win, &c->command, &c->count)) {
		CPRINTF(1, c, "[uc] WM_COMMAND = %s\n", show_command(c->command, c->count));
		if (!c->seq && c->command) {
			for (seq = sequences; seq; seq = seq->next) {
				wordexp_t we = { 0, };

				if (!seq->f.command)
					continue;
				if (!wordexp(seq->f.command, &we, 0)) {
					if (we.we_wordc == c->count) {
						int i;

						for (i = 0; i < c->count; i++)
							if (strcmp(we.we_wordv[i], c->command[i]))
								break;
						if (i == c->count) {
							CPRINTF(1, c,
								"[uc] found sequence by WM_COMMAND\n");
							assist_client(c, seq);
							wordfree(&we);
							break;
						}
					}
					wordfree(&we);
				}
			}
		}

	}
	/* _NET_STARTUP_INFO BIN= field */
	if (!c->seq && c->pid && c->hostname && islocalhost(c->hostname)) {
		for (seq = sequences; seq; seq = seq->next) {
			char *str;

			if (!seq->f.bin)
				continue;
			if ((str = get_proc_comm(c->pid)) && !strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[uc] found sequence by _NET_STARTUP_ID binary\n");
				free(str);
				assist_client(c, seq);
				break;
			}
			free(str);
			if ((str = get_proc_exec(c->pid)) && !strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[uc] found sequence by _NET_STARTUP_ID binary\n");
				free(str);
				assist_client(c, seq);
				break;
			}
			free(str);
			if ((str = get_proc_argv0(c->pid)) && !strcmp(seq->f.bin, str)) {
				CPRINTF(1, c, "[uc] found sequence by _NET_STARTUP_ID binary\n");
				free(str);
				assist_client(c, seq);
				break;
			}
			free(str);
		}
	}
	/* The remaining property gets are used to populate other information about the
	   state of the client the must be refreshed after selecting for input on the
	   window. */

	/* WM_STATE */
	if ((c->wms = XGetWMState(dpy, c->win))) {
		if (c->wms->state != WithdrawnState || c->dockapp) {
			c->managed = True;
			c->mapped = True;
			CPRINTF(1, c, "[uc] WM_STATE = %s, 0x%08lx\n", show_state(c->wms->state),
				c->wms->icon_window);
		}
	}
	/* WM_NAME */
	if (XFetchName(dpy, c->win, &c->name))
		CPRINTF(1, c, "[uc] WM_NAME = \"%s\"\n", c->name);
	/* _NET_WM_STATE */
	if ((atoms = get_atoms(c->win, _XA_NET_WM_STATE, XA_ATOM, &n))) {
		for (i = 0; i < n; i++) {
			if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
				CPRINTF(1, c, "[uc] is focussed\n");
				c->managed = True;
				c->mapped = True;
				break;
			}
		}
	}
	/* _NET_WM_ALLOWED_ACTIONS */
	/* _NET_WM_VISIBLE_NAME */
	/* _NET_WM_VISIBLE_ICON_NAME */
	/* WM_NAME */
	/* WM_WINDOW_ROLE */
	/* _NET_WM_NAME */
	/* _NET_WM_ICON_NAME */
	/* _NET_WM_DESKTOP or _WIN_WORKSPACE */
	if (get_cardinal(c->win, _XA_NET_WM_DESKTOP, AnyPropertyType, &card))
		c->desktop = card + 1;
	if (!c->desktop)
		if (get_cardinal(c->win, _XA_WIN_WORKSPACE, AnyPropertyType, &card))
			c->desktop = card + 1;

	if (c->need_update) {
		c->need_update = False;
		need_updates--;
	}
}

static Client *
add_client(Window win)
{
	long mask =
	    ExposureMask | VisibilityChangeMask | StructureNotifyMask |
	    SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask;
	Client *c;

	PTRACE(5);
	if ((c = calloc(1, sizeof(*c)))) {
		c->win = win;
		c->icon_win = win;
		c->time_win = win;
		c->next = clients;
		clients = c;
		XSaveContext(dpy, win, ScreenContext, (XPointer) scr);
		XSaveContext(dpy, win, ClientContext, (XPointer) c);
		XSelectInput(dpy, win, mask);
		need_update(c);
	}
	return (c);
}

static Client *
get_client(Window win)
{
	Client *c = NULL;

	if (XFindContext(dpy, win, ClientContext, (XPointer *) &c))
		c = add_client(win);
	return (c);
}

static void
show_entry(const char *prefix, Entry *e)
{
	const char **label;
	char **entry;

	if (options.debug < 2 && options.output < 2)
		return;
	if (prefix)
		fprintf(stderr, "\n%s from file %s\n", prefix, e->path);
	for (label = DesktopEntryFields, entry = &e->Type; *label; label++, entry++)
		if (*entry)
			fprintf(stderr, "%-24s = %s\n", *label, *entry);
	if (prefix)
		fputs("\n", stderr);
	fflush(stderr);
}

static void
info_entry(const char *prefix, Entry *e)
{
	const char **label;
	char **entry;

	if (options.output < 1)
		return;
	if (prefix)
		fprintf(stdout, "%s: from %s\n", prefix, e->path);
	for (label = DesktopEntryFields, entry = &e->Type; *label; label++, entry++)
		if (*entry)
			fprintf(stdout, "%-24s = %s\n", *label, *entry);
	if (prefix)
		fputs("\n", stdout);
	fflush(stdout);
}

static void
free_entry(Entry *e)
{
	const char **label;
	char **entry;

	if (!e)
		return;
	for (label = DesktopEntryFields, entry = &e->Type; *label; label++, entry++) {
		free(*entry);
		*entry = NULL;
	}
	free(e->path);
	e->path = NULL;
	if (e != &myent)
		free(e);
}

static Sequence *unref_sequence(Sequence *seq);
static Bool msg_from_wm(Window w);
static Bool msg_from_la(Window w);
static Bool msg_from_me(Window w);
static Bool msg_from_ap(Window w, Window c);

static const char *
show_source(Window w, Window c)
{
	if (msg_from_wm(w))
		return ("window manager");
	if (msg_from_la(w))
		return ("launch assistant");
	if (msg_from_me(w))
		return (SELECTION_ATOM);
	if (msg_from_ap(w, c))
		return ("application");
	return ("unknown source");
}

static void
show_sequence(const char *prefix, Sequence *seq)
{
	const char **label;
	char **field;

	if (options.debug < 2 && options.output < 2)
		return;
	if (prefix)
		fprintf(stderr, "%s: from 0x%08lx (%s)\n", prefix, seq->from, show_source(seq->from, seq->client));
	for (label = StartupNotifyFields, field = seq->fields; *label; label++, field++) {
		if (*field)
			fprintf(stderr, "%s=%s\n", *label, *field);
		else
			fprintf(stderr, "%s=\n", *label);
	}
	fflush(stderr);
}

static void
info_sequence(const char *prefix, Sequence *seq)
{
	const char **label;
	char **field;

	if (options.output < 1)
		return;
	if (prefix)
		fprintf(stdout, "%s: from 0x%08lx (%s)\n", prefix, seq->from, show_source(seq->from, seq->client));
	for (label = StartupNotifyFields, field = seq->fields; *label; label++, field++) {
		if (*field)
			fprintf(stdout, "%s=%s\n", *label, *field);
		else
			fprintf(stdout, "%s=\n", *label);
	}
	fflush(stdout);
}

static Sequence *remove_sequence(Sequence *del);

/** @brief update client from startup notification sequence
  * @param seq - the sequence that has changed state
  *
  * Update the client associated with a startup notification sequence.
  */
static void
update_startup_client(Sequence *seq)
{
	Client *c;

	if (!seq->f.id) {
		EPRINTF("Sequence without id!\n");
		return;
	}
	if (myseq.f.id) {
		if (strcmp(seq->f.id, myseq.f.id)) {
			DPRINTF(1, "Sequence for unrelated id %s\n", seq->f.id);
			remove_sequence(seq);
			return;
		}
		OPRINTF(1, "Related sequence received:\n");
		switch (seq->state) {
		case StartupNotifyIdle:
			break;
		case StartupNotifyNew:
			if (options.output > 1)
				show_sequence("New sequence:", seq);
			if (options.info)
				info_sequence("New sequence:", seq);
			break;
		case StartupNotifyChanged:
			if (options.output > 1)
				show_sequence("Change sequence:", seq);
			if (options.info)
				info_sequence("Change sequence:", seq);
			break;
		case StartupNotifyComplete:
			if (options.output > 1)
				show_sequence("Completed sequence:", seq);
			if (options.info)
				info_sequence("Completed sequence:", seq);
			if (options.toolwait || options.assist) {
				OPRINTF(1, "Startup notification complete!\n");
				running = False;
			}
			break;
		}
	}
	for (c = clients; c; c = c->next)
		need_retest(c);
	return;
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

	old->from = new->from;
	for (i = 0; i < sizeof(old->f) / sizeof(char *); i++) {
		if (new->fields[i]) {
			free(old->fields[i]);
			old->fields[i] = new->fields[i];
		}
	}
	convert_sequence_fields(old);
	if (options.output > 1)
		show_sequence("Updated sequence fields", old);
	if (options.info)
		info_sequence("Updated sequence fields", old);
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
close_sequence(Sequence *seq)
{
#ifdef GIO_GLIB2_SUPPORT
	if (seq->timer) {
		DPRINTF(1, "removing timer\n");
		g_source_remove(seq->timer);
		seq->timer = 0;
	}
#endif				/* GIO_GLIB2_SUPPORT */
}

static Sequence *
unref_sequence(Sequence *seq)
{
	if (seq) {
		if (--seq->refs <= 0) {
			Sequence *s, **prev;

			DPRINTF(1, "deleting sequence\n");
			for (prev = &sequences, s = *prev; s && s != seq;
			     prev = &s->next, s = *prev) ;
			if (s) {
				*prev = s->next;
				s->next = NULL;
			}
			close_sequence(seq);
			free_sequence_fields(seq);
			free_entry(seq->e);
			seq->e = NULL;
			if (seq != &myseq)
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
remove_sequence(Sequence *seq)
{
	if (seq->removed) {
		OPRINTF(1, "==> originally removed by 0x%08lx (%s)\n", seq->remover,
				show_source(seq->remover, seq->client));
		OPRINTF(1, "==> redundant  removal by 0x%08lx (%s)\n", seq->from,
				show_source(seq->from, seq->client));
		return (NULL);
	}
	if (options.output > 1)
		show_sequence("Removing sequence", seq);
	if (options.info)
		info_sequence("Removing sequence", seq);
	seq->removed = True;
	seq->remover = seq->from;
	return unref_sequence(seq);
}

#ifdef GIO_GLIB2_SUPPORT

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
	if (options.assist) {
		send_remove(seq);
		seq->timer = 0;
		return FALSE;	/* remove timeout source */
	}
	return TRUE;	/* continue timeout interval */
}

#endif				/* GIO_GLIB2_SUPPORT */

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
	seq->client = None;
	seq->next = sequences;
	sequences = seq;
	if (!seq->e && islocalhost(seq->f.hostname)) {
		char *path, *appid;

		if (!(appid = seq->f.application_id) && seq->f.bin) {
			appid = strrchr(seq->f.bin, '/');
			appid = appid ? appid + 1 : seq->f.bin;
		}
		if ((path = lookup_file(appid))) {
			DPRINTF(1, "found desktop file %s\n", path);
			Entry *e = calloc(1, sizeof(*e));

			if (!parse_file(path, e))
				free_entry(e);
			else {
				seq->e = e;
				if (options.output > 1)
					show_entry("Parsed entries", e);
				if (options.info)
					info_entry("Parsed entries", e);
			}
			free(path);
		}
	}
#ifdef GIO_GLIB2_SUPPORT
	seq->timer = g_timeout_add(options.guard, sequence_timeout_callback, (gpointer) seq);
#endif				/* GIO_GLIB2_SUPPORT */
	if (options.output > 1)
		show_sequence("Added sequence", seq);
	if (options.info)
		info_sequence("Added sequence", seq);
}

static void
process_startup_msg(Message *m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q, *copy, *b;
	const char **f;
	int i;
	int escaped, quoted;

	cmd.from = m->origin;
	DPRINTF(1, "Got message from 0x%08lx (%s): %s\n", m->origin,
		show_source(cmd.from, cmd.client), m->data);
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
		if (!cmd.f.bin)
			cmd.f.bin = strdup(cmd.f.launchee);
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
			update_startup_client(seq);
			return;
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
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
			update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyComplete:
		/* remove sequence */
		break;
	}
	/* remove sequence */
	copy_sequence_fields(seq, &cmd);
	update_startup_client(seq);
	remove_sequence(seq);
}

static void
cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	PTRACE(5);
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
managed_client(Client *c, Time time)
{
	PTRACE(5);
	if (!c) {
		EPRINTF("null client!\n");
		return;
	}
	if (!c->managed) {
		c->managed = True;
		pulltime(&c->map_time, time ? : c->last_time);
		need_retest(c);
		CPRINTF(1, c, "[mc] managed\n");
	}
}

static void
mapped_client(Client *c, Time time)
{
	PTRACE(5);
	if (!c) {
		EPRINTF("null client!\n");
		return;
	}
	managed_client(c, time);
	if (!c->mapped) {
		c->mapped = True;
		pulltime(&c->map_time, time ? : c->last_time);
		need_retest(c);
		CPRINTF(1, c, "[mc] mapped\n");
	}
}

static void
unmanaged_client(Client *c)
{
	if (c->managed) {
		c->wasmanaged = True;
		c->managed = False;
	}
	/* FIXME: we can remove the client and any associated startup notification. */
}

static void
listed_client(Client *c, Time time)
{
	PTRACE(5);
	if (!c) {
		EPRINTF("null client!\n");
		return;
	}
	mapped_client(c, time);
	if (!c->listed) {
		c->listed = True;
		pulltime(&c->list_time, time ? : c->last_time);
		need_retest(c);
		CPRINTF(1, c, "[lc] listed\n");
	}
}

static void
remove_client(Client *c)
{
	Window *winp;
	int i;

	PTRACE(5);
	if (c->startup_id) {
		DPRINTF(5, "Freeing: startup_id\n");
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (c->command) {
		DPRINTF(5, "Freeing: command\n");
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (c->name) {
		DPRINTF(5, "Freeing: name\n");
		XFree(c->name);
		c->name = NULL;
	}
	if (c->hostname) {
		DPRINTF(5, "Freeing: hostname\n");
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (c->client_id) {
		DPRINTF(5, "Freeing: client_id\n");
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (c->role) {
		DPRINTF(5, "Freeing: role\n");
		XFree(c->role);
		c->role = NULL;
	}
	if (c->ch.res_name) {
		DPRINTF(5, "Freeing: ch.res_name\n");
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		DPRINTF(5, "Freeing: ch.res_class\n");
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (c->wmh) {
		DPRINTF(5, "Freeing: wmh\n");
		XFree(c->wmh);
		c->wmh = NULL;
	}
	if (c->wms) {
		DPRINTF(5, "Freeing: wms\n");
		XFree(c->wms);
		c->wms = NULL;
	}
	if (c->xei) {
		DPRINTF(5, "Freeing: xei\n");
		XFree(c->xei);
		c->xei = NULL;
	}
	for (i = 0, winp = &c->win; i < 3; i++, winp++) {
		if (*winp) {
			XDeleteContext(dpy, *winp, ScreenContext);
			XDeleteContext(dpy, *winp, ClientContext);
			XSelectInput(dpy, *winp, NoEventMask);
			*winp = None;
		}
	}
	del_group(c, TransiGroup);
	del_group(c, WindowGroup);
	del_group(c, ClientGroup);
	if (c->seq) {
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
	for (cp = &clients, c = *cp; c && c != r; cp = &c->next, c = *cp) ;
	if (c == r)
		*cp = c->next;
	remove_client(r);
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
	Client *c;

	PTRACE(5);
	if ((list = get_windows(scr->root, atom, type, &n))) {
		for (i = 0; i < n; i++)
			if ((c = find_client(list[i])))
				listed_client(c, e ? e->time : CurrentTime);
		XFree(list);
	}
}

/** @brief track the active window
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the _NET_ACTIVE_WINDOW property on the root window set
  * by EWMH/NetWM compliant window managers to indicate the active window.  We
  * keep track of the last time that each client window was active.  If a window
  * appears in this property it has become mapped and managed.
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
			mapped_client(c, e ? e->time : CurrentTime);
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
	if (c->mapped)
		return;
	CPRINTF(1, c, "[cm] _NET_ACTIVE_WINDOW for unmanaged window 0x%08lx\n", e->window);
	mapped_client(c, e->data.l[1]);
}

static void
pc_handle_NET_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static void
pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
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
	CPRINTF(1, c, "[cm] _NET_CLOSE_WINDOW for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, time);
}

static void
cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_MOVERESIZE_WINDOW for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
}

/** @brief handle _NET_REQUEST_FRAME_EXTENTS ClientMessage event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This message, unlike others, is sent before a window is initially mapped
  * (managed).  A client can send this message before a window is mapped to ask
  * the window manager to set the _NET_FRAME_EXTENTS property for the window.
  * We log this fact so that we can tell whether the window was managed when the
  * _NET_FRAME_EXTENTS property is added to the window.
  */
static void
cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c)
		return;
	if (c->managed)
		CPRINTF(1, c, "[cm] _NET_REQUEST_FRAME_EXTENTS for managed window 0x%08lx\n", e->window);
	c->request = True;
}

/** @brief handle _NET_FRAME_EXTENTS PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * The window manager sets this property, either when the window is managed or
  * before it is managed when it has received a _NET_REQUEST_FRAME_EXTENTS for
  * the window.  So, because of the timing, we cannot know that this property
  * being set means anything unless we also track _NET_REQUEST_FRAME_EXTENTS for
  * the client.
  */
static void
pc_handle_NET_FRAME_EXTENTS(XPropertyEvent *e, Client *c)
{
	long n = 0, *extents = NULL;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "[pc] _NET_FRAME_EXTENTS = (deleted)\n");
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	if ((extents = get_cardinals(c->win, _XA_NET_FRAME_EXTENTS, AnyPropertyType, &n))) {
		CPRINTF(1, c, "[pc] _NET_FRAME_EXTENTS = (%ld, %ld, %ld, %ld)\n",
			extents[0], extents[1], extents[2], extents[3]);
		XFree(extents);
		if (!c->managed && !c->request)
			managed_client(c, e ? e->time : CurrentTime);
		c->request = False;
	}
}

static void
cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_RESTACK_WINDOW for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
}

/** @brief handle _NET_STARTUP_ID PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * _NET_STARTUP_ID is placed on a primary window (often the group leader window
  * from WM_HINTS or the session leader window from WM_CLIENT_LEADER) and can be
  * used by the window manager to correlate the startup notification sequence
  * (received in ClientMessage events) with the application windows.  Also, some
  * information (launcher, launchee, binary, sequence and time) is codified into
  * the identifier.  The addition of this property to a window indicates that it
  * either participates fully in startup notification; or, it was assisted by a
  * separate monitor or launcher application.  A recheck is required when the
  * client has not already been assigned a sequence.
  */
static void
pc_handle_NET_STARTUP_ID(XPropertyEvent *e, Client *c)
{
	char *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->startup_id))
		c->startup_id = NULL;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "[pc] _NET_STARTUP_ID = (deleted)\n");
		if (old) {
			XFree(old);
			groups_need_retest(c);
		}
		return;
	}
	c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID);
	if (c->startup_id && (!old || strcmp(old, c->startup_id))) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_STARTUP_ID was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] _NET_STARTUP_ID = \"%s\"\n", c->startup_id);
		groups_need_retest(c);
	}
}

/** @brief handle _NET_WM_ALLOWED_ACTIONS PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property is only set by the window manager, and then only when it has
  * already been (or is about to be) managed.  Setting of this property is an
  * indication that the window manager has managed (or is about to manage) the
  * window.
  */
static void
pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *e, Client *c)
{
	Atom *atoms = NULL;
	long n = 0;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "[pc] _NET_WM_ALLOWED_ACTIONS = (deleted)\n");
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	if (!c->managed) { /* reduce noise */
		if ((atoms = get_atoms(c->win, _XA_NET_WM_ALLOWED_ACTIONS, AnyPropertyType, &n))) {
			CPRINTF(1, c, "[pc]  _NET_WM_ALLOWED_ACTIONS = %s\n", show_atoms(atoms, n));
			XFree(atoms);
			managed_client(c, e ? e->time : CurrentTime);
		}
	}
}

static void
cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_WM_ALLOWED_ACTIONS for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

static void
pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	/* window manager only put this on managed windows */
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (!e || e->state == PropertyDelete)
		return;
	managed_client(c, e ? e->time : CurrentTime);
#endif
}

static void
cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_WM_FULLSCREEN_MONITORS for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

static void
pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	/* pager only put this on managed windows */
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (!e || e->state == PropertyDelete)
		return;
	managed_client(c, e ? e->time : CurrentTime);
#endif
}

/** @brief handle _NET_WM_ICON_NAME PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property is set by the client.  I don't know why I am tracking it.
  */
static void
pc_handle_NET_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_WM_MOVERSIZE for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
}

/** @brief handle _NET_WM_NAME PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property is set by the client.  It can be used to update the
  * DESCRIPTION= field of the startup notification when there is no description
  * in the original notification (which I don't yet).  _NET_WM_NAME should take
  * precedence over WM_NAME.
  */
static void
pc_handle_NET_WM_NAME(XPropertyEvent *e, Client *c)
{
	char *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->name))
		c->name = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_NAME was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] _NET_WM_NAME = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	if (!c->name)
		c->name = get_text(c->win, _XA_NET_WM_NAME);
	if (!c->name)
		c->name = get_text(c->win, XA_WM_NAME);
	if (c->name && (!old || strcmp(old, c->name))) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_NAME was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] _NET_WM_NAME = \"%s\"\n", c->name);
	}
	if (old)
		XFree(old);
}

/** @brief handle _NET_WM_PID PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property identifies the process id (or process id of the primary
  * thread) of the Linux process running the application.  It must be
  * interpreted in conjunction with WM_CLIENT_MACHINE, because the PID is only
  * significant when combined with the full-qualified hosthame.  A recheck is
  * required when this property is added or changes.
  */
static void
pc_handle_NET_WM_PID(XPropertyEvent *e, Client *c)
{
	long pid = 0, old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->pid))
		c->pid = 0;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "[pc] _NET_WM_PID = (deleted)\n");
		return;
	}
	get_cardinal(c->win, _XA_NET_WM_PID, AnyPropertyType, &pid);
	if ((c->pid = pid) && (!old || old != pid)) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_PID was %ld\n", old);
		CPRINTF(1, c, "[pc] _NET_WM_PID = %ld\n", pid);
		groups_need_retest(c);
	}
}

/** @brief handle _NET_WM_STATE PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
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
	long i, n = 0;
	Bool manage = False;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete) {
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	if (!c->managed) {
		if ((atoms = get_atoms(c->win, _XA_NET_WM_STATE, AnyPropertyType, &n))) {
			CPRINTF(1, c, "[pc] _NET_WM_STATE = %s\n", show_atoms(atoms, n));
			for (i = 0; i < n; i++) {
				if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
					if (e)
						pushtime(&c->focus_time, e->time);
					manage = True;
				} else if (atoms[i] == _XA_NET_WM_STATE_HIDDEN)
					manage = True;
			}
			XFree(atoms);
		}
		if (manage)
			managed_client(c, e ? e->time : CurrentTime);
	}
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
	CPRINTF(1, c, "[cm] _NET_WM_STATE sent for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
}

/** @brief handle _NET_WM_USER_TIME_WINDOW PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * _NET_WM_USER_TIME_WINDOW is the window that is used to report user
  * interaction time updates.  It is often placed in a separate window so that
  * a manager can select input on the main window without receiving a large
  * number of updates for _NET_WM_USER_TIME property (which is placed only on
  * the _NET_WM_USER_TIME_WINDOW).  Because this property is primary and
  * structural, a recheck of sequence will have to be done if there is not
  * already a sequence assigned to the client.
  */
static void
pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *e, Client *c)
{
	Window old;
	Time time;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->time_win))
		c->time_win = None;
	if (old == c->win)
		old = None;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_USER_TIME_WINDOW was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] _NET_WM_USER_TIME_WINDOW = (deleted)\n");
		return;
	}
	get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win);
	if (c->time_win == c->win)
		c->time_win = None;
	if (c->time_win && (!old || old != c->time_win)) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_USER_TIME_WINDOW was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] _NET_WM_USER_TIME_WINDOW = 0x%08lx\n", c->time_win);

		XSaveContext(dpy, c->time_win, ScreenContext, (XPointer) scr);
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);

		if (get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
			pushtime(&c->user_time, time);
			pushtime(&last_user_time, time);
			pushtime(&current_time, time);
		}
	}
	if (!c->time_win)
		c->time_win = c->win;
}

/** @brief handle _NET_WM_USER_TIME PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * _NET_WM_USER_TIME is partly input and partly output.  Often, when the client
  * is startup notificaiton aware it initially places this parameter on the
  * window intiialized to the timestamp from the startup notificaiton,   It can
  * be used to match a startup-notification sequence.  A recheck is required
  * when this property is added or changed.  When assisting the window manager
  * and application, this is set to the timestamp of the startup notification
  * identifier.
  */
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
		CPRINTF(5, c, "[pc] _NET_WM_USER_TIME = %ld\n", time);
	}
}

/** @brief handle _NET_WM_VISIBLE_NAME PropertyNotify event
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
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
	CPRINTF(1, c, "[pc] _NET_WM_VISIBLE_NAME set unmanaged window 0x%08lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

/** @brief handle _NET_WM_VISIBLE_ICON_NAME PropertyNotify event
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
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
	CPRINTF(1, c, "[pc] _NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%08lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

/** @brief handle _NET_WM_DESKTOP PropertyNotify event.
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This property property can be set either by the client (before mapping) or
  * the window manager (after mapping).  Therefore it indicates nothing of the
  * management of the client.
  *
  * Under startup notification, the client and window manager should respect the
  * DESKTOP= field in the startup notification (that is, open the window on the
  * desktop that was requested when launched).  Few clients it seems support
  * this properly so the window manager must enforce it or we must enforce it
  * here when the window manager won't.  This can be done by resetting this
  * property when it changes before the window is managed, and forceably moving
  * the window if it is on the wrong desktop when it becomes managed.
  */
static void
pc_handle_NET_WM_DESKTOP(XPropertyEvent *e, Client *c)
{
	long desktop = 0, old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->desktop))
		c->desktop = 0;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_DESKTOP was %ld\n", old - 1);
		CPRINTF(1, c, "[pc] _NET_WM_DESKTOP = (deleted)\n");
		return;
	}
	if (get_cardinal(c->win, _XA_NET_WM_DESKTOP, AnyPropertyType, &desktop))
		c->desktop = desktop + 1;
	if (c->desktop && (!old || old != c->desktop)) {
		if (old)
			CPRINTF(1, c, "[pc] _NET_WM_DESKTOP was %ld\n", old - 1);
		CPRINTF(1, c, "[pc] _NET_WM_DESKTOP = %d\n", c->desktop);
		/* TODO: check if this actually matches sequence if any */
	}
}

static void
cm_handle_NET_WM_DESKTOP(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _NET_WM_DESKTOP for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
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
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _WIN_LAYER for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
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
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] _WIN_STATE for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

/** @brief handle _WIN_WORKSPACE PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This is a GNOME1/WMH property that sets the workspace (desktop) on which the
  * client wants the window to appear when mapped and the workspace that it is
  * actually on when the window is managed.
  *
  * Under startup notification, the client and window manager should respect the
  * DESKTOP= field in the startup notification (that is, open the window on the
  * desktop that was requested when launched).  Few clients it seems support
  * this properly so the window manager must enforce it or we must enforce it
  * here when the window manager won't.  This can be done by resetting this
  * property when it changes before the window is managed, and forceably moving
  * the window if it is on the wrong desktop when it becomes managed.
  *
  * Some window managers still support GNOME1/WMH at the same time as EWMH, so
  * we should not act on all WinWMH properties, but this one we should.
  */
static void
pc_handle_WIN_WORKSPACE(XPropertyEvent *e, Client *c)
{
	long desktop = 0, old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->desktop))
		c->desktop = 0;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] _WIN_WORKSPACE was %ld\n", old - 1);
		CPRINTF(1, c, "[pc] _WIN_WORKSPACE = (deleted)\n");
		return;
	}
	if (get_cardinal(c->win, _XA_WIN_WORKSPACE, AnyPropertyType, &desktop))
		c->desktop = desktop + 1;
	if (c->desktop && (!old || old != c->desktop)) {
		if (old)
			CPRINTF(1, c, "[pc] _WIN_WORKSPACE was %ld\n", old - 1);
		CPRINTF(1, c, "[pc] _WIN_WORKSPACE = %d\n", c->desktop - 1);
		/* TODO: force update the workspace from sequence */
	}
}

static void
cm_handle_WIN_WORKSPACE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[pc] _WIN_WORKSPACE for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

static void
pc_handle_SM_CLIENT_ID(XPropertyEvent *e, Client *c)
{
	char *old;

	/* pointless */
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->client_id))
		c->client_id = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] SM_CLIENT_ID was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] SM_CLIENT_ID = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	c->client_id = get_text(c->win, _XA_SM_CLIENT_ID);
	if (c->client_id && (!old || strcmp(old, c->client_id))) {
		if (old)
			CPRINTF(1, c, "[pc] SM_CLIENT_ID was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] SM_CLIENT_ID = \"%s\"\n", c->client_id);
	}
	if (old)
		XFree(old);
}

/** @brief handle WM_CLASS PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * The WM_CLASS property contains the resource name and resource class
  * associated with the window.  It is set on each and every window.  A recheck
  * is required when the property is added or changed.
  */
static void
pc_handle_WM_CLASS(XPropertyEvent *e, Client *c)
{
	XClassHint old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old.res_name = c->ch.res_name))
		c->ch.res_name = NULL;
	if ((old.res_class = c->ch.res_class))
		c->ch.res_class = NULL;
	if (e && e->state == PropertyDelete) {
		if (old.res_name)
			XFree(old.res_name);
		if (old.res_class)
			XFree(old.res_class);
		return;
	}
	XGetClassHint(dpy, c->win, &c->ch);
	if ((c->ch.res_name && (!old.res_name || strcmp(old.res_name, c->ch.res_name))) ||
	    (c->ch.res_class && (!old.res_class || strcmp(old.res_class, c->ch.res_class)))
	    ) {
		CPRINTF(1, c, "[pc] WM_CLASS = ('%s','%s') %s\n", c->ch.res_name, c->ch.res_class,
			(c->dockapp = is_dockapp(c)) ? "dock app" : "");
		groups_need_retest(c);
	}
	if (old.res_name)
		XFree(old.res_name);
	if (old.res_class)
		XFree(old.res_class);
}

static void
cm_handle_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] WM_CHANGE_STATE for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

/** @brief handle WM_CLIENT_LEADER PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * WM_CLIENT_LEADER is the leader from a session management perspective. It is
  * possibly where you will find WM_COMMAND, WM_CLIENT_MACHINE (not always set
  * per window).  Each window in this group should have its own WM_WINDOW_ROLE.
  * Because this property is primary and structural, a recheck of sequence will
  * have to be done if there is not already a sequence assigned to the client.
  */
static void
pc_handle_WM_CLIENT_LEADER(XPropertyEvent *e, Client *c)
{
	Window old, now = None;
	Client *l;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	CPRINTF(5, c, "[pc] forgetting WM_CLIENT_LEADER\n");
	old = (l = c->leads[ClientGroup]) ? l->win : None;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_CLIENT_LEADER was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] WM_CLIENT_LEADER = (deleted)\n");
		return;
	}
	get_window(c->win, _XA_WM_CLIENT_LEADER, AnyPropertyType, &now);
	if (now && (!old || old != now)) {
		if (old)
			CPRINTF(1, c, "[pc] WM_CLIENT_LEADER was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] WM_CLIENT_LEADER = 0x%08lx\n", now);
		add_group(c, now, ClientGroup);
		groups_need_retest(c);
	}
}

/** @brief handle WM_CLIENT_MACHINE PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * WM_CLIENT_MACHINE identifies the hostname of the machine on which the client
  * is running.  This was an original part of the X11R6 session management and
  * can often appear on the client leader window.  Neverthless, is use is
  * greatly expanded (e.g. used in _NET_WM_PID) and may appear on any window.
  * Because this affects the interpretation of _NET_WM_PID, a recheck is
  * required when this property changes.
  */
static void
pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *e, Client *c)
{
	char *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->hostname))
		c->hostname = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_CLIENT_MACHINE was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_CLIENT_MACHINE = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE);
	if (c->hostname && (!old || strcasecmp(old, c->hostname))) {
		if (old)
			CPRINTF(1, c, "[pc] WM_CLIENT_MACHINE was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_CLIENT_MACHINE = \"%s\"\n", c->hostname);
		groups_need_retest(c);
	}
	if (old)
		XFree(old);
}

/** @brief handle WM_COMMAND PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * WM_COMMAND is part of X11R6 session management and is often set on the
  * WM_CLIENT_LEADER window instead of each application window.  This can differ
  * from the command used by the launcher to start the application (if only
  * becase a binary was 'exec'ed by a shell).  Nevertheless it is primary and a
  * recheck will be done if there is not already a sequence assigned to the
  * client.
  */
static void
pc_handle_WM_COMMAND(XPropertyEvent *e, Client *c)
{
	char *old = NULL, *now = NULL;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->command)
		old = strdup(show_command(c->command, c->count));
	if (c->command) {
		CPRINTF(5, c, "[pc] freeing old WM_COMMAND\n");
		XFreeStringList(c->command);
		c->command = NULL;
		c->count = 0;
	}
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_COMMAND was %s\n", old);
		CPRINTF(1, c, "[pc] WM_COMMAND = (deleted)\n");
		if (old)
			free(old);
		return;
	}
	XGetCommand(dpy, c->win, &c->command, &c->count);
	if (c->command) {
		now = strdup(show_command(c->command, c->count));
		if (!old || strcmp(old, now)) {
			if (old)
				CPRINTF(1, c, "[pc] WM_COMMAND was %s\n", old);
			CPRINTF(1, c, "[pc] WM_COMMAND = %s\n", now);
			groups_need_retest(c);
		}
	}
	if (old)
		free(old);
	if (now)
		free(now);
}

/** @brief handle WM_HINTS PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * WM_HINTS is a primary property that indicates the group structure and the
  * group leader of a set of windows.  When this changes, we may need to recheck
  * for a sequence (if one has not already been assigned.
  */
static void
pc_handle_WM_HINTS(XPropertyEvent *e, Client *c)
{
	Window group = None;
	XWMHints *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->wmh))
		c->wmh = NULL;
	if (e && e->state == PropertyDelete) {
		CPRINTF(1, c, "[pc] WM_HINTS = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	c->wmh = XGetWMHints(dpy, c->win);
	if (c->wmh && (!old || c->wmh->flags != old->flags
		       || c->wmh->window_group != old->window_group)) {
		if (c->wmh->flags & WindowGroupHint)
			group = c->wmh->window_group;
		CPRINTF(1, c, "[pc] WM_HINTS = (0x%08lx) %s\n", group,
			(c->dockapp = is_dockapp(c)) ? "dock app" : "");
		add_group(c, group, WindowGroup);
		groups_need_retest(c);
	}
	if (old)
		XFree(old);
}

static void
pc_handle_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_ICON_SIZE(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

/** @brief handle WM_NAME PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property is set by the client.  It can be used to update the
  * DESCRIPTION= field of the startup notificiation when there is no description
  * in the original notification (which I don't yet).  _NET_WM_NAME should be
  * used in preference.
  */
static void
pc_handle_WM_NAME(XPropertyEvent *e, Client *c)
{
	char *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->name))
		c->name = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_NAME was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_NAME = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	c->name = get_text(c->win, XA_WM_NAME);
	if (c->name && (!old || strcmp(old, c->name))) {
		if (old)
			CPRINTF(1, c, "[pc] WM_NAME was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_NAME = \"%s\"\n", c->name);
	}
	if (old)
		XFree(old);
}

static void
pc_handle_WM_NORMAL_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
pc_handle_WM_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_WM_PROTOCOLS(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] WM_PROTOCOLS for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

static void
pc_handle_WM_SIZE_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
}

static void
cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
}

/** @brief handle WM_STATE PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This handler tracks the WM_STATE of the client.  If the client was added on
  * CreateNotify of its top-level window it can be addressed now.  If the state
  * is WithdrawnState and the client is not a dockapp, the state is
  * transitioning in the wrong direction.  Otherwise, the window has (or is
  * about to be) mapped and we can check startup notification completion.
  *
  * WM_STATE is placed on a window by a window manager to indicate the managed
  * state of the window.  All ICCCM 2.0 compliant window managers do this.  The
  * appearance of this property indicates that the window is managed by the
  * window manager; its remove, that it is unmanaged.  Our view of the state of
  * the client is changed accordingly.
  */
static void
pc_handle_WM_STATE(XPropertyEvent *e, Client *c)
{
	XWMState *old;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->wms))
		c->wms = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_STATE was %s, 0x%08lx\n", show_state(old->state),
				old->icon_window);
		CPRINTF(1, c, "[pc] WM_STATE = (deleted)\n");
		if (old)
			XFree(old);
		unmanaged_client(c);
		return;
	}
	c->wms = XGetWMState(dpy, c->win);
	if (c->wms
	    && (!old || old->state != c->wms->state || old->icon_window != c->wms->icon_window)) {
		if (old)
			CPRINTF(1, c, "[pc] WM_STATE was %s, 0x%08lx\n", show_state(old->state),
				old->icon_window);
		CPRINTF(1, c, "[pc] WM_STATE = %s, 0x%08lx\n", show_state(c->wms->state),
			c->wms->icon_window);
		if (c->wms->state == WithdrawnState) {
			/* The window manager placed a WM_STATE of WithdrawnState on the
			   window which probably means that it is a dock app that was
			   just managed, but only for WindowMaker work-alikes. Otherwise, 
			   per ICCCM, placing withdrawn state on the window means that it 
			   is unmanaged. */
			if (!(c->dockapp = is_dockapp(c)) || !scr->maker_check)
				unmanaged_client(c);
			else
				managed_client(c, e ? e->time : CurrentTime);
		} else
			managed_client(c, e ? e->time : CurrentTime);
	}
	if (old)
		XFree(old);
}

static void
cm_handle_WM_STATE(XClientMessageEvent *e, Client *c)
{
	PTRACE(5);
#if 1
	if (!c || c->managed)
		return;
	CPRINTF(1, c, "[cm] WM_STATE for unmanaged window 0x%08lx\n", e->window);
	managed_client(c, CurrentTime);
#endif
}

/** @brief handle WM_TRANSIENT_FOR PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * WM_TRANSIENT_FOR is the window for which the current window is transient.
  * Because this property is primary and structural, a recheck of sequence will
  * have to be done if there is not already a sequence assigned to the client.
  */
static void
pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *e, Client *c)
{
	Window old, now = None;
	Client *l;

	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	old = (l = c->leads[TransiGroup]) ? l->win : None;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_TRANSIENT_FOR was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] WM_TRANSIENT_FOR = (deleted)\n");
		return;
	}
	get_window(c->win, XA_WM_TRANSIENT_FOR, AnyPropertyType, &now);
	if (now && (!old || old != now)) {
		if (old)
			CPRINTF(1, c, "[pc] WM_TRANSIENT_FOR was 0x%08lx\n", old);
		CPRINTF(1, c, "[pc] WM_TRANSIENT_FOR = 0x%08lx\n", now);
		add_group(c, now, TransiGroup);
		groups_need_retest(c);
	}
}

/** @brief handle WM_WINDOW_ROLE PropertyNotify event.
  * @param e - X event to handle (may be NULL)
  * @param c - client for which to handle event (should not be NULL)
  *
  * This property is set by the client to identify a separate role for the
  * window under X11R6 session management.  I don't really know why I'm tracking
  * it.
  */
static void
pc_handle_WM_WINDOW_ROLE(XPropertyEvent *e, Client *c)
{
	char *old;

	/* pointless */
	PTRACE(5);
	if (!c || (e && e->type != PropertyNotify))
		return;
	if ((old = c->role))
		c->role = NULL;
	if (e && e->state == PropertyDelete) {
		if (old)
			CPRINTF(1, c, "[pc] WM_WINDOW_ROLE was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_WINDOW_ROLE = (deleted)\n");
		if (old)
			XFree(old);
		return;
	}
	c->role = get_text(c->win, _XA_WM_WINDOW_ROLE);
	if (c->role && (!old || strcmp(old, c->role))) {
		if (old)
			CPRINTF(1, c, "[pc] WM_WINDOW_ROLE was \"%s\"\n", old);
		CPRINTF(1, c, "[pc] WM_WINDOW_ROLE = %s\n", c->role);
	}
	if (old)
		XFree(old);
}

static void
pc_handle_WM_ZOOM_HINTS(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
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
	CPRINTF(1, c, "[cm] _XEMBED message for unmanaged client 0x%08lx\n", e->window);
	managed_client(c, e->data.l[0]);
}

/** @brief handle MANAGER client message
  * @param e - ClientMessage event
  * @param c - client for message (may be NULL)
  *
  * We are expecting MANAGER messages for several selections: WM_S%d,
  * _NET_SYSTEM_TRAY_S%d, _NET_DESKTOP_LAYOUT_S%d, WM_CM_S%d and _XDG_ASSIST_S%d
  * which correspond to window manager, system tray, pager, composite manager
  * and launch assistant respectively.
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
			DPRINTF(1, "window manager changed from 0x%08lx to 0x%08lx\n",
				scr->icccm_check, owner);
			scr->icccm_check = owner;
			handle_wmchange();
		}
	} else if (selection == scr->stray_atom) {
		if (owner && owner != scr->stray_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "system tray changed from 0x%08lx to 0x%08lx\n", scr->stray_owner,
				owner);
			scr->stray_owner = owner;
			check_stray();
		}
	} else if (selection == scr->pager_atom) {
		if (owner && owner != scr->pager_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "pager changed from 0x%08lx to 0x%08lx\n", scr->pager_owner,
				owner);
			scr->pager_owner = owner;
			check_pager();
		}
	} else if (selection == scr->compm_atom) {
		if (owner && owner != scr->compm_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "composite manager changed from 0x%08lx to 0x%08lx\n", scr->compm_owner,
				owner);
			scr->compm_owner = owner;
			check_compm();
		}
	} else if (selection == scr->shelp_atom) {
		if (owner && owner != scr->shelp_owner) {
			XSelectInput(dpy, owner,
				     StructureNotifyMask | PropertyChangeMask);
			DPRINTF(1, "launch assistant changed from 0x%08lx to 0x%08lx\n", scr->compm_owner,
				owner);
			scr->shelp_owner = owner;
			check_shelp();
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
	PTRACE(5);
}

static void
pc_handle_NET_SYSTEM_TRAY_VISUAL(XPropertyEvent *e, Client *c)
{
	PTRACE(5);
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
	char *name = NULL;

	if (XFindContext(dpy, e->atom, PropertyContext, (XPointer *) &handle) == Success)
		(*handle) (e, c);
	else
		DPRINTF(1, "no PropertyNotify handler for %s\n",
			(name = XGetAtomName(dpy, e->atom)));
	if (name)
		XFree(name);
}

void
cm_handle_atom(XClientMessageEvent *e, Client *c)
{
	cm_handler_t handle = NULL;
	char *name = NULL;

	if (XFindContext(dpy, e->message_type, ClientMessageContext, (XPointer *) &handle)
	    == Success)
		(*handle) (e, c);
	else
		CPRINTF(1, c, "no ClientMessage handler for %s\n",
			(name = XGetAtomName(dpy, e->message_type)));
	if (name)
		XFree(name);
}

void
handle_property_event(XPropertyEvent *e)
{
	Client *c;

	if (e)
		pushtime(&current_time, e->time);
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && e) {
		pushtime(&c->last_time, e->time);
		scr = screens + c->screen;
	}
	pc_handle_atom(e, c);
}

/** @brief handle FocusIn event
  *
  * If a client receives a FocusIn event (receives the focus); then it was
  * mapped and is managed.  We can updated the client and mark it as managed.
  */
void
handle_focus_change_event(XFocusChangeEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->mapped) {
		CPRINTF(1, c, "[fc] XFocusChangeEvent for unmapped window 0x%08lx\n", e->window);
		mapped_client(c, CurrentTime);
	}
}

void
handle_expose_event(XExposeEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->mapped) {
		CPRINTF(1, c, "[ee] XExposeEvent for unmapped window 0x%08lx\n", e->window);
		mapped_client(c, CurrentTime);
	}
}

/** @brief handle VisibilityNotify event
  *
  * If a client becomes something other than fully obscured, then it is mapped
  * and visible, and was managed.  We can update the client and mark it as
  * managed.
  */
void
handle_visibility_event(XVisibilityEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->mapped
	    && e->state != VisibilityFullyObscured) {
		CPRINTF(1, c, "[ve] XVisibilityEvent for unmanaged window 0x%08lx\n", e->window);
		mapped_client(c, CurrentTime);
	}
}

/** @brief handle CreateNotify event
  *
  * When a top-level, non-override-redirect window is created, add it to the
  * potential client list as unmanaged.
  */
void
handle_create_window_event(XCreateWindowEvent *e)
{
	Client *c = NULL;

	/* only interested in top-level windows that are not override redirect */
	if (e->override_redirect || e->send_event || e->parent != scr->root)
		return;
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if (!(c = find_client_noscreen(e->window)))
		add_client(e->window);
}

/** @brief handle DestroyNotify event
  *
  * When the window destroyed is one of our check windows, recheck the
  * compliance of the window manager to the appropriate check.
  */
void
handle_destroy_window_event(XDestroyWindowEvent *e)
{
	Client *c = NULL;

	if (!find_screen(e->event)) /* NOTE: e->window is destroyed */
		DPRINTF(5, "could not find screen for window 0x%08lx\n", e->window);
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
	if (scr->netwm_check && e->window == scr->netwm_check) {
		DPRINTF(1, "NetWM removed from 0x%08lx\n", scr->netwm_check);
		check_netwm();
	}
	if (scr->winwm_check && e->window == scr->winwm_check) {
		DPRINTF(1, "WinWM removed from 0x%08lx\n", scr->winwm_check);
		check_winwm();
	}
	if (scr->maker_check && e->window == scr->maker_check) {
		DPRINTF(1, "Maker removed from 0x%08lx\n", scr->maker_check);
		check_maker();
	}
	if (scr->motif_check && e->window == scr->motif_check) {
		DPRINTF(1, "Motif removed from 0x%08lx\n", scr->motif_check);
		check_motif();
	}
	if (scr->icccm_check && e->window == scr->icccm_check) {
		DPRINTF(1, "ICCCM removed from 0x%08lx\n", scr->icccm_check);
		check_icccm();
	}
	if (scr->redir_check && e->window == scr->redir_check) {
		DPRINTF(1, "redir removed from 0x%08lx\n", scr->redir_check);
		check_redir();
	}
}

/** @brief handle MapNotify event
  *
  * If a client becomes mapped, then it is mapped and visible, and was managed.
  * We can update the client and mark it as managed.
  */
void
handle_map_event(XMapEvent *e)
{
	Client *c = NULL;

	if (e->override_redirect)
		return;
	if (!find_screen(e->window))
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed) {
		/* Not sure we should do this for anything but dockapps here...
		   Window managers may map normal windows before setting the
		   WM_STATE property, and system trays invariably map window
		   before sending the _XEMBED message. */
		if ((c->dockapp = is_dockapp(c))) {
			/* We missed a management event, so treat the window as
			   managed now */
			if (!c->mapped)
				CPRINTF(1, c, "[me] XMapEvent for unmapped window 0x%08lx\n", e->window);
			mapped_client(c, CurrentTime);
		}
	}
}

/** @brief handle UnmapNotify event
  *
  * If the client is unmapped (can be synthetic), we can perform deletion of
  * managed client window information.
  */
void
handle_unmap_event(XUnmapEvent *e)
{
	/* we can't tell much from a simple unmap event */
}

/** @brief handle Reparent event
  */
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
		DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
	if ((c = find_client_noscreen(e->window)) && !c->managed) {
		if (e->parent == scr->root)
			/* reparented the wrong way */
			return;
		if ((c->statusicon = is_trayicon(c))) {
			/* This is a status icon. */
			if (scr->stray_owner)
				/* Status icons will receive an _XEMBED message
				   from the system tray when managed. */
				return;
			/* It is questionable whether status icons will be
			   reparented at all when there is no system tray... */
			DPRINTF(1, "status icon 0x%08lx reparented to 0x%08lx\n",
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
			DPRINTF(1, "normal window 0x%08lx reparented to 0x%08lx\n",
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
			if (!c->managed)
				CPRINTF(1, c, "[re] XReparentEvent for unmanaged window 0x%08lx\n", e->window);
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
	Client *c;

	if (!find_screen(e->window))
		DPRINTF(4, "could not find screen for window 0x%08lx\n", e->window);
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
#if 1
		if (!find_screen(e->window)) {
			DPRINTF(1, "could not find screen for window 0x%08lx\n", e->window);
			return;
		}
		if (e->selection != scr->slctn_atom)
			return;
		if (e->window != scr->selwin)
			return;
		XDestroyWindow(dpy, scr->selwin);
		scr->selwin = None;
		DPRINTF(1, "lost " SELECTION_ATOM " selection: exiting\n", scr->screen);
		exit(EXIT_SUCCESS);
#endif
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
	Client *c;

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
	while (need_updates || need_retests || need_changes) {
		while (need_updates)
			for (c = clients; c; c = c->next)
				if (c->need_update)
					update_client(c);
		while (need_retests)
			for (c = clients; c; c = c->next)
				if (c->need_retest)
					retest_client(c);
		while (need_changes)
			for (c = clients; c; c = c->next)
				if (c->need_change)
					change_client(c);
	}
}

volatile int signum = 0;

void
sighandler(int sig)
{
	signum = sig;
}

#ifdef GIO_GLIB2_SUPPORT
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
#endif				/* GIO_GLIB2_SUPPORT */

static void
main_loop(int argc, char *argv[])
{
#ifdef GIO_GLIB2_SUPPORT
	int xfd;
	GIOChannel *chan;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	guint srce;
	GMainLoop *loop;

	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	chan = g_io_channel_unix_new(xfd);
	srce = g_io_add_watch(chan, mask, on_xfd_watch, NULL);
	(void) srce;

	loop = g_main_loop_new(NULL, FALSE);
	DPRINTF(1, "entering main loop\n");
	g_main_loop_run(loop);
	DPRINTF(1, "exitted main loop\n");
#else				/* GIO_GLIB2_SUPPORT */
	int xfd;
	XEvent ev;

	PTRACE(5);
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGQUIT, sighandler);

	/* main event loop */
	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	DPRINTF(1, "entering main loop\n");
	while (running) {
		struct pollfd pfd = { xfd, POLLIN | POLLHUP | POLLERR, 0 };

		if (signum) {
			EPRINTF("exiting on signal\n");
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
			}
		}
	}
	DPRINTF(1, "exitted main loop\n");
#endif				/* GIO_GLIB2_SUPPORT */
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

/** @brief run without taking the selection
  *
  * No SELECTION_ATOM is taken
  */
static void
do_monitor(int argc, char *argv[])
{
	int s;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0,
					BlackPixel(dpy, s), BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);
	}
	s = DefaultScreen(dpy);
	scr = screens + s;
	main_loop(argc, argv);
}

/** @brief run without replacing a running instance
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
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
					DisplayHeight(dpy, s), 1, 1, 0,
					BlackPixel(dpy, s), BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if (!(scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
			XSync(dpy, False);
		}
		XUngrabServer(dpy);

		if (scr->owner) {
			EPRINTF("another instance is running on screen %d\n", s);
			exit(EXIT_FAILURE);
		}
		announce_selection();
	}
	s = DefaultScreen(dpy);
	scr = screens + s;
	main_loop(argc, argv);
}

/** @brief replace running instance with this one
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
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
	s = DefaultScreen(dpy);
	scr = screens + s;
	while (selcount) {
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
		announce_selection();
	}
	main_loop(argc, argv);
}

/** @brief ask running instance to quit
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
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
	s = DefaultScreen(dpy);
	scr = screens + s;
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
    [-a, --assist]\n\
        run as a launch assistant process, default if no other\n\
        command option is specified\n\
    -r, --replace\n\
        restart %1$s by replacing the existing running process\n\
        with the current one\n\
    -q, --quit\n\
        ask a running %1$s process to stop\n\
    -m, --monitor\n\
        run as a monitoring process only\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General Options:\n\
    -g, --guard TIMEOUT\n\
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
			{"assist",	no_argument,		NULL, 'a'},
			{"guard",	required_argument,	NULL, 'g'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "mrqg:aD::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "mrqg:aDvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'a':	/* -a, --assist */
			if (options.command != CommandDefault)
				goto bad_command;
			options.command = CommandAssist;
			if (command == CommandDefault)
				command = CommandAssist;
			break;
		case 'm':	/* -m, --monitor */
			if (options.command != CommandDefault)
				goto bad_command;
			options.command = CommandMonitor;
			if (command == CommandDefault)
				command = CommandMonitor;
			options.assist = False;
			options.debug = 3;
			options.output = 3;
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
	case CommandAssist:
		if (!get_display())
			exit(EXIT_FAILURE);
		do_run(argc, argv);
		break;
	case CommandReplace:
		if (!get_display())
			exit(EXIT_FAILURE);
		do_replace(argc, argv);
		break;
	case CommandQuit:
		if (!get_display())
			exit(EXIT_FAILURE);
		do_quit(argc, argv);
		break;
	case CommandMonitor:
		if (!get_display())
			exit(EXIT_FAILURE);
		do_monitor(argc, argv);
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
