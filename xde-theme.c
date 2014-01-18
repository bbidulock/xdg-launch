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
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/select.h>
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

const char *program = NAME;

static int debug = 0;
static int output = 1;

typedef struct {
	Bool grab;
	Bool setroot;
	Bool nomonitor;
	char *theme;
	unsigned long delay;
	Bool areas;
	char **files;
} Options;

Options options = {
	False,
	False,
	False,
	NULL,
	2000,
	False,
	NULL
};

Display *dpy = NULL;
int screen;
Window root;

Atom _XA_BB_THEME;
Atom _XA_BLACKBOX_PID;
Atom _XA_ESETROOT_PMAP_ID;
Atom _XA_ICEWMBG_QUIT;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_DESKTOP_LAYOUT;
Atom _XA_NET_DESKTOP_PIXMAPS;
Atom _XA_NET_NUMBER_OF_DESKTOPS;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_NET_WM_NAME;
Atom _XA_NET_WM_PID;
Atom _XA_OB_THEME;
Atom _XA_OPENBOX_PID;
Atom _XA_WIN_DESKTOP_BUTTON_PROXY;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WIN_WORKSPACE_COUNT;
Atom _XA_XROOTPMAP_ID;
Atom _XA_XSETROOT_ID;

static void handle_BB_THEME(XEvent *);
static void handle_BLACKBOX_PID(XEvent *);
static void handle_ESETROOT_PMAP_ID(XEvent *);
static void handle_NET_CURRENT_DESKTOP(XEvent *);
static void handle_NET_DESKTOP_LAYOUT(XEvent *);
static void handle_NET_NUMBER_OF_DESKTOPS(XEvent *);
static void handle_NET_SUPPORTING_WM_CHECK(XEvent *);
static void handle_NET_VISIBLE_DESKTOPS(XEvent *);
static void handle_OB_THEME(XEvent *);
static void handle_OPENBOX_PID(XEvent *);
static void handle_WIN_DESKTOP_BUTTON_PROXY(XEvent *);
static void handle_WIN_SUPPORTING_WM_CHECK(XEvent *);
static void handle_WIN_WORKSPACE_COUNT(XEvent *);
static void handle_WIN_WORKSPACE(XEvent *);
static void handle_XROOTPMAP_ID(XEvent *);
static void handle_XSETROOT_ID(XEvent *);

typedef struct {
	char *name;
	Atom *atom;
	void (*handler) (XEvent *);
	Atom value;
} Atoms;

Atoms atoms[] = {
	/* *INDENT-OFF* */
	/* name				global				handler					value			*/
	/* ----				------				-------					-----			*/
	{ "_BB_THEME",			&_XA_BB_THEME,			handle_BB_THEME,			None			},
	{ "_BLACKBOX_PID",		&_XA_BLACKBOX_PID,		handle_BLACKBOX_PID,			None			},
	{ "ESETROOT_PMAP_ID",		&_XA_ESETROOT_PMAP_ID,		handle_ESETROOT_PMAP_ID,		None			},
	{ "_ICEWMBG_QUIT",		&_XA_ICEWMBG_QUIT,		NULL,					None			},
	{ "_NET_CURRENT_DESKTOP",	&_XA_NET_CURRENT_DESKTOP,	handle_NET_CURRENT_DESKTOP,		None			},
	{ "_NET_DESKTOP_LAYOUT",	&_XA_NET_DESKTOP_LAYOUT,	handle_NET_DESKTOP_LAYOUT,		None			},
	{ "_NET_DESKTOP_PIXMAPS",	&_XA_NET_DESKTOP_PIXMAPS,	NULL,					None			},
	{ "_NET_NUMBER_OF_DESKTOPS",	&_XA_NET_NUMBER_OF_DESKTOPS,	handle_NET_NUMBER_OF_DESKTOPS,		None			},
	{ "_NET_SUPPORTING_WM_CHECK",	&_XA_NET_SUPPORTING_WM_CHECK,	handle_NET_SUPPORTING_WM_CHECK,		None			},
	{ "_NET_VISIBLE_DESKTOPS",	&_XA_NET_VISIBLE_DESKTOPS,	handle_NET_VISIBLE_DESKTOPS,		None			},
	{ "_NET_WM_NAME",		&_XA_NET_WM_NAME,		NULL,					None			},
	{ "_NET_WM_PID",		&_XA_NET_WM_PID,		NULL,					None			},
	{ "_OB_THEME",			&_XA_OB_THEME,			handle_OB_THEME,			None			},
	{ "_OPENBOX_PID",		&_XA_OPENBOX_PID,		handle_OPENBOX_PID,			None			},
	{ "_WIN_DESKTOP_BUTTON_PROXY",	&_XA_WIN_DESKTOP_BUTTON_PROXY,	handle_WIN_DESKTOP_BUTTON_PROXY,	None			},
	{ "_WIN_SUPPORTING_WM_CHECK",	&_XA_WIN_SUPPORTING_WM_CHECK,	handle_WIN_SUPPORTING_WM_CHECK,		None			},
	{ "_WIN_WORKSPACE_COUNT",	&_XA_WIN_WORKSPACE_COUNT,	handle_WIN_WORKSPACE_COUNT,		None			},
	{ "_WIN_WORKSPACE",		&_XA_WIN_WORKSPACE,		handle_WIN_WORKSPACE,			None			},
	{ "WM_COMMAND",			NULL,				NULL,					XA_WM_COMMAND		},
	{ "_XROOTPMAP_ID",		&_XA_XROOTPMAP_ID,		handle_XROOTPMAP_ID,			None			},
	{ "_XSETROOT_ID",		&_XA_XSETROOT_ID,		handle_XSETROOT_ID,			None			},
	{ NULL,				NULL,				NULL,					None			}
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

Bool
get_display()
{
	if (!dpy) {
		if (!(dpy = XOpenDisplay(0))) {
			fprintf(stderr, "cannot open display\n");
			exit(127);
		} else {
			screen = DefaultScreen(dpy);
			root = RootWindow(dpy, screen);
			intern_atoms();
		}
	}
	return (dpy ? True : False);
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

Window *
get_windows(Window win, Atom prop, Atom type, long *n)
{
	return (Window *) get_cardinals(win, prop, type, n);
}

Bool
get_window(Window win, Atom prop, Atom type, Window *win_ret)
{
	return get_cardinal(win, prop, type, (long *) win_ret);
}

Time *
get_times(Window win, Atom prop, Atom type, long *n)
{
	return (Time *) get_cardinals(win, prop, type, n);
}

Bool
get_time(Window win, Atom prop, Atom type, Time * time_ret)
{
	return get_cardinal(win, prop, type, (long *) time_ret);
}

Atom *
get_atoms(Window win, Atom prop, Atom type, long *n)
{
	return (Atom *) get_cardinals(win, prop, type, n);
}

Bool
get_atom(Window win, Atom prop, Atom type, Atom *atom_ret)
{
	return get_cardinal(win, prop, type, (long *) atom_ret);
}

Window
check_recursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check;

	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
		if (nitems > 0) {
			check = data[0];
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

Window netwm_check;
Window winwm_check;

Window
check_netwm()
{
	if ((netwm_check = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW)))
		XSelectInput(dpy, netwm_check, PropertyChangeMask | StructureNotifyMask);
	return netwm_check;
}

Window
check_winwm()
{
	if ((winwm_check = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL)))
		XSelectInput(dpy, winwm_check, PropertyChangeMask | StructureNotifyMask);
	return winwm_check;
}

typedef struct Deferred Deferred;
struct Deferred {
	Deferred *next;
	void (*action)(void);
};

Deferred *deferred = NULL;

Deferred *
defer_action(void (*action) (void))
{
	Deferred *d = calloc(1, sizeof(*d));

	d->next = deferred;
	deferred = d;
	d->action = action;
	return d;
}

static void
handle_BB_THEME(XEvent *e)
{
}

static void
handle_BLACKBOX_PID(XEvent *e)
{
}

static void
handle_ESETROOT_PMAP_ID(XEvent *e)
{
}

static void
handle_NET_CURRENT_DESKTOP(XEvent *e)
{
}

static void
handle_NET_DESKTOP_LAYOUT(XEvent *e)
{
}

static void
handle_NET_NUMBER_OF_DESKTOPS(XEvent *e)
{
}

static void
handle_NET_SUPPORTING_WM_CHECK(XEvent *e)
{
}

static void
handle_NET_VISIBLE_DESKTOPS(XEvent *e)
{
}

static void
handle_OB_THEME(XEvent *e)
{
}

static void
handle_OPENBOX_PID(XEvent *e)
{
}

static void
handle_WIN_DESKTOP_BUTTON_PROXY(XEvent *e)
{
}

static void
handle_WIN_SUPPORTING_WM_CHECK(XEvent *e)
{
}

static void
handle_WIN_WORKSPACE_COUNT(XEvent *e)
{
}

static void
handle_WIN_WORKSPACE(XEvent *e)
{
}

static void
handle_XROOTPMAP_ID(XEvent *e)
{
}

static void
handle_XSETROOT_ID(XEvent *e)
{
}

Bool running;

void
handle_event(XEvent *e)
{
	int i;

	switch (e->type) {
	case PropertyNotify:
		for (i = 0; atoms[i].atom; i++) {
			if (e->xproperty.atom == atoms[i].value) {
				if (atoms[i].handler)
					atoms[i].handler(e);
				break;
			}
		}
		break;
	}
}

void
handle_deferred_events()
{
	Deferred *d, *next;

	while ((d = deferred)) {
		next = d->next;
		d->action();
		if (deferred == d) {
			deferred = next;
			free(d);
		}
	}
}

void
event_loop()
{
	fd_set rd;
	int xfd;
	XEvent ev;

	running = True;
	XSelectInput(dpy, root, PropertyChangeMask);
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);

	while (running) {
		FD_ZERO(&rd);
		FD_SET(xfd, &rd);
		if (select(xfd + 1, &rd, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "select failed\n");
			fflush(stderr);
			exit(1);
		}
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			handle_event(&ev);
		}
		handle_deferred_events();
	}
}

/*
 * Testing for window managers:
 *
 * IceWM:   Sets _NET_SUPPORTING_WM_CHECK(WINDOW) appropriately.  Note that it sets
 *	    _WIN_SUPPORTING_WM_CHECK(CARDINAL) as well.  Also, it sets both to the
 *	    same window.  It sets _NET_WM_NAME(STRING) to "IceWM 1.3.7 (Linux
 *	    3.4.0/x86_64)" or some such.  Extract the first word of the string for
 *	    the actual name.  Note that _NET_WM_NAME should be (UTF8_STRING) instead
 *	    of (STRING) [this has been fixed].  It sets _NET_WM_PID(CARDINAL) to the
 *	    pid of the window manager; however, it does not set
 *	    WM_CLIENT_MACHINE(STRING) to the fully qualified domain name of the
 *	    window manager machine as required by the EWMH specification [this has
 *	    been fixed].
 *
 * Blackbox:
 *	    Blackbox is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.  It
 *	    properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and the
 *	    check window.  On the check window the only other thing that it sets is
 *	    _NET_WM_NAME(UTF8_STRING) whcih is a property UTF8_STRING with the single
 *	    word "Blackbox".  [It now sets _NET_WM_PID correctly, but still does not
 *	    set WM_CLIENT_MACHINE(STRING) to the fully qualified domain name of the
 *	    window manager machine. [fixed]]
 *
 * Fluxbox: Fluxbox is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.  It
 *	    properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and the
 *	    check window.  On the check window the only other thing it sets is
 *	    _NET_WM_NAME(UTF8_STRING) which is a propert UTF8_STRING with the single
 *	    word "Fluxbox".
 *
 *	    Fluxbox also sets _BLACKBOX_PID(CARDINAL) on the root window.  (Gee,
 *	    blackbox doesn't!)  Fluxbox interns the _BLACKBOX_ATTRIBUTES atom and
 *	    then does nothing with it.  Fluxbox interns the _FLUXBOX_ACTION,
 *	    _FLUXBOX_ACTION_RESULT and _FLUXBOX_GROUP_LEFT atoms.  Actions are only
 *	    possible when the session.session0.allowRemoteActions resources is set to
 *	    tru.  THey are affected by changing the _FLUXBOX_ACTION(STRING) property
 *	    on the root window to reflect the new command.  The result is
 *	    communicated by fluxbox setting the _FLUXBOX_ACTION_RESULT(STRING)
 *	    property on the root window with the result.
 *
 * Openbox: Openbox is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.  It
 *	    properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and the
 *	    check window.  On the check window, the only other thing that it sets is
 *	    _NET_WM_NAME(UTF8_STRING) which is a proper UTF8_STRING with the single
 *	    word "Openbox".
 *
 *	    Openbox also sets _OPENBOX_PID(CARDINAL) on the root window.  It also
 *	    sets _OB_VERSION(UTF8_STRING) and _OB_THEME(UTF8_STRING) on the root
 *	    window.
 *
 * FVWM:    FVWM is both GNOME/WMH and ICCCM/EWMH compliant.  It sets
 *	    _NET_SUPPORTING_WM_CHECK(WINDOW) properly on the root and check window.
 *	    On the check window it sets _NET_WM_NAME(UTF8_STRING) to "FVWM".  It sets
 *	    WM_NAME(STRING) to "fvwm" and WM_CLASS(STRING) to "fvwm", "FVWM".  FVWM
 *	    implements _WIN_SUPPORTING_WM_CHECK(CARDINAL) in a separate window from
 *	    _NET_SUPPORTING_WM_CHECK(WINDOW), but the same one as
 *	    _WIN_DESKTOP_BUTTON_PROXY(CARDINAL).  There are no additional properties
 *	    set on those windows.
 *
 * WindowMaker:
 *	    WindowMaker is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.
 *	    It properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and
 *	    the check window.  It does not set the _NET_WM_NAME(UTF8_STRING) on the
 *	    check window.  It does, however, define a recursive
 *	    _WINDOWMAKER_NOTICEBOARD(WINDOW) that shares the same window as the check
 *	    window and sets the _WINDOWMAKER_ICON_TILE(_RGBA_IMAGE) property on this
 *	    window to the ICON/DOCK/CLIP tile.
 *
 * PeKWM:
 *	    PeKWM is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.  It
 *	    properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and the
 *	    check window.  It sets _NET_WM_NAME(STRING) on the check window.  Note
 *	    that _NET_WM_NAME should be (UTF8_STRING) instead of (STRING) (corrected
 *	    in 'git' version).  It does not set WM_CLIENT_MACHINE(STRING) on the
 *	    check window as required by EWMH, but sets it on the root window.  It
 *	    does not, however, set it to the fully qualified domain name as required
 *	    by EWMH.  Also, it sets _NET_WM_PID(CARDINAL) on the check window, but
 *	    mistakenly sets it on the root window.  It sets WM_CLASS(STRING) to a
 *	    null string on the check window and does not set WM_NAME(STRING).
 *
 * JWM:
 *	    JWM is only ICCCM/EWMH compliant and is not GNOME/WMH compliant.  It
 *	    properly sets _NET_SUPPORTING_WM_CHECK(WINDOW) on both the root and the
 *	    check window.  It properly sets _NET_WM_NAME(UTF8_STRING) on the check
 *	    window (to "JWM").  It does not properly set _NET_WM_PID(CARDINAL) on the
 *	    check window, or anywhere for that matter [it does now].  It does not set
 *	    WM_CLIENT_MACHINE(STRING) anywhere and there is no WM_CLASS(STRING) or
 *	    WM_NAME(STRING) on the check window.
 *
 */

static void
copying(int argc, char *argv[])
{
	if (!output && !debug)
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
	if (!output && !debug)
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
	if (!output && !debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [command option] [options] [FILE [FILE ...]]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	if (!output && !debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [command option] [options] [FILE [FILE ...]]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    [FILE [FILE ...]]\n\
        a list of files (one per virtual desktop)\n\
Command options:\n\
    -s, --set\n\
        set the background\n\
    -e, --edit\n\
        launch background settings editor\n\
    -q, --quit\n\
        ask running instance to quit\n\
    -r, --restart\n\
        ask running instance to restart\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
Options:\n\
    -g, --grab\n\
	grab the X server while setting backgrounds\n\
    -s, --setroot\n\
	set the background pixmap instead of just properties\n\
    -n, --nomonitor\n\
	exit after setting the background\n\
    -t, --theme THEME\n\
	set the specified theme\n\
    -d, --delay MILLISECONDS\n\
	specifies delay after window manager appearance\n\
    -a, --areas\n\
	distribute backgrounds also over work areas\n\
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
			{"grab",	no_argument,		NULL, 'g'},
			{"setroot",	no_argument,		NULL, 's'},
			{"nomonitor",	no_argument,		NULL, 'n'},
			{"theme",	required_argument,	NULL, 't'},
			{"delay",	required_argument,	NULL, 'd'},
			{"areas",	no_argument,		NULL, 'a'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "D::v::hVCH?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "DvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'g':	/* -g, --grab */
			options.grab = True;
			break;
		case 's':	/* -s, --setroot */
			options.setroot = True;
			break;
		case 'n':	/* -n, --nomonitor */
			options.nomonitor = True;
			break;
		case 't':	/* -t, --theme THEME */
			options.theme = strdup(optarg);
			break;
		case 'd':	/* -d, --delay MILLISECONDS */
			options.delay = strtoul(optarg, NULL, 0);
			break;
		case 'a':	/* -a, --areas */
			options.areas = True;
			break;

		case 'D':	/* -D, --debug [level] */
			if (debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			if (debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				output++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			if (debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(0);
		case 'V':	/* -V, --version */
			if (debug)
				fprintf(stderr, "%s: printing version message\n", argv[0]);
			version(argc, argv);
			exit(0);
		case 'C':	/* -C, --copying */
			if (debug)
				fprintf(stderr, "%s: printing copying message\n", argv[0]);
			copying(argc, argv);
			exit(0);
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (output || debug) {
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
	if (debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind < argc) {
		int n = argc - optind, j = 0;

		options.files = calloc(n + 1, sizeof(*options.files));
		while (optind < argc)
			options.files[j++] = strdup(argv[optind++]);
	}
	exit(0);
}
