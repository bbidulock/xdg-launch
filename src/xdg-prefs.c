/*****************************************************************************

 Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2002-2000  OpenSS7 Corporation <http://www.openss7.com/>
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

static gboolean
read_mimeapp_file(GKeyFile *file, const gchar *directory, const char *base)
{
	char *path;
	gboolean result;

	path = g_build_filename(directory, base, NULL);
	result = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL);
	g_free(path);
	return result;
}

static gboolean
read_default_file(GKeyFile *file, char **desktops, const gchar *directory, const char *base)
{
	char **desktop;

	for (desktop = desktops; *desktop; desktop++) {
		char *name, *path;
		gboolean result;

		if (!*desktop[0])
			continue;
		name = g_strdup_printf("%s-%s", *desktop, base);
		path = g_build_filename(directory, name, NULL);
		result = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL);
		g_free(name);
		g_free(path);
		if (result)
			return result;
	}
	return read_mimeapp_file(file, directory, base);
}

gboolean
find_mimeapp_file(GKeyFile *file, const gchar *directory, const char *base)
{
	const gchar * cdir;
	const gchar * ddir;
	const gchar * const * dir;
	const gchar * const * cdirs;
	const gchar * const * ddirs;

	(void) directory;
	cdir = g_get_user_config_dir();
	ddir = g_get_user_data_dir();
	cdirs = g_get_system_config_dirs();
	ddirs = g_get_system_data_dirs();
	if (read_mimeapp_file(file, cdir, base))
		return TRUE;
	if (read_mimeapp_file(file, ddir, base))
		return TRUE;
	for (dir = cdirs; *dir; dir++) {
		if (!*dir[0])
			continue;
		if (read_mimeapp_file(file, *dir, base))
			return TRUE;
	}
	for (dir = ddirs; *dir; dir++) {
		if (!*dir[0])
			continue;
		if (read_mimeapp_file(file, *dir, base))
			return TRUE;
	}
	return FALSE;
}

/* Search order:

   $XDG_CONFIG_HOME/$desktop-mimeapps.list
   $XDG_CONFIG_HOME/mimeapps.list
   $XDG_CONFIG_DIRS/$desktop-mimeapps.list
   $XDG_CONFIG_DIRS/mimeapps.list
   $XDG_DATA_HOME/applications/$desktop-mimeapps.list
   $XDG_DATA_HOME/applications/mimeapps.list
   $XDG_DATA_DIRS/applications/$desktop-mimeapps.list
   $XDG_DATA_DIRS/applications/mimeapps.list

   $desktop variety only contain defaults, whereas the mimeapps.list can contain
   added and removed associations.
 */
gboolean
find_default_file(GKeyFile *file, char **desktops, const gchar *directory, const char *base)
{
	const gchar * cdir;
	const gchar * ddir;
	const gchar * const * dir;
	const gchar * const * cdirs;
	const gchar * const * ddirs;

	(void) directory;
	cdir = g_get_user_config_dir();
	ddir = g_get_user_data_dir();
	cdirs = g_get_system_config_dirs();
	ddirs = g_get_system_data_dirs();
	if (read_default_file(file, desktops, cdir, base))
		return TRUE;
	if (read_default_file(file, desktops, ddir, base))
		return TRUE;
	for (dir = cdirs; *dir; dir++) {
		if (!*dir[0])
			continue;
		if (read_default_file(file, desktops, *dir, base))
			return TRUE;
	}
	for (dir = ddirs; *dir; dir++) {
		if (!*dir[0])
			continue;
		if (read_default_file(file, desktops, *dir, base))
			return TRUE;
	}
	return FALSE;
}

/*
 * From freedesktop.org mime-apps specification:
 *
 * usr: $XDG_CONFIG_HOME/{$desktop-,}mimeapps.list
 * sys: $XDG_CONFIG_DIRS/{$desktop-,}mimeapps.list
 * dep: $XDG_DATA_HOME/applications/{$desktop-,}mimeapps.list (deprecated)
 * dst: $XDG_DATA_DIRS/applications/{$desktop-,}mimeapps.list
 *
 * usr = user, sys = system, dep = deprecated, dst = distro
 * $desktop is the $XDG_CURRENT_DESKTOP (array) converted to lower-case
 * $desktop files only specify default (no added or removed associations)
 *
 * Added and Removed associations:
 *
 * The suggested algorithm for listing (in preference order) the applications
 * associated to a given mimetype is:
 *
 * 1. create an empty list for the results, and a temporary empty blacklist
 * 2. visit each mimeapps.list file, in turn; a missing file is equivalent to an
 *    empty file.
 * 3. add to the results list any "Added Associations" in the mimeapps.list,
 *    excluding tiems on the blacklist.
 * 4. add to the blacklist any "Removed Associations" in the mimeapps.list.
 * 5. add to the results list any .desktop file found in the same directory as
 *    the mimeapps.list that lists the given type in its MimeType= line,
 *    excluding any desktop files already in the blacklist.  For directories
 *    based on $XDG_CONFIG_HOME and $XDG_CONFIG_DIRS, there are (by definition)
 *    no desktop files in the same directory.
 * 6. add to the blacklist the names of any desktop files found in the same
 *    directory as the mimeapps.list file (which for directories based on
 *    $XDG_CONFIG_HOME and $XDG_CONFIG_DIRS, is none).
 * 7. repeat steps 3 thru 6 for each subsequent directory.
 *
 * The above process is repeated for each mimetype from the most specific to the
 * least specific.  Note in particular that an application "Added" with a more
 * specific mime type will keep that association, even if it is "Removed" in a
 * higher-precedence directory, using a less specific type.
 *
 * Default Associations:
 *
 * The suggested algorithm for determining the default application for a given
 * mimetype is:
 *
 * 1. get the list of desktop ids for the given mimetype under the "Default
 *    Applications" group in the first mimeapps.list.
 * 2. for each desktop ID in the list, attempt to load the named desktop file,
 *    using the normal rules.
 * 3. If a valid desktop file is found, verify that it is associated with the
 *    type (as in the previous section).
 * 4. If a valid association is found, we have found the default application.
 * 5. If after all list items are handled, we have not yet found a default
 *    application, proceed to the next mimeapps.list file in the search order and
 *    repeat.
 * 6. If after all files are handled, we have not yet found a default
 *    application, select the most-preferred application (according to
 *    associations) that supports the type.
 *
 * The above process is repeated for each mimetype from the most specific to the
 * least specific.  Note in particular that an application that can handle a
 * more specific type will eb used in preference to an application explicitly
 * marked as the default for a less-specific type.
 *
 * Note that, unlike adding and removing associations, a desktop ID set as the
 * default for an application can refer to a desktop file of the same name found
 * in a directory of higher precedence.
 *
 * Note as well that the default application for a given type must be an
 * application that is associated with the type.  This means that
 * implementations should either ensure that such an association exists or add
 * one explicitly when setting an application as the default for a type.
 *
 */
void
read_mime_apps(void)
{
	GKeyFile *dfile, *afile;
	char **desktops = NULL;
	static const char *base = "mimeapps.list";
	static const char *evar = "XDG_CURRENT_DESKTOP";

	if (!(dfile = g_key_file_new())) {
		EPRINTF("could not allocate key file\n");
		exit(EXIT_FAILURE);
	}
	if (!(afile = g_key_file_new())) {
		EPRINTF("could not allocate key file\n");
		exit(EXIT_FAILURE);
	}
	if (getenv(evar) && *getenv(evar)) {
		char **desktop;

		desktops = g_strsplit(getenv(evar), ";", 0);
		for (desktop = desktops; *desktop; desktop++) {
			char *dlower;

			dlower = g_utf8_strdown(*desktop, -1);
			g_free(*desktop);
			*desktop = dlower;
		}
	}
	const gchar *cdir; const gchar *const *cdirs;
	const gchar *ddir; const gchar *const *ddirs;
	gchar **dirs;
	unsigned i, n = 0, ndir = 0;

	if ((cdir = g_get_user_config_dir()))
		ndir++;
	if ((ddir = g_get_user_data_dir()))
		ndir++;
	if ((cdirs = g_get_system_config_dirs()))
		ndir += g_strv_length((gchar **)cdirs);
	if ((ddirs = g_get_system_data_dirs()))
		ndir += g_strv_length((gchar **)ddirs);
	dirs = g_malloc0_n(ndir + 1, sizeof(gchar *));
	if (cdir)
		dirs[n++] = g_strdup(cdir);
	for (i = 0; i < g_strv_length((gchar **)cdirs); i++)
		dirs[n++] = g_strdup(cdirs[i]);
	if (ddir)
		dirs[n++] = g_build_filename(ddir, "applications", NULL);
	for (i = 0; i < g_strv_length((gchar **)ddirs); i++)
		dirs[n++] = g_build_filename(ddirs[i], "applications", NULL);
	(void) base;
	/* TODO */
	/* TODO */
	/* TODO */
	g_key_file_unref(afile);
	g_key_file_unref(dfile);
	if (desktops)
		g_strfreev(desktops);
}

static void
do_pref(int argc, char *argv[])
{
	char **type, *appid;
	GDesktopAppInfo *desk;
	GAppInfo *info;
	int setcount = 0;

	(void) argc;
	(void) argv;
	appid = calloc(PATH_MAX + 1, sizeof(*appid));
	strncpy(appid, options.appid, PATH_MAX);
	if (strstr(appid, ".desktop") != appid + strlen(appid) - 8)
		strncat(appid, ".desktop", PATH_MAX);
	if (strstr(appid, "/")) {
		if (!(desk = g_desktop_app_info_new_from_filename(appid))) {
			EPRINTF("%s: cannot find appid file %s\n", argv[0], appid);
			exit(EXIT_FAILURE);
		}
	} else {
		if (!(desk = g_desktop_app_info_new(appid))) {
			EPRINTF("%s: cannot find appid %s\n", argv[0], appid);
			exit(EXIT_FAILURE);
		}
	}
	info = G_APP_INFO(desk);

	for (type = options.types; type && *type; type++) {
		gchar *content;

		if (!strchr(*type, '/')) {
			EPRINTF("%s: categories not yet supported\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		if ((content = g_content_type_from_mime_type(*type))) {
			if (options.fallback) {
				EPRINTF("%s: could not set %s fallback for type %s\n", argv[0],
					appid, content);
			} else if (options.recommend) {
				if (g_app_info_set_as_last_used_for_type(info, content, NULL)) {
					setcount++;
					continue;
				}
				EPRINTF("%s: could not set %s lastused for type %s\n", argv[0],
					appid, content);
			} else {
				/* A problem with this gio implementation is that it only 
				   set one default application in the [Default
				   Associations] section of the mimepps.list file,
				   obliterating any existing list.  Also, it writes only
				   to the non-desktop-specific mimeapps.list file and
				   does not change the desktop-specific file, yet reads
				   the desktop-specific file: meaning that it does not
				   change the default at all if a desktop-specific file
				   exits. The only solution to this is to write the
				   default entries directly to the keyfile ourselves.

				   Another problem is that gio won't let you set a
				   non-existent application id in the default
				   associations nor the added associations, even though
				   one might want to set one.  */

				if (g_app_info_set_as_default_for_type(info, content, NULL)) {
					setcount++;
					continue;
				}
				EPRINTF("%s: could not set %s default for type %s\n", argv[0],
					appid, content);
			}
			g_free(content);
		}
	}
	if (!setcount) {
		if (options.fallback)
			EPRINTF("%s: could not set %s fallback for any type\n", argv[0], appid);
		else if (options.recommend)
			EPRINTF("%s: could not set %s lastused for any type\n", argv[0], appid);
		else
			EPRINTF("%s: could not set %s default for any type\n", argv[0], appid);
		exit(EXIT_FAILURE);
	}
	g_object_unref(desk);
	free(appid);
}

static char **
get_desktops(void)
{
	static const char *evar = "XDG_CURRENT_DESKTOP";
	char **desktops = NULL;

	if (getenv(evar) && *getenv(evar)) {
		char **desktop;

		desktops = g_strsplit(getenv(evar), ";", 0);
		for (desktop = desktops; desktop && *desktop; desktop++) {
			char *dlower;

			dlower = g_utf8_strdown(*desktop, -1);
			g_free(*desktop);
			*desktop = dlower;
		}
	}
	return desktops;
}

static char **
get_searchdirs(void)
{
	const gchar *cdir, *ddir, *const *dirs, *const *cdirs, *const *ddirs;
	char **searchdirs = NULL;
	int len = 0, i = 0;

	cdir = g_get_user_config_dir(); len++;
	ddir = g_get_user_data_dir(); len++;
	cdirs = g_get_system_config_dirs();
	for (dirs = cdirs; dirs && *dirs; dirs++) len++;
	ddirs = g_get_system_data_dirs();
	for (dirs = ddirs; dirs && *dirs; dirs++) len++;

	searchdirs = calloc(len+1, sizeof(*searchdirs));
	searchdirs[i++] = strdup(cdir);
	searchdirs[i++] = g_build_filename(ddir, "applications", NULL);
	for (dirs = cdirs; dirs && *dirs; dirs++)
		searchdirs[i++] = strdup(*dirs);
	for (dirs = ddirs; dirs && *dirs; dirs++)
		searchdirs[i++] = g_build_filename(*dirs, "applications", NULL);
	return searchdirs;
}

static void
do_exec(int argc, char *argv[])
{
	char *type;
	GList *app;
	GAppInfo *info = NULL;
	GDesktopAppInfo *desk = NULL;

	(void) argc;
	(void) argv;
	if (!(type = options.types[0])) {
		EPRINTF("a MIMETYPE or CATEGORY must be specified\n");
		exit(EXIT_FAILURE);
	}

	if (strchr(type, '/')) {
		gchar *content;

		if (!(content = g_content_type_from_mime_type(type))) {
			EPRINTF("unknown content type for mime type '%s'\n", type);
			exit(EXIT_FAILURE);
		}
		if (!options.recommend && !options.fallback) {
			if ((info = g_app_info_get_default_for_type(content, FALSE))) {
				/* execute this one */
			}
		} else if (options.recommend && !options.fallback) {
			if ((app = g_app_info_get_recommended_for_type(content))) {
				info = app->data;
				/* execute this one */
			}
		} else if (options.fallback) {
			if ((app = g_app_info_get_fallback_for_type(content))) {
				info = app->data;
				/* execute this one */
			}
		}
	} else {
		char **desktops, **desktop, **searchdirs, **dir;
		gboolean got_dfile = FALSE, got_afile = FALSE;
		GKeyFile *dfile, *afile;

		desktops = get_desktops();
		searchdirs = get_searchdirs();

		if (!(dfile = g_key_file_new())) {
			EPRINTF("could not allocate key file\n");
			exit(EXIT_FAILURE);
		}
		if (!(afile = g_key_file_new())) {
			EPRINTF("could not allocate key file\n");
			exit(EXIT_FAILURE);
		}

		for (dir = searchdirs; dir && *dir; dir++) {
			char *path, *name;

			for (desktop = desktops; desktop && *desktop; desktop++) {
				name = g_strdup_printf("%s-prefapps.list", *desktop);
				path = g_build_filename(*dir, name, NULL);
				if (!got_dfile)
					got_dfile =
					    g_key_file_load_from_file(dfile, path, G_KEY_FILE_NONE,
								      NULL);
				g_free(path);
				g_free(name);
			}
			path = g_build_filename(*dir, "prefapps.list", NULL);
			if (!got_dfile)
				got_dfile =
				    g_key_file_load_from_file(dfile, path, G_KEY_FILE_NONE, NULL);
			if (!got_afile)
				got_afile =
				    g_key_file_load_from_file(afile, path, G_KEY_FILE_NONE, NULL);
			g_free(path);
			if (got_dfile && got_afile)
				break;
		}
		if (desktops)
			g_strfreev(desktops);
		if (searchdirs)
			g_strfreev(searchdirs);

		if (!options.recommend && !options.fallback) {
			if (!desk && got_dfile) {
				char **apps, **app;

				if ((apps =
				     g_key_file_get_string_list(dfile, "Default Applications", type,
								NULL, NULL))) {
					for (app = apps; *app; app++) {
						if ((desk = g_desktop_app_info_new(*app))) {
							/* use this one */
							break;
						}
					}
					g_strfreev(apps);
				}
			}
		}
		if (!options.fallback) {
			if (!desk && got_afile) {
				char **apps, **app;

				if ((apps =
				     g_key_file_get_string_list(afile, "Added Categories", type,
								NULL, NULL))) {
					for (app = apps; *app; app++) {
						if ((desk = g_desktop_app_info_new(*app))) {
							/* use this one */
							break;
						}
					}
					g_strfreev(apps);
				}
			}
		}
		g_key_file_unref(dfile);
		g_key_file_unref(afile);

		if (!desk) {
			char *category, *categories;
			GList *apps, *app;
			const char *cat;

			category = calloc(PATH_MAX + 1, sizeof(*category));

			strncpy(category, ";", PATH_MAX);
			strncat(category, type, PATH_MAX);
			strncat(category, ";", PATH_MAX);

			if ((apps = g_app_info_get_all())) {
				categories = calloc(PATH_MAX + 1, sizeof(*categories));
				for (app = apps; app; app = app->next) {
					if ((cat = g_desktop_app_info_get_categories(app->data))) {
						strncpy(categories, ";", PATH_MAX);
						strncat(categories, cat, PATH_MAX);
						strncat(categories, ";", PATH_MAX);
						if (strstr(categories, category)) {
							desk = G_DESKTOP_APP_INFO(app->data);
							/* use this one */
							break;
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
do_list(int argc, char *argv[])
{
	char **type;

	(void) argc;
	(void) argv;
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
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2002-2009  OpenSS7 Corporation <http://www.openss7.com/>\n\
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
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2020, 2021, 2022  Monavacon Ltd.\n\
Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009  OpenSS7 Corporation.\n\
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
	(void) argc;
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
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Name:\n\
    %1$s: manage preferred applications\n\
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
General Options:\n\
    --recommend, -r\n\
        list, execute or set recommended rather than default\n\
    --fallback, -f\n\
        list or execute fallback rather than default or recommended\n\
    --pointer, -P\n\
        pass this option to xdg-launch when executing\n\
    --keyboard, -K\n\
        pass this option to xdg-launch when executing\n\
Standard Options:\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
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
			DPRINTF(1, "%s: done options processing\n", argv[0]);
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
			if (*endptr)
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
			DPRINTF(1, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			val = strtol(optarg, &endptr, 0);
			if (*endptr || val < 0)
				goto bad_option;
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			DPRINTF(1, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.output++;
				break;
			}
			val = strtol(optarg, &endptr, 0);
			if (*endptr || val < 0)
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
	DPRINTF(1, "%s: option index = %d\n", argv[0], optind);
	DPRINTF(1, "%s: option count = %d\n", argv[0], argc);
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
		DPRINTF(1, "%s: setting preferences\n", argv[0]);
		do_pref(argc, argv);
		break;
	case CommandExec:
		DPRINTF(1, "%s: executing preference\n", argv[0]);
		do_exec(argc, argv);
		break;
	case CommandList:
		DPRINTF(1, "%s: listing preferences\n", argv[0]);
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
