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

#ifdef GIO_GLIB2_SUPPORT
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
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

typedef enum {
	CommandDefault,
	CommandWhich,
	CommandList,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} CommandType;

typedef enum {
	WhichApplication,
	WhichAutostart,
	WhichXsession,
} WhichEntry;

typedef struct {
	int debug;
	int output;
	CommandType command;
	WhichEntry which;
	int all;
	int skipdot;
	int skiptilde;
	int showdot;
	int showtilde;
	int mimetypes;
	char **appids;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.command = CommandDefault,
	.which = WhichApplication,
	.all = 0,
	.skipdot = 0,
	.skiptilde = 0,
	.showdot = 0,
	.showtilde = 0,
	.mimetypes = 1,
	.appids = NULL,
};

static void
output_types(const char *path)
{
#ifdef GIO_GLIB2_SUPPORT
	GDesktopAppInfo *info;
	const char **types;

	if ((info = g_desktop_app_info_new_from_filename(path)) &&
	    (types = g_app_info_get_supported_types(G_APP_INFO(info))))
		while (*types)
			fprintf(stdout, "\t%s\n", *types++);
#endif
}

static int
output_path(const char *home, const char *path)
{
	char *p, *line, *cdir;

	if (options.skipdot && *path == '.')
		return 0;
	if (options.skiptilde && (*path == '~' || ((p = strstr(path, home)) && p == path)))
		return 0;
	cdir = calloc(PATH_MAX + 1, sizeof(*cdir));
	if (!getcwd(cdir, PATH_MAX))
		strncpy(cdir, ".", PATH_MAX);
	line = calloc(PATH_MAX + 1, sizeof(*line));
	if (*path == '~' && !options.showtilde) {
		strncpy(line, home, PATH_MAX);
		strncat(line, path + 1, PATH_MAX);
	} else if (*path == '.' && !options.showdot) {
		strncpy(line, cdir, PATH_MAX);
		strncat(line, path + 1, PATH_MAX);
	} else if (options.showtilde && ((p = strstr(path, home)) && p == path)) {
		strncpy(line, "~", PATH_MAX);
		strncat(line, path + strlen(home), PATH_MAX);
	} else if (options.showdot && ((p = strstr(path, cdir)) && p == path)) {
		strncpy(line, ".", PATH_MAX);
		strncat(line, path + strlen(cdir), PATH_MAX);
	} else
		strncpy(line, path, PATH_MAX);
	if (options.output > 0)
		fprintf(stdout, "%s\n", line);
	free(cdir);
	free(line);
	if (options.mimetypes)
		output_types(path);
	return 1;
}

/** @brief look up the file from APPID.
  *
  * We search in the applications directories first (i.e.
  * /usr/share/applications) and then in the autostart directories (e.g.
  * /etc/xdg/autostart).
  */
static void
lookup_file(char *name)
{
	char *path = NULL, *appid, *home;
	struct stat st;

	home = getenv("HOME") ? : ".";
	appid = calloc(PATH_MAX + 1, sizeof(*appid));
	strncat(appid, name, PATH_MAX);
	if (strstr(appid, ".desktop") != appid + strlen(appid) - 8)
		strncat(appid, ".desktop", PATH_MAX);
	if ((*appid != '/') && (*appid != '.')) {
		char *list, *dirs, *env;

		list = calloc(PATH_MAX + 1, sizeof(*list));
		if (options.which == WhichAutostart) {
			dirs = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";
			if ((env = getenv("XDG_CONFIG_HOME")) && *env)
				strncpy(list, env, PATH_MAX);
			else {
				strncpy(list, home, PATH_MAX);
				strncat(list, "/.config", PATH_MAX);
			}
		} else {
			dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
			if ((env = getenv("XDG_DATA_HOME")) && *env)
				strncpy(list, env, PATH_MAX);
			else {
				strncpy(list, home, PATH_MAX);
				strncat(list, "/.local/share", PATH_MAX);
			}
		}
		if (options.skiptilde) {
			strncpy(list, dirs, PATH_MAX);
		} else {
			strncat(list, ":", PATH_MAX);
			strncat(list, dirs, PATH_MAX);
		}
		for (dirs = list; !path && *dirs;) {
			char *p;

			if (*dirs == '.' && options.skipdot)
				continue;
			if (*dirs == '~' && options.skiptilde)
				continue;
			if ((p = strchr(dirs, ':'))) {
				*p = '\0';
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, PATH_MAX + 1);
			switch (options.which) {
			case WhichApplication:
				strncat(path, "/applications/", PATH_MAX);
				break;
			case WhichAutostart:
				strncat(path, "/autostart/", PATH_MAX);
				break;
			case WhichXsession:
				strncat(path, "/xsessions/", PATH_MAX);
				break;
			}
			strncat(path, appid, PATH_MAX);
			DPRINTF(1, "%s: checking '%s'\n", NAME, path);
			if (!stat(path, &st)) {
				if (output_path(home, path) && !options.all) {
					free(list);
					free(appid);
					free(path);
					return;
				}
			}
			free(path);
			path = NULL;
		}
		switch (options.which) {
		case WhichApplication:
			break;
		case WhichAutostart:
			/* only look in autostart for autostart entries */
		case WhichXsession:
			/* only look in xsession for xsession entries */
			free(list);
			free(appid);
			return;
		}
#if 1
		/* next look in fallback subdirectory */
		dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
		if ((env = getenv("XDG_DATA_HOME")) && *env)
			strncpy(list, env, PATH_MAX);
		else {
			strncpy(list, home, PATH_MAX);
			strncat(list, "/.local/share", PATH_MAX);
		}
		if (options.skiptilde) {
			strncpy(list, dirs, PATH_MAX);
		} else {
			strncpy(list, ":", PATH_MAX);
			strncat(list, dirs, PATH_MAX);
		}
		for (dirs = list; !path && *dirs;) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = '\0';
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, PATH_MAX + 1);
			strncat(path, "/fallback/", PATH_MAX);
			strncat(path, appid, PATH_MAX);
			DPRINTF(1, "%s: checking '%s'\n", NAME, path);
			if (!stat(path, &st)) {
				if (output_path(home, path) && !options.all) {
					free(list);
					free(appid);
					free(path);
					return;
				}
			}
			free(path);
			path = NULL;
		}
#endif
		/* next look in autostart subdirectory */
		dirs = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";
		if ((env = getenv("XDG_CONFIG_HOME")) && *env)
			strncpy(list, env, PATH_MAX);
		else {
			strncpy(list, home, PATH_MAX);
			strncat(list, "/.config", PATH_MAX);
		}
		if (options.skiptilde) {
			strncpy(list, dirs, PATH_MAX);
		} else {
			strncat(list, ":", PATH_MAX);
			strncat(list, dirs, PATH_MAX);
		}
		for (dirs = list; !path && *dirs;) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = '\0';
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, PATH_MAX + 1);
			strncat(path, "/autostart/", PATH_MAX);
			strncat(path, appid, PATH_MAX);
			DPRINTF(1, "%s: checking '%s'\n", NAME, path);
			if (!stat(path, &st)) {
				if (output_path(home, path) && !options.all) {
					free(list);
					free(appid);
					free(path);
					return;
				}
			}
			free(path);
			path = NULL;
		}
		/* next look in xsessions subdirectory */
		dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
		if ((env = getenv("XDG_DATA_HOME")) && *env)
			strncpy(list, env, PATH_MAX);
		else {
			strncpy(list, home, PATH_MAX);
			strncat(list, "/.local/share", PATH_MAX);
		}
		if (options.skiptilde) {
			strncpy(list, dirs, PATH_MAX);
		} else {
			strncpy(list, ":", PATH_MAX);
			strncat(list, dirs, PATH_MAX);
		}
		for (dirs = list; !path && *dirs;) {
			char *p;

			if ((p = strchr(dirs, ':'))) {
				*p = '\0';
				path = strdup(dirs);
				dirs = p + 1;
			} else {
				path = strdup(dirs);
				*dirs = '\0';
			}
			path = realloc(path, PATH_MAX + 1);
			strncat(path, "/xsessions/", PATH_MAX);
			strncat(path, appid, PATH_MAX);
			DPRINTF(1, "%s: checking '%s'\n", NAME, path);
			if (!stat(path, &st)) {
				if (output_path(home, path) && !options.all) {
					free(list);
					free(appid);
					free(path);
					return;
				}
			}
			free(path);
			path = NULL;
		}
		free(list);
	} else {
		path = strdup(appid);
		DPRINTF(1, "%s: checking '%s'\n", NAME, path);
		if (!stat(path, &st)) {
			output_path(home, path);
		}
		free(path);
	}
	free(appid);
	return;
}

static void
list_paths(void)
{
	char *path = NULL, *home;
	char *list, *dirs, *env;
	struct stat st;

	home = getenv("HOME") ? : ".";

	list = calloc(PATH_MAX + 1, sizeof(*list));
	if (options.which == WhichAutostart) {
		dirs = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";
		if ((env = getenv("XDG_CONFIG_HOME")) && *env)
			strncpy(list, env, PATH_MAX);
		else {
			strncpy(list, home, PATH_MAX);
			strncat(list, "/.config", PATH_MAX);
		}
	} else {
		dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
		if ((env = getenv("XDG_DATA_HOME")) && *env)
			strncpy(list, env, PATH_MAX);
		else {
			strncpy(list, home, PATH_MAX);
			strncat(list, "/.local/share", PATH_MAX);
		}
	}
	if (options.skiptilde) {
		strncpy(list, dirs, PATH_MAX);
	} else {
		strncat(list, ":", PATH_MAX);
		strncat(list, dirs, PATH_MAX);
	}
	for (dirs = list; !path && *dirs;) {
		char *p;

		if (*dirs == '.' && options.skipdot)
			continue;
		if (*dirs == '~' && options.skiptilde)
			continue;
		if ((p = strchr(dirs, ':'))) {
			*p = '\0';
			path = strdup(dirs);
			dirs = p + 1;
		} else {
			path = strdup(dirs);
			*dirs = '\0';
		}
		path = realloc(path, PATH_MAX + 1);
		switch (options.which) {
		case WhichApplication:
			strncat(path, "/applications", PATH_MAX);
			break;
		case WhichAutostart:
			strncat(path, "/autostart", PATH_MAX);
			break;
		case WhichXsession:
			strncat(path, "/xsessions", PATH_MAX);
			break;
		}
		DPRINTF(1, "%s: checking '%s'\n", NAME, path);
		if (!stat(path, &st))
			output_path(home, path);
		free(path);
		path = NULL;
	}
	switch (options.which) {
	case WhichApplication:
		break;
	case WhichAutostart:
		/* only look in autostart for autostart entries */
	case WhichXsession:
		/* only look in xsession for xsession entries */
		free(list);
		return;
	}
	/* next look in fallback subdirectory */
	dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
	if ((env = getenv("XDG_DATA_HOME")) && *env)
		strncpy(list, env, PATH_MAX);
	else {
		strncpy(list, home, PATH_MAX);
		strncat(list, "/.local/share", PATH_MAX);
	}
	if (options.skiptilde) {
		strncpy(list, dirs, PATH_MAX);
	} else {
		strncpy(list, ":", PATH_MAX);
		strncat(list, dirs, PATH_MAX);
	}
	for (dirs = list; !path && *dirs;) {
		char *p;

		if ((p = strchr(dirs, ':'))) {
			*p = '\0';
			path = strdup(dirs);
			dirs = p + 1;
		} else {
			path = strdup(dirs);
			*dirs = '\0';
		}
		path = realloc(path, PATH_MAX + 1);
		strncat(path, "/fallback", PATH_MAX);
		DPRINTF(1, "%s: checking '%s'\n", NAME, path);
		if (!stat(path, &st))
			output_path(home, path);
		free(path);
		path = NULL;
	}
	/* next look in autostart subdirectory */
	dirs = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";
	if ((env = getenv("XDG_CONFIG_HOME")) && *env)
		strncpy(list, env, PATH_MAX);
	else {
		strncpy(list, home, PATH_MAX);
		strncat(list, "/.config", PATH_MAX);
	}
	if (options.skiptilde) {
		strncpy(list, dirs, PATH_MAX);
	} else {
		strncat(list, ":", PATH_MAX);
		strncat(list, dirs, PATH_MAX);
	}
	for (dirs = list; !path && *dirs;) {
		char *p;

		if ((p = strchr(dirs, ':'))) {
			*p = '\0';
			path = strdup(dirs);
			dirs = p + 1;
		} else {
			path = strdup(dirs);
			*dirs = '\0';
		}
		path = realloc(path, PATH_MAX + 1);
		strncat(path, "/autostart", PATH_MAX);
		DPRINTF(1, "%s: checking '%s'\n", NAME, path);
		if (!stat(path, &st))
			output_path(home, path);
		free(path);
		path = NULL;
	}
	/* next look in xsessions subdirectory */
	dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
	if ((env = getenv("XDG_DATA_HOME")) && *env)
		strncpy(list, env, PATH_MAX);
	else {
		strncpy(list, home, PATH_MAX);
		strncat(list, "/.local/share", PATH_MAX);
	}
	if (options.skiptilde) {
		strncpy(list, dirs, PATH_MAX);
	} else {
		strncpy(list, ":", PATH_MAX);
		strncat(list, dirs, PATH_MAX);
	}
	for (dirs = list; !path && *dirs;) {
		char *p;

		if ((p = strchr(dirs, ':'))) {
			*p = '\0';
			path = strdup(dirs);
			dirs = p + 1;
		} else {
			path = strdup(dirs);
			*dirs = '\0';
		}
		path = realloc(path, PATH_MAX + 1);
		strncat(path, "/xsessions", PATH_MAX);
		DPRINTF(1, "%s: checking '%s'\n", NAME, path);
		if (!stat(path, &st))
			output_path(home, path);
		free(path);
		path = NULL;
	}
	free(list);
	return;
}

static void
do_which(int argc, char *argv[])
{
	char **appid;

	for (appid = options.appids; appid && *appid; appid++)
		lookup_file(*appid);
}

static void
do_list(int argc, char *argv[])
{
	list_paths();
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
    %1$s [options] [--] [APPID]\n\
    %1$s [options] {-l|--list}\n\
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
    %1$s [options] [--] [APPID]\n\
    %1$s [options] {-l|--list}\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    APPID\n\
        the application identifier of XDG application to locate\n\
Command Options:\n\
    --list, -l\n\
        print list of effective lookup paths\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
General Options:\n\
    --xsession, -X\n\
        interpret entry as xsession instead of application\n\
    --autostart, -U\n\
        interpret entry as autostart instead of application\n\
    --all, -a\n\
        print all matching desktop entries\n\
    --skip-dot, -o\n\
        skip directories that start with a dot\n\
    --skip-tilde, -t\n\
        skip directories that start with a tilde\n\
    --show-dot, -O\n\
        print dots instead of full path\n\
    --show-tilde, -T\n\
        print tildes instead of full path\n\
", argv[0]);
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"xsession",	no_argument,		NULL, 'X'},
			{"autostart",	no_argument,		NULL, 'U'},
			{"all",		no_argument,		NULL, 'a'},
			{"skip-dot",	no_argument,		NULL, 'o'},
			{"skip-tilde",	no_argument,		NULL, 't'},
			{"show-dot",	no_argument,		NULL, 'O'},
			{"show-tilde",	no_argument,		NULL, 'T'},
			{"mime-types",	no_argument,		NULL, 'm'},
			{"list",	no_argument,		NULL, 'l'},

			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "XUaotOTmlD::v::hVCH?", long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "XUaotOTmlDvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'X':	/* -X, --xsession */
			if (options.which != WhichApplication)
				goto bad_option;
			options.which = WhichXsession;
			break;
		case 'U':	/* -U, --autostart */
			if (options.which != WhichApplication)
				goto bad_option;
			options.which = WhichAutostart;
			break;
		case 'a':	/* -a, --all */
			options.all = 1;
			break;
		case 'o':	/* -o, --skip-dot */
			options.skipdot = 1;
			break;
		case 't':	/* -t, --skip-tilde */
			options.skiptilde = 1;
			break;
		case 'O':	/* -O, --show-dot */
			options.showdot = 1;
			break;
		case 'T':	/* -T, --show-tilde */
			options.showtilde = 1;
			break;
		case 'm':	/* -m, --mime-types */
			options.mimetypes = 1;
			break;
		case 'l':	/* -l, --list */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandList;
			options.command = CommandList;
			break;

		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			options.debug = val;
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
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandVersion;
			options.command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandCopying;
			options.command = CommandCopying;
			break;
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
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
			}
		      bad_usage:
			usage(argc, argv);
			exit(2);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind < argc) {
		int n = argc - optind, j = 0;

		options.appids = calloc(n + 1, sizeof(*options.appids));
		while (optind < argc)
			options.appids[j++] = strdup(argv[optind++]);
	} else if (command == CommandDefault) {
		fprintf(stderr, "%s: APPID must be specified\n", argv[0]);
		goto bad_usage;
	}

	switch (command) {
	case CommandDefault:
		command = CommandWhich;
		/* fall thru */
	case CommandWhich:
		DPRINTF(1, "%s: running which\n", argv[0]);
		do_which(argc, argv);
		break;
	case CommandList:
		DPRINTF(1, "%s: running list\n", argv[0]);
		do_list(argc, argv);
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
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
