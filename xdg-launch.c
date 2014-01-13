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
};

struct fields {
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

Bool
get_display()
{
	if (!dpy) {
		if (!(dpy = XOpenDisplay(0)))
			fprintf(stderr, "cannot open display\n");
		else {
			screen = DefaultScreen(dpy);
			root = RootWindow(dpy, screen);
		}
	}
	return (dpy ? True : False);
}

void
find_screen()
{
	int dummy1, dummy2, x, y;
	Window root_ret, child_ret;
	unsigned int mask_ret;

	if (!get_display())
		return;

	if (!XQueryPointer(dpy, root, &root_ret, &child_ret, &x, &y,
			&dummy1, &dummy2, &mask_ret)) {
		if (!fields.screen) {
			int i, count = ScreenCount(dpy);

			for (i = 0; i < count; i++)
				if (RootWindow(dpy, i) == root_ret) {
					root = root_ret;
					screen = i;
					break;
				}
			fields.screen = calloc(64, sizeof(*fields.screen));
			snprintf(fields.screen, 64, "%d", i);
		}
	} else {
		if (!fields.screen) {
			fields.screen = calloc(64, sizeof(*fields.screen));
			snprintf(fields.screen, 64, "%d", screen);
		}
	}
	if (fields.monitor)
		goto done;
#if XINERAMA
	if (XineramaQueryExtension(dpy, &dummy1, &dummy2)) {
		int i, n;
		XineramaScreenInfo *si;

		if (!XineramaIsActive(dpy))
			goto no_xinerama;
		if (!(si = XineramaQueryScreens(dpy, &n)) || n < 1)
			goto no_xinerama;
		for (i = 0; i < n; i++)
			if (x >= si[i].x_org && x <= si[i].x_org + si[i].width &&
			    y >= si[i].y_org && y <= si[i].y_org + si[i].height)
				break;
		if (i < n) {
			monitor = si[i].screen_number;
			XFree(si);
			fields.monitor = calloc(64, sizeof(*fields.monitor));
			snprintf(fields.monitor, 64, "%d", monitor);
			goto done;
		}
		XFree(si);
	}
no_xinerama:
#endif
#if XRANDR
	if (XRRQueryExtension(dpy, &dummy1, &dummy2)) {
		XRRScreenResources *sr;
		int i, n;

		if (!(sr = XRRGetScreenResources(dpy, root)) || sr->ncrtc < 1)
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
				XFree(ci);
				break;
			}
		}
		XFree(sr);
		if (i < n) {
			monitor = i;
			fields.monitor = calloc(64, sizeof(*fields.monitor));
			snprintf(fields.monitor, 64, "%d", monitor);
			goto done;
		}
	}
no_xrandr:
#endif
	goto done;
      done:
	if (ScreenCount(dpy) == 1 && fields.monitor) {
		free(fields.screen);
		fields.screen = fields.monitor;
		fields.monitor = NULL;
	}
}

void
find_desktop()
{
	Atom atom, real;
	int format;
	unsigned long nitems, after;
	unsigned long *data;

	if (!get_display())
		return;
	atom = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	if (XGetWindowProperty(dpy, root, atom, 0L, monitor+1, False,
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
	if (data)
		XFree(data);
	atom = XInternAtom(dpy, "_NET_VISIBLE_DESKTOPS", False);
	if (XGetWindowProperty(dpy, root, atom, 0L, monitor+1, False,
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
	if (data)
		XFree(data);
}

void
find_timestamp()
{
	XEvent ev;
	Atom atom = XInternAtom(dpy, "_TIMESTAMP_PROP", False);
	unsigned char data = 'a';
	unsigned int time;

	if (!get_display())
		return;

	XSelectInput(dpy, root, PropertyChangeMask);
	XSync(dpy, False);
	XChangeProperty(dpy, root, atom, atom, 8, PropModeReplace, &data, 1);
	XMaskEvent(dpy, PropertyChangeMask, &ev);
	XSelectInput(dpy, root, NoEventMask);
	time = ev.xproperty.time;
	fields.timestamp = calloc(64, sizeof(*fields.timestamp));
	snprintf(fields.timestamp, 64, "%u", time);
}

void
find_pid()
{
	unsigned int pid = getpid();

	fields.pid = calloc(64, sizeof(*fields.pid));
	snprintf(fields.pid, 64, "%u", pid);
}

void
find_hostname()
{
	fields.hostname = calloc(64, sizeof(*fields.hostname));
	gethostname(fields.hostname, 64);
}

void
find_command()
{
}

void
find_silent()
{
	int notify = 0, wmclass = 0;

	if (entry.StartupNotify &&
		(!strcasecmp(entry.StartupNotify,"true") ||
		 !strcasecmp(entry.StartupNotify,"yes") ||
		 !strcmp(entry.StartupNotify,"1")))
		notify = 1;
	if (entry.StartupWMClass && strlen(entry.StartupWMClass))
		wmclass = 1;
	fields.silent = calloc(64, sizeof(*fields.silent));
	if (!notify && !wmclass)
		snprintf(fields.silent, 64, "%d", 1);
	else
		snprintf(fields.silent, 64, "%d", 0);
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
", argv[0], defaults.launcher, defaults.sequence, defaults.hostname, defaults.monitor, defaults.screen, defaults.desktop, defaults.timestamp, defaults.name, defaults.icon, defaults.binary, defaults.description, defaults.wmclass, defaults.silent, defaults.pid);
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
				     "L:l:S:n:m:s:p:w:t:N:i:b:d:W:q:a:x:f:u:D::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "L:l:S:n:m:s:p:w:t:N:i:b:d:W:q:a:x:f:u:DvhVC?");
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
			break;
		case 's':	/* -s, --screen SCREEN */
			defaults.screen = options.screen = strdup(optarg);
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
	/* override fields */
	if (options.name) {
		free(entry.Name);
		entry.Name = strdup(options.name);
	}
	if (options.description) {
		free(entry.Comment);
		entry.Comment = strdup(options.description);
	}
	if (options.icon) {
		free(entry.Icon);
		entry.Icon = strdup(options.icon);
	}
	if (options.exec) {
		free(entry.Exec);
		free(entry.TryExec);
		entry.Exec = strdup(options.exec);
		entry.TryExec = NULL;
	}
	if (options.wmclass) {
		free(entry.StartupWMClass);
		entry.StartupWMClass = strdup(options.wmclass);
	}
	if (output > 1) {
		fprintf(stderr, "Entries after override:\n");
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
	if (!entry.Exec) {
		fprintf(stderr, "%s: no exec command\n", argv[0]);
		exit(1);
	}
	/* populate some fields */
	if (getenv("DESKTOP_STARTUP_ID") && *getenv("DESKTOP_STARTUP_ID"))
		fields.id = strdup(getenv("DESKTOP_STARTUP_ID"));
	if (entry.Name)
		fields.name = strdup(entry.Name);
	if (entry.Icon) {
		char *p;

		if (strrchr(entry.Icon, '/'))
			fields.icon = strdup(p+1);
		else
			fields.icon = strdup(entry.Icon);
		if ((p = strstr(fields.icon, ".xpm")))
			*p = '\0';
		if ((p = strstr(fields.icon, ".png")))
			*p = '\0';
		if ((p = strstr(fields.icon, ".svg")))
			*p = '\0';
		if ((p = strstr(fields.icon, ".jpg")))
			*p = '\0';
	}
	if (entry.Comment)
		fields.description = strdup(entry.Comment);
	if (entry.StartupWMClass)
		fields.wmclass = strdup(entry.StartupWMClass);
	if (options.silent)
		fields.silent = strdup(options.silent);
	if (options.appid) {
		char *p;

		if ((p = strrchr(options.appid, '/')))
			fields.application_id = strdup(p+1);
		else
			fields.application_id = strdup(options.appid);
		if ((p = strstr(fields.application_id, ".desktop")))
			*p = '\0';
	}
	if (options.desktop)
		fields.desktop = strdup(options.desktop);
	if (options.monitor) {
		fields.monitor = strdup(options.monitor);
		monitor = strtoul(fields.monitor, NULL, 0);
		fields.screen = strdup(options.monitor);
	}
	if (options.screen) {
		free(fields.screen);
		fields.screen = strdup(options.screen);
	}
	if (fields.id) {
		char *p, *q;

		if ((p = strstr(fields.id, "_TIME")))
			if (strtoul(p + 5, NULL, 0))
				fields.timestamp = strdup(p + 5);
		if ((q = strchr(fields.id, '/'))) {
			*q = '\0';
			free(options.launcher);
			options.launcher = strdup(fields.id);
			*q = '/';
			while ((p = strchr(options.launcher, '|')))
				*p = '/';
		}
		if ((p = strchr(fields.id, '/')) &&
		    (p = strchr(p + 1, '/')) && (q = strchr(p + 1, '-'))) {
			*q = '\0';
			if (strtoul(p + 1, NULL, 0))
				fields.pid = strdup(p + 1);
			*q = '-';
		}
		if ((p = strchr(fields.id, '/')) && (q = strchr(p + 1, '/'))) {
			*q = '\0';
			if (strlen(p + 1)) {
				fields.bin = strdup(p + 1);
				while ((p = strchr(fields.bin, '|')))
					*p = '/';
				if (!options.launchee)
					options.launchee = strdup(fields.bin);
			}
			*q = '/';
		}
	}
	if (options.timestamp && strtoul(options.timestamp, NULL, 0)) {
		free(fields.timestamp);
		fields.timestamp = strdup(options.timestamp);
	}
	if (options.binary) {
		free(fields.bin);
		fields.bin = strdup(options.binary);
	}
	if (options.pid) {
		free(fields.pid);
		fields.pid = strdup(options.pid);
	}
	if (options.hostname)
		fields.hostname = strdup(options.hostname);
	if (entry.Exec)
		fields.command = strdup(entry.Exec);
	if (output > 1) {
		fprintf(stderr, "Initial notify fields:\n");
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
	/* fill out some extra fields */
	if (!fields.monitor || !fields.screen)
		find_screen();
	if (!fields.desktop)
		find_desktop();
	if (!fields.timestamp)
		find_timestamp();
	if (!fields.pid)
		find_pid();
	if (!fields.hostname)
		find_hostname();
	if (!fields.command)
		find_command();
	if (!fields.silent)
		find_silent();
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

	exit(0);
}
