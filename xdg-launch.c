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

struct params {
	char *appid;
	char *launcher;
	char *launchee;
	char *sequence;
	char *hostname;
	char *monitor;
	char *screen;
	char *desktop;
	char *timestamp;
	char *name;
	char *icon;
	char *binary;
	char *description;
	char *wmclass;
	char *silent;
	char *exec;
	char *file;
	char *url;
	char *pid;
	char *keyboard;
	char *pointer;
	char *assist;
};

struct params options = {
	.launcher = NAME,
};

struct params defaults = {
	.appid = "[APPID]",
	.launcher = NAME,
	.launchee = "[APPID]",
	.sequence = "0",
	.hostname = "gethostname()",
	.monitor = "0",
	.screen = "0",
	.desktop = "0",
	.timestamp = "0",
	.name = "[Name=]",
	.icon = "[Icon=]",
	.binary = "[TryExec=|Exec=]",
	.description = "[Comment=]",
	.wmclass = "[StartupWMClass=]",
	.silent = "0",
	.file = "",
	.url = "",
	.keyboard = "auto",
	.pointer = "auto",
	.assist = "auto",
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
};

struct fields fields = { NULL, };

struct entry {
	char *Name;
	char *Comment;
	char *Icon;
	char *TryExec;
	char *Exec;
	char *Terminal;
	char *StartupNotify;
	char *StartupWMClass;
};

struct entry entry = { NULL, };

Display *dpy = NULL;
int monitor;
int screen;
Window root;

Atom _XA_NET_STARTUP_ID;
Atom _XA_NET_WM_PID;
Atom _XA_NET_STARTUP_INFO_BEGIN;
Atom _XA_NET_STARTUP_INFO;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_WIN_WORKSPACE;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_NET_WM_USER_TIME;
Atom _XA_NET_WM_USER_TIME_WINDOW;
Atom _XA_NET_WM_STATE;
Atom _XA_NET_WM_ALLOWED_ACTIONS;

struct atoms {
	char *name;
	Atom *atom;
} atoms[] = {
	/* *INDENT-OFF* */
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID		},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN	},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO		},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK	},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK	},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED		},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP	},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS	},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE		},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP		},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME		},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW	},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE		},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS	},
	{ NULL,					NULL				}
	/* *INDENT-ON* */
};

void
intern_atoms()
{
	int i, n;
	char **atom_names;
	Atom *atom_values;

	for (n = 0; atoms[n].name; n++) ;
	atom_names = calloc(n + 1, sizeof(*atom_names));
	atom_values = calloc(n + 1, sizeof(*atom_values));
	for (i = 0; i < n; i++)
		atom_names[i] = atoms[i].name;
	XInternAtoms(dpy, atom_names, n, False, atom_values);
	for (i = 0; i < n; i++)
		*atoms[i].atom = atom_values[i];
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

Bool
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
			return False;
		}
		if (XGetWindowProperty(dpy, check, atom, 0L, 1L, False, type, &real,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success && format != 0) {
			if (nitems > 0) {
				if (check != (Window) data[0]) {
					XFree(data);
					return False;
				}
			} else {
				if (data)
					XFree(data);
				return False;
			}
			XFree(data);
		} else
			return False;
	} else
		return False;
	return True;
}

Bool
check_netwm()
{
	return check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

Bool
check_winwm()
{
	return check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

Bool
set_screen_of_root(Window sroot)
{
	int i, count;

	for (i = 0, count = ScreenCount(dpy); i < count; i++)
		if (RootWindow(dpy, i) == sroot) {
			screen = i;
			root = sroot;
			return True;
		}
	fprintf(stderr, "%s: ERROR: Could not find screen for root 0x%lx!\n", NAME, sroot);
	return False;
}

Bool
find_focus_screen()
{
	Window focus, froot;
	int di;
	unsigned int du;

	XGetInputFocus(dpy, &focus, &di);

	if (focus == None || focus == PointerRoot)
		return False;

	if (!XGetGeometry(dpy, focus, &froot, &di, &di, &du, &du, &du, &du))
		return False;

	return set_screen_of_root(froot);
}

Bool
find_pointer_screen()
{
	Window proot = None, dw;
	int di;
	unsigned int du;

	if (XQueryPointer(dpy, root, &proot, &dw, &di, &di, &di, &di, &du))
		return True;
	return set_screen_of_root(proot);
}

void
set_screen()
{
	free(fields.screen);
	fields.screen = calloc(64, sizeof(*fields.screen));
	if (options.screen && 0 <= atoi(options.screen) && atoi(options.screen) < ScreenCount(dpy))
		strcat(fields.screen, options.screen);
	else if (options.keyboard && find_focus_screen())
		snprintf(fields.screen, 64, "%d", screen);
	else if (options.pointer && find_pointer_screen())
		snprintf(fields.screen, 64, "%d", screen);
	else if (!options.keyboard && !options.pointer &&
		 (find_focus_screen() || find_pointer_screen()))
		snprintf(fields.screen, 64, "%d", screen);
	else {
		screen = DefaultScreen(dpy);
		root = DefaultRootWindow(dpy);
		snprintf(fields.screen, 64, "%d", screen);
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
find_focus_monitor()
{
	Window focus, froot;
	int x, y, xmin, xmax, ymin, ymax, a, area, best, di;
	unsigned int w, h, b, du;

	XGetInputFocus(dpy, &focus, &di);

	if (focus == None || focus == PointerRoot)
		return False;
	if (!XGetGeometry(dpy, focus, &froot, &x, &y, &w, &h, &b, &du))
		return False;
	xmin = x;
	xmax = x + w + 2 * b;
	ymin = y;
	ymax = y + h + 2 * b;
#if XINERAMA
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
			monitor = best;
			return True;
		}
	}
      no_xinerama:
#endif
#if XRANDR
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
			monitor = best;
			return True;
		}
	}
      no_xrandr:
#endif
	return False;
}

Bool
find_pointer_monitor()
{
	int di, x, y;
	Window proot, dw;
	unsigned int du;

	XQueryPointer(dpy, root, &proot, &dw, &x, &y, &di, &di, &du);
#if XINERAMA
	if (XineramaQueryExtension(dpy, &di, &di) && XineramaIsActive(dpy)) {
		int i, n;
		XineramaScreenInfo *si;

		if (!(si = XineramaQueryScreens(dpy, &n)) || n < 2)
			goto no_xinerama;
		for (i = 0; i < n; i++)
			if (x >= si[i].x_org && x <= si[i].x_org + si[i].width &&
			    y >= si[i].y_org && y <= si[i].y_org + si[i].height) {
				monitor = si[i].screen_number;
				XFree(si);
				return True;
			}
		XFree(si);
	}
      no_xinerama:
#endif
#if XRANDR
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
				monitor = i;
				XFree(ci);
				XFree(sr);
				return True;
			}
			XFree(ci);
		}
		XFree(sr);
	}
      no_xrandr:
#endif
	return False;
}

void
set_monitor()
{
	free(fields.monitor);
	fields.monitor = calloc(64, sizeof(*fields.monitor));
	if (options.monitor)
		strcat(fields.monitor, options.monitor);
	else if (options.keyboard && find_focus_monitor())
		snprintf(fields.monitor, 64, "%d", monitor);
	else if (options.pointer && find_pointer_monitor())
		snprintf(fields.monitor, 64, "%d", monitor);
	else if (!options.keyboard && !options.pointer &&
		 (find_focus_monitor() || find_pointer_monitor()))
		snprintf(fields.monitor, 64, "%d", monitor);
	else {
		free(fields.monitor);
		fields.monitor = NULL;
	}
}

void
set_desktop()
{
	Atom atom, real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	if (!check_netwm())
		goto no_netwm;
	atom = _XA_NET_CURRENT_DESKTOP;
	if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
			       AnyPropertyType, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
		if (monitor && nitems > monitor) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[monitor]);
		} else if (nitems > 0) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[0]);
		}
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	atom = _XA_NET_VISIBLE_DESKTOPS;
	if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
			       AnyPropertyType, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
		if (monitor && nitems > monitor) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[monitor]);
		} else if (nitems > 0) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[0]);
		}
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	if (fields.desktop)
		return;
      no_netwm:
	if (!check_winwm())
		goto no_winwm;
	atom = _XA_WIN_WORKSPACE;
	if (XGetWindowProperty(dpy, root, atom, 0L, monitor + 1, False,
			       AnyPropertyType, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
		if (monitor && nitems > monitor) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[monitor]);
		} else if (nitems > 0) {
			free(fields.desktop);
			fields.desktop = calloc(64, sizeof(*fields.desktop));
			snprintf(fields.desktop, 64, "%lu", data[0]);
		}
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	if (fields.desktop)
		return;
      no_winwm:
	return;
}

void
set_name()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.name);
	if (options.name)
		fields.name = strdup(options.name);
	else if (entry.Name)
		fields.name = strdup(entry.Name);
	else
		fields.name = strdup("");	/* must be included in new: message */
}

void
set_description()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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

char *
set_wmclass()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.wmclass);
	if (options.wmclass)
		fields.wmclass = strdup(options.wmclass);
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.pid);
	if (options.pid)
		fields.pid = strdup(options.pid);
	else if ((p = fields.id) &&
		 (p = strchr(p, '/')) && p++ &&
		 (p = strchr(p, '/')) && p++ && (q = strchr(p, '-')) && strtoul(p, NULL, 0))
		fields.pid = strndup(p, q - p);
	else {
		fields.pid = calloc(64, sizeof(*fields.pid));
		snprintf(fields.pid, 64, "%d", (int) getpid());
	}
}

void
set_hostname()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.sequence);
	if (options.sequence)
		fields.sequence = strdup(options.sequence);
	else {
		fields.sequence = calloc(64, sizeof(*fields.sequence));
		snprintf(fields.sequence, 64, "%d", 0);
	}
}

void
set_timestamp()
{
	char *p;

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.timestamp);
	if (options.timestamp)
		fields.timestamp = strdup(options.timestamp);
	else if (fields.id && (p = strstr(fields.id, "_TIME")) && strtoul(p + 5, NULL, 0))
		fields.timestamp = strdup(p + 5);
	else {
		XEvent ev;
		Atom atom = _XA_TIMESTAMP_PROP;
		unsigned char data = 'a';

		XSelectInput(dpy, root, PropertyChangeMask);
		XSync(dpy, False);
		XChangeProperty(dpy, root, atom, atom, 8, PropModeReplace, &data, 1);
		XMaskEvent(dpy, PropertyChangeMask, &ev);
		XSelectInput(dpy, root, NoEventMask);

		fields.timestamp = calloc(64, sizeof(*fields.timestamp));
		snprintf(fields.timestamp, 64, "%lu", ev.xproperty.time);
	}
}

void
do_subst(char *cmd, char *chars, char *str)
{
	int len = 0;
	char *p;

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d (cmd %s, char %s, str %s)\n", __FUNCTION__,
			__FILE__, __LINE__, cmd, chars, str);
	len = str ? strlen(str) : 0;
	for (p = cmd; (p = strchr(p, '%')); p++) {
		if (*(p - 1) != '%' && strspn(p + 1, chars)) {
			memmove(p + len, p + 2, strlen(p + 2) + 1);
			memcpy(p, str, len);
			p += len - 1;
		}
	}
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
	char *cmd, *p;
	int len;

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.command);
	cmd = calloc(2048, sizeof(*cmd));
	if (truth_value(entry.Terminal)) {
		/* A little more to be done here: we should set WMCLASS to xterm to assist the DE.
		   SILENT should be set to zero. */
		strncat(cmd, "xterm -T \"%c\" -e ", 1024);
		free(fields.wmclass);
		fields.wmclass = strdup("xterm");
		free(fields.silent);
		fields.silent = NULL;
	}
	if (options.exec)
		strncat(cmd, options.exec, 1024);
	else if (entry.Exec)
		strncat(cmd, entry.Exec, 1024);
	else {
		fprintf(stderr, "%s: cannot launch anything without a command\n", NAME);
		exit(1);
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
	do_subst(cmd, "i", fields.icon);
	do_subst(cmd, "c", fields.name);
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
	return (fields.command = cmd);
}

void
set_silent()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.bin);
	if (options.binary)
		fields.bin = strdup(options.binary);
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.application_id);
	if (options.appid) {
		if ((p = strrchr(options.appid, '/')))
			p++;
		else
			p = options.appid;
		if ((q = strstr(options.appid, ".desktop")) && !*(q + 8))
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.launcher);
	if (options.launcher)
		fields.launcher = strdup(options.launcher);
	else if (fields.id && (p = strchr(fields.id, '/')))
		fields.id = strndup(fields.id, p - fields.id);
	else
		fields.launcher = strdup(NAME);
	for (; (p = strchr(fields.launcher, '|')); *p = '/') ;
}

void
set_launchee()
{
	char *p, *q;

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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

	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	free(fields.id);

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
		 launcher, launchee, fields.pid, fields.sequence,
		 fields.hostname, fields.timestamp);
	free(launcher);
	free(launchee);
}

void
set_all()
{
	if (output > 2)
		fprintf(stderr, "starting %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
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
	if (!fields.pid)
		set_pid();
	if (!fields.hostname)
		set_hostname();
	if (!fields.command)
		set_command();
	if (!fields.id)
		set_id();
}

/*
 * Check whether the window manager needs assitance: return True when it needs assistance
 * and False otherwise.
 */
Bool
need_assistance()
{
	Atom atom, real, test;
	int format;
	unsigned long nitems, after, i;
	unsigned long *data;
	Bool ret;

	if (!check_netwm()) {
		fprintf(stderr, "Failed NetWM check!\n");
		return True;
	}

	atom = _XA_NET_SUPPORTED;
	test = _XA_NET_STARTUP_ID;
	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, XA_ATOM, &real, &format,
			       &nitems, &after, (unsigned char **) &data) != Success
	    || format == 0 || nitems < 1) {
		if (data)
			XFree(data);
		fprintf(stderr, "Failed to retrieve _NET_SUPPORTED! %s %s:%d\n", __FUNCTION__,
			__FILE__, __LINE__);
		return True;
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	after = ((after + 1) >> 2) + 1;
	if (XGetWindowProperty(dpy, root, atom, 0L, after, False, XA_ATOM, &real, &format,
			       &nitems, &after, (unsigned char **) &data) != Success
	    || format == 0 || nitems < 1) {
		if (data)
			XFree(data);
		fprintf(stderr, "Failed to retrieve _NET_SUPPORTED! %s %s:%d\n", __FUNCTION__,
			__FILE__, __LINE__);
		return True;
	}
	for (ret = True, i = 0; i < nitems && ret; i++)
		if (test == (Atom) data[i])
			ret = False;
	if (data) {
		XFree(data);
		data = NULL;
	}
	return ret;
}

int
parse_file(char *path)
{
	FILE *file;
	char buf[4096], *p, *q;
	int outside_entry = 1;
	char *section = NULL;
	char *key, *val, *lang, *llang, *slang;
	int ok = 0;

	if (getenv("LANG") && *getenv("LANG"))
		llang = strdup(getenv("LANG"));
	else
		llang = strdup("POSIX");
	if (output > 1)
		fprintf(stderr, "language is '%s'\n", llang);

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

	if (output > 1) {
		fprintf(stderr, "long  language string is '%s'\n", llang);
		fprintf(stderr, "short language string is '%s'\n", slang);
	}

	if (!(file = fopen(path, "r"))) {
		fprintf(stderr, "%s: cannot open file '%s' for reading\n", NAME, path);
		exit(1);
	}
	while ((p = fgets(buf, sizeof(buf), file))) {
		if ((q = strchr(p, '\n')))
			*q = '\0';
		if (*p == '[' && (q = strchr(p, ']'))) {
			*q = '\0';
			free(section);
			section = strdup(p + 1);
			outside_entry = strcmp(section, "Desktop Entry");
		}
		if (outside_entry)
			continue;
		if (*p == '#' || !(q = strchr(p, '=')))
			continue;
		*q = '\0';
		key = p;
		val = q + 1;
		if (strstr(key, "Name") == key) {
			if (strcmp(key, "Name") == 0) {
				if (!entry.Name)
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
				if (!entry.Comment)
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
			if (!entry.Icon)
				entry.Icon = strdup(val);
			ok = 1;
		} else if (strcmp(key, "TryExec") == 0) {
			if (!entry.TryExec)
				entry.TryExec = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Exec") == 0) {
			if (!entry.Exec)
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
		}
	}
	fclose(file);
	return ok;
}

char *
lookup_file(void)
{
	char *path = NULL;
	struct stat st;

	if (strstr(options.appid, ".desktop") != options.appid + strlen(options.appid) - 8)
		options.appid = strcat(options.appid, ".desktop");
	if (!strchr(options.appid, '/')) {
		/* go looking for it, need to look in XDG_DATA_HOME and then each of the
		   directories in XDG_DATA_DIRS */
		char *home, *dirs;

		if (getenv("XDG_DATA_DIRS") && *getenv("XDG_DATA_DIRS") != 0)
			dirs = getenv("XDG_DATA_DIRS");
		else
			dirs = "/usr/local/share:/usr/share";
		home = calloc(4096, sizeof(*home));
		if (getenv("XDG_DATA_HOME") && *getenv("XDG_DATA_HOME") != 0)
			strcat(home, getenv("XDG_DATA_HOME"));
		else {
			if (getenv("HOME"))
				strcat(home, getenv("HOME"));
			else
				strcat(home, "~");
			strcat(home, "/.local/share");
		}
		strcat(home, ":");
		strcat(home, dirs);

		for (dirs = home; !path && strlen(dirs);) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = 0;
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = 0;
			}
			path = realloc(path, 4096);
			strcat(path, "/applications/");
			strcat(path, options.appid);
			if (stat(path, &st) == 0)
				break;
			free(path);
			path = NULL;
		}
	} else {
		path = strdup(options.appid);
		if (stat(path, &st) == 0) {
			free(path);
			path = NULL;
		}
	}
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

XContext context;

void
send_msg(char *msg)
{
	XEvent xev;
	int l;
	Window from;
	char *p;

	if (output > 1)
		fprintf(stderr, "Message is: '%s'\n", msg);

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

typedef struct client client;

struct client {
	Window win;
	client *next;
};

client *clients = NULL;

void
handle_event(XEvent * e)
{
	client *c = NULL;

	XFindContext(dpy, e->xany.window, context, (XPointer *) & c);

	switch (e->type) {
	case PropertyNotify:
		if (!c)
			break;
		if (e->xproperty.atom == _XA_NET_STARTUP_ID) {
		} else if (e->xproperty.atom == XA_WM_HINTS) {
		} else if (e->xproperty.atom == XA_WM_NORMAL_HINTS) {
		}
		break;
	case FocusIn:
		break;
	case FocusOut:
		break;
	case Expose:
		break;
	case VisibilityNotify:
		break;
	case CreateNotify:
		if (!c) {
			c = calloc(1, sizeof(*c));
			c->next = clients;
			clients = c;
			c->win = e->xany.window;
			XSaveContext(dpy, c->win, context, (XPointer) c);
			XSelectInput(dpy, e->xany.window, ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
			break;
	case DestroyNotify:
			break;
	case UnmapNotify:
			break;
	case MapNotify:
			break;
	case ReparentNotify:
			break;
	case ConfigureNotify:
			break;
	case ClientMessage:
			break;
	case MappingNotify:
			break;
	default:
			break;
		}
	}
}

/*
 * Assist the window manager to do the right thing with respect to focus and
 * with respect to positioning of the window on the correct monitor and the
 * correct desktop.
 */
void
assist()
{
	pid_t pid;

	XSync(dpy, False);
	if ((pid = fork()) < 0) {
		perror(NAME);
		exit(1);
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
		perror(NAME);
		exit(1);
	} else if (pid != 0) {
		/* parent exits */
		exit(0);
	}
	/* release current directory */
	if (chdir("/") < 0) {
		perror(NAME);
		exit(1);
	}
	umask(0);		/* clear file creation mask */
	/* continue on monitoring */
	{
		fd_set rd;
		int xfd;
		XEvent ev;

		XSelectInput(dpy, root, VisibilityChangeMask | StructureNotifyMask |
			     FocusChangeMask | PropertyChangeMask);
		context = XUniqueContext();

		/* main event loop */
		running = True;
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
		}
	}
}

void
launch()
{
	size_t size;
	char *id, *cmd;
	Bool need_assist = False;
	Bool change_only = False;

	if (getenv("DESKTOP_STARTUP_ID") && *getenv("DESKTOP_STARTUP_ID"))
		change_only = True;

	size = strlen("DESKTOP_STARTUP_ID=") + strlen(fields.id) + 1;
	id = calloc(size, sizeof(*id));
	snprintf(id, size, "DESKTOP_STARTUP_ID=%s", fields.id);
	putenv(id);

	if (fields.wmclass) {
		if (output > 1)
			fprintf(stderr, "%s: WMCLASS: needs assistance\n", NAME);
		need_assist = True;
	}
	if (fields.silent && atoi(fields.silent)) {
		if (output > 1)
			fprintf(stderr, "%s: SILENT: needs assistance\n", NAME);
		need_assist = True;
	}
	if (!need_assistance())
		need_assist = False;
	else {
		if (output > 1)
			fprintf(stderr, "%s: WindowManager: needs assistance\n", NAME);
	}
	if (options.assist) {
		need_assist = True;
	}

	/* make the call... */
	if (change_only)
		send_change();
	else
		send_new();

	if (need_assist) {
		if (output > 1)
			fprintf(stderr, "%s: window manager assistance is needed\n", NAME);
#if 0
		assist();
#endif
	} else {
		if (output > 1)
			fprintf(stderr, "%s: window manager assistance is NOT needed\n", NAME);
	}

	cmd = calloc(strlen(fields.command) + 32, sizeof(cmd));
	strcat(cmd, "exec ");
	strcat(cmd, fields.command);
	if (output > 1)
		fprintf(stderr, "Command is: '%s'\n", cmd);
	execlp("/bin/sh", "sh", "-c", cmd, NULL);
	fprintf(stderr, "Should never get here!\n");
	exit(127);
}

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
    %1$s [options] [APPID [FILE|URL]]\n\
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
        name of launcher for startup id, default: '%2$s'\n\
    -l, --launchee LAUNCHEE\n\
        name of launchee for startup id, default: APPID\n\
    -S, --sequence SEQUENCE\n\
        sequence number to use in startup id, default: %3$s\n\
    -n, --hostname HOSTNAME\n\
        hostname to use in startup id, default: '%4$s'\n\
    -m, --monitor MONITOR\n\
        Xinerama monitor to specify in SCREEN tag, default: %5$s\n\
    -s, --screen SCREEN\n\
        screen to specify in SCREEN tag, default: %6$s\n\
    -w, --workspace DESKTOP\n\
        workspace to specify in DESKTOP tag, default: %7$s\n\
    -t, --timestamp TIMESTAMP\n\
        X server timestamp for startup id, default: %8$s\n\
    -N, --name NAME\n\
        name of XDG application, default: '%9$s'\n\
    -i, --icon ICON\n\
        icon name of the XDG application, default: '%10$s'\n\
    -b, --binary BINARY\n\
        binary name of the XDG application, default: '%11$s'\n\
    -D, --description DESCRIPTION\n\
        description of the XDG application, default: '%12$s'\n\
    -W, --wmclass WMCLASS\n\
        resource name or class of the XDG application, default: '%13$s'\n\
    -q, --silent SILENT\n\
        whether startup notification is silent (0/1), default: '%14$s'\n\
    -p, --pid PID\n\
        process id of the XDG application, default '%15$s'\n\
    -a, --appid APPID\n\
        override application identifier\n\
    -x, --exec EXEC\n\
        override command to execute\n\
    -f, --file FILE\n\
        filename to use with application\n\
    -u, --url URL\n\
        URL to use with application\n\
    -K, --keyboard\n\
        determine screen (monitor) from keyboard focus, default: '%16$s'\n\
    -P, --pointer\n\
        determine screen (monitor) from pointer location, default: '%17$s'\n\
    -A, --assist\n\
        assist non-startup notification aware window manager, default: '%18$s'\n\
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
", argv[0], defaults.launcher, defaults.sequence, defaults.hostname, defaults.monitor, defaults.screen, defaults.desktop, defaults.timestamp, defaults.name, defaults.icon, defaults.binary, defaults.description, defaults.wmclass, defaults.silent, defaults.pid, defaults.keyboard, defaults.pointer, defaults.assist);
}

int
main(int argc, char *argv[])
{
	while (1) {
		int c, val;
		char *buf;

		buf = defaults.hostname = options.hostname = calloc(64, sizeof(*buf));
		gethostname(buf, 64);
		defaults.pid = options.pid = calloc(64, sizeof(*buf));
		sprintf(defaults.pid, "%d", getpid());

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
			{"pid",		required_argument,	NULL, 'p'},
			{"appid",	required_argument,	NULL, 'a'},
			{"exec",	required_argument,	NULL, 'x'},
			{"file",	required_argument,	NULL, 'f'},
			{"url",		required_argument,	NULL, 'u'},
			{"keyboard",	no_argument,		NULL, 'K'},
			{"pointer",	no_argument,		NULL, 'P'},
			{"assist",	no_argument,		NULL, 'A'},

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
				     "L:l:S:n:m:s:p:w:t:N:i:b:d:W:q:a:x:f:u:KPAD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "L:l:S:n:m:s:p:w:t:N:i:b:d:W:q:a:x:f:u:KPADvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'L':	/* -L, --lancher LAUNCHER */
			defaults.launcher = options.launcher = strdup(optarg);
			break;
		case 'l':	/* -l, --launchee LAUNCHEE */
			defaults.launchee = options.launchee = strdup(optarg);
			break;
		case 'S':	/* -S, --sequence SEQUENCE */
			defaults.sequence = options.sequence = strdup(optarg);
			break;
		case 'n':	/* -n, --hostname HOSTNAME */
			defaults.hostname = options.hostname = strdup(optarg);
			break;
		case 'm':	/* -m, --monitor MONITOR */
			defaults.monitor = options.monitor = strdup(optarg);
			monitor = strtoul(optarg, NULL, 0);
			break;
		case 's':	/* -s, --screen SCREEN */
			defaults.screen = options.screen = strdup(optarg);
			screen = strtoul(optarg, NULL, 0);
			break;
		case 'p':	/* -p, --pid PID */
			defaults.pid = options.pid = strdup(optarg);
			break;
		case 'w':	/* -w, --workspace WORKSPACE */
			defaults.desktop = options.desktop = strdup(optarg);
			break;
		case 't':	/* -t, --timestamp TIMESTAMP */
			defaults.timestamp = options.timestamp = strdup(optarg);
			break;
		case 'N':	/* -N, --name NAME */
			defaults.name = options.name = strdup(optarg);
			break;
		case 'i':	/* -i, --icon ICON */
			defaults.icon = options.icon = strdup(optarg);
			break;
		case 'b':	/* -b, --binary BINARY */
			defaults.binary = options.binary = strdup(optarg);
			break;
		case 'd':	/* -d, --description DESCRIPTION */
			defaults.description = options.description = strdup(optarg);
			break;
		case 'W':	/* -W, --wmclass WMCLASS */
			defaults.wmclass = options.wmclass = strdup(optarg);
			break;
		case 'q':	/* -q, --silent SILENT */
			defaults.silent = options.silent = strdup(optarg);
			break;
		case 'a':	/* -a, --appid APPID */
			defaults.appid = options.appid = strdup(optarg);
			break;
		case 'x':	/* -x, --exec EXEC */
			defaults.exec = options.exec = strdup(optarg);
			break;
		case 'f':	/* -f, --file FILE */
			defaults.file = options.file = strdup(optarg);
			break;
		case 'u':	/* -u, --url URL */
			defaults.url = options.url = strdup(optarg);
			break;
		case 'K':	/* -K, --keyboard */
			defaults.keyboard = options.keyboard = strdup("true");
			if (options.pointer) {
				free(options.pointer);
				options.pointer = NULL;
				defaults.pointer = "inactive";
			}
			break;
		case 'P':	/* -P, --pointer */
			defaults.pointer = options.pointer = strdup("true");
			if (options.keyboard) {
				free(options.keyboard);
				options.keyboard = NULL;
				defaults.keyboard = "inactive";
			}
			break;
		case 'A':	/* -A, --assist */
			defaults.assist = options.assist = strdup("true");
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
	if (!options.appid && !options.exec) {
		fprintf(stderr, "%s: APPID or EXEC must be specified\n", argv[0]);
		goto bad_usage;
	} else if (options.appid) {
		char *path;

		if (!(path = lookup_file())) {
			fprintf(stderr, "%s: could not find file '%s'\n", argv[0], options.appid);
			exit(1);
		}
		if (!parse_file(path)) {
			fprintf(stderr, "%s: could not parse file '%s'\n", argv[0], path);
			exit(1);
		}
	}
	if (output > 1) {
		fprintf(stderr, "Entries from file:\n");
		if (entry.Name)
			fprintf(stderr, "%-30s = %s\n", "Name", entry.Name);
		if (entry.Comment)
			fprintf(stderr, "%-30s = %s\n", "Comment", entry.Comment);
		if (entry.Icon)
			fprintf(stderr, "%-30s = %s\n", "Icon", entry.Icon);
		if (entry.TryExec)
			fprintf(stderr, "%-30s = %s\n", "TryExec", entry.TryExec);
		if (entry.Exec)
			fprintf(stderr, "%-30s = %s\n", "Exec", entry.Exec);
		if (entry.Terminal)
			fprintf(stderr, "%-30s = %s\n", "Terminal", entry.Terminal);
		if (entry.StartupNotify)
			fprintf(stderr, "%-30s = %s\n", "StartupNotify", entry.StartupNotify);
		if (entry.StartupWMClass)
			fprintf(stderr, "%-30s = %s\n", "StartupWMClass", entry.StartupWMClass);
	}
	if (!options.exec && !entry.Exec) {
		fprintf(stderr, "%s: no exec command\n", argv[0]);
		exit(1);
	}
	/* populate some fields */
	if (getenv("DESKTOP_STARTUP_ID") && *getenv("DESKTOP_STARTUP_ID"))
		fields.id = strdup(getenv("DESKTOP_STARTUP_ID"));

	/* open display now */
	get_display();
	/* fill out all fields */
	set_all();
	if (output > 1) {
		fprintf(stderr, "Final notify fields:\n");
		if (fields.id)
			fprintf(stderr, "%-30s = %s\n", "ID", fields.id);
		if (fields.name)
			fprintf(stderr, "%-30s = %s\n", "NAME", fields.name);
		if (fields.icon)
			fprintf(stderr, "%-30s = %s\n", "ICON", fields.icon);
		if (fields.bin)
			fprintf(stderr, "%-30s = %s\n", "BIN", fields.bin);
		if (fields.description)
			fprintf(stderr, "%-30s = %s\n", "DESCRIPTION", fields.description);
		if (fields.wmclass)
			fprintf(stderr, "%-30s = %s\n", "WMCLASS", fields.wmclass);
		if (fields.silent)
			fprintf(stderr, "%-30s = %s\n", "SILENT", fields.silent);
		if (fields.application_id)
			fprintf(stderr, "%-30s = %s\n", "APPLICATION_ID", fields.application_id);
		if (fields.desktop)
			fprintf(stderr, "%-30s = %s\n", "DESKTOP", fields.desktop);
		if (fields.screen)
			fprintf(stderr, "%-30s = %s\n", "SCREEN", fields.screen);
		if (fields.monitor)
			fprintf(stderr, "%-30s = %s\n", "MONITOR", fields.monitor);
		if (fields.timestamp)
			fprintf(stderr, "%-30s = %s\n", "TIMESTAMP", fields.timestamp);
		if (fields.pid)
			fprintf(stderr, "%-30s = %s\n", "PID", fields.pid);
		if (fields.hostname)
			fprintf(stderr, "%-30s = %s\n", "HOSTNAME", fields.hostname);
		if (fields.command)
			fprintf(stderr, "%-30s = %s\n", "COMMAND", fields.command);
	}
	launch();
	exit(0);
}
