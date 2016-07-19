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
#ifdef GIO_GLIB2_SUPPORT
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
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
	CommandDefault,
	CommandPref,
	CommandExec,
	CommandList,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} CommandType;

typedef struct {
	int debug;
	int output;
	CommandType command;
	int skipdot;
	int skiptilde;
	int showdot;
	int showtilde;
	int recommend;
	int fallback;
	char *appid;
	char **types;
	Bool pointer;
	Bool keyboard;
	int button;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.command = CommandDefault,
	.skipdot = 0,
	.skiptilde = 0,
	.showdot = 0,
	.showtilde = 0,
	.recommend = 0,
	.fallback = 0,
	.appid = NULL,
	.types = NULL,
	.pointer = False,
	.keyboard = False,
	.button = 0,
};

static void
do_pref(int argc, char *argv[])
{
}

static void
do_exec(int argc, char *argv[])
{
}

static void
do_list(int argc, char *argv[])
{
	char **type;

	for (type = options.types; type && *type; type++) {
		GList *apps, *app;
		GAppInfo *info;
		GDesktopAppInfo *desk;

		if (strchr(*type, '/')) {
			gchar *content;

			if ((content = g_content_type_from_mime_type(*type))) {
				fprintf(stdout, "%s:\n", content);
				if ((info = g_app_info_get_default_for_type(content, FALSE))) {
					fprintf(stdout, "\tdefault:\n");
					fprintf(stdout, "\t\t%s:\n", g_app_info_get_id(info));
				}
				if (options.recommend) {
					if ((apps = g_app_info_get_recommended_for_type(content))) {
						fprintf(stdout, "\trecommended:\n");
						for (app = apps; app; app = app->next) {
							info = app->data;
							fprintf(stdout, "\t\t%s\n", g_app_info_get_id(info));
						}
						g_list_free(apps);
					}
				}
				if (options.fallback) {
					if ((apps = g_app_info_get_fallback_for_type(content))) {
						fprintf(stdout, "\tfallback:\n");
						for (app = apps; app; app = app->next) {
							info = app->data;
							fprintf(stdout, "\t\t%s\n", g_app_info_get_id(info));
						}
						g_list_free(apps);
					}
				}
				g_free(content);
			}
		} else {
			char *category = calloc(PATH_MAX + 1, sizeof(*category));

			strncpy(category, ";", PATH_MAX);
			strncat(category, *type, PATH_MAX);
			strncat(category, ";", PATH_MAX);

			fprintf(stdout, "%s:\n", *type);

			if ((apps = g_app_info_get_all())) {
				char *categories = calloc(PATH_MAX + 1, sizeof(*categories));
				const char *cat;

				for (app = apps; app; app = app->next) {
					desk = G_DESKTOP_APP_INFO(app->data);
					if ((cat = g_desktop_app_info_get_categories(desk))) {
						strncpy(categories, ";", PATH_MAX);
						strncat(categories, cat, PATH_MAX);
						strncat(categories, ";", PATH_MAX);
						if (strstr(categories, category)) {
							info = G_APP_INFO(desk);
							fprintf(stdout, "\t%s\n", g_app_info_get_id(info));
						}
					}
				}
				g_list_free(apps);
				free(categories);
			}
			free(category);
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
    %1$s [options] {-p|--pref} [--] APPID[;APPID[...]] [TYPE [...]]\n\
    %1$s [options] {-e|--exec} [--] TYPE [...]\n\
    %1$s [options] {-l|--list} [--] TYPE [...]\n\
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
    %1$s [options] {-p|--pref} [--] APPID[;APPID[...]] [TYPE [...]]\n\
    %1$s [options] {-e|--exec} [--] TYPE [...]\n\
    %1$s [options] {-l|--list} [--] TYPE [...]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    APPID\n\
        the application identifier of XDG application to set\n\
    TYPE\n\
        XDG category or mime type to list, execute or set defaults\n\
Command Options:\n\
    --pref, -p\n\
        set preferred application for types\n\
    --exec, -e\n\
        execute preferred application for type\n\
    --list, -l\n\
        list preferred application for types\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
General Options:\n\
    --skip-dot, -o\n\
        skip directories that start with a dot\n\
    --skip-tilde, -t\n\
        skip directories that start with a tilde\n\
    --show-dot, -O\n\
        print dots instead of full path\n\
    --show-tilde, -T\n\
        print tildes instead of full path\n\
    --recommend, -r\n\
        list, execute or set recommended rather than default\n\
  - --fallback, -f\n\
        list or execute fallback rather than default\n\
", argv[0]);
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"skip-dot",	no_argument,		NULL, 'o'},
			{"skip-tilde",	no_argument,		NULL, 't'},
			{"show-dot",	no_argument,		NULL, 'O'},
			{"show-tilde",	no_argument,		NULL, 'T'},
			{"recommend",	no_argument,		NULL, 'r'},
			{"fallback",	no_argument,		NULL, 'f'},

			{"pointer",	no_argument,		NULL, 'P'},
			{"keyboard",	no_argument,		NULL, 'K'},
			{"button",	required_argument,	NULL, 'b'},

			{"pref",	no_argument,		NULL, 'p'},
			{"exec",	no_argument,		NULL, 'e'},
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

		c = getopt_long_only(argc, argv, "otOTrfPKb:pelD::v::hVCH?", long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "otOTrfPKb:pelDvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

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
		case 'r':	/* -r, --recommend */
			options.recommend = 1;
			break;
		case 'f':	/* -f, --fallback */
			options.fallback = 1;
			break;

		case 'P':	/* -P, --pointer */
			options.pointer = True;
			options.keyboard = False;
			if (!options.button)
				options.button = 1;
			break;
		case 'K':	/* -K, --keyboard */
			options.pointer = False;
			options.keyboard = True;
			options.button = 0;
			break;
		case 'b':	/* -b, --button BUTTON */
			val = strtoul(optarg, &endptr, 0);
			if (endptr && *endptr)
				goto bad_option;
			if (val < 0 || val > 8)
				goto bad_option;
			if ((options.button = val)) {
				options.pointer = True;
				options.keyboard = False;
			} else {
				options.pointer = False;
				options.keyboard = True;
			}
			break;


		case 'p':	/* -p, --pref */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandPref;
			options.command = CommandPref;
			break;
		case 'e':	/* -e, --exec */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandExec;
			options.command = CommandExec;
			break;
		case 'l':	/* -l, --list */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandList;
			options.command = CommandList;
			break;

		case 'D':	/* -D, --debug [level] */
			DPRINTF("%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			DPRINTF("%s: increasing output verbosity\n", argv[0]);
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
					EPRINTF("%s: syntax error near '", argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					EPRINTF("%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			}
		      bad_usage:
			usage(argc, argv);
			exit(2);
		}
	}
	DPRINTF("%s: option index = %d\n", argv[0], optind);
	DPRINTF("%s: option count = %d\n", argv[0], argc);
	if (optind < argc) {
		int n = argc - optind, j = 0;

		if (command == CommandPref) {
			free(options.appid);
			options.appid = strdup(argv[optind++]);
		}
		free(options.types);
		options.types = calloc(n + 1, sizeof(*options.types));
		while (optind < argc)
			options.types[j++] = strdup(argv[optind++]);
	} else if (command == CommandPref) {
		EPRINTF("%s: APPID must be specified\n", argv[0]);
		goto bad_usage;
	} else if (command == CommandExec || command == CommandList) {
		EPRINTF("%s: at least one TYPE must be specified\n", argv[0]);
		goto bad_usage;
	}

	switch (command) {
	case CommandDefault:
		goto bad_usage;
	case CommandPref:
		DPRINTF("%s: setting preferences\n", argv[0]);
		do_pref(argc, argv);
		break;
	case CommandExec:
		DPRINTF("%s: executing preference\n", argv[0]);
		do_exec(argc, argv);
		break;
	case CommandList:
		DPRINTF("%s: listing preferences\n", argv[0]);
		do_list(argc, argv);
		break;
	case CommandHelp:
		DPRINTF("%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF("%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF("%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
