AM_CFLAGS = $(DEBUG_CFLAGS)

bin_PROGRAMS = \
	xdg-launch \
	xdg-assist \
	xdg-list \
	xdg-types \
	xdg-which

EXTRA_PROGRAMS = \
	xdg-find \
	xdg-prefs \
	xdg-whereis

if HAVE_GIO_GLIB2_SUPPORT
bin_PROGRAMS += \
	xdg-find \
	xdg-prefs
endif

xdg_launch_CPPFLAGS = -DNAME=\"xde-launch\"
xdg_launch_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(X11_CFLAGS)
xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(GLIB2_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xdg_assist_CPPFLAGS = -DNAME=\"xde-assist\"
xdg_assist_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(X11_CFLAGS)
xdg_assist_SOURCES = xdg-assist.c
xdg_assist_LDADD = $(GLIB2_LIBS) $(X11_LIBS)

xdg_which_CPPFLAGS = -DNAME=\"xde-which\"
xdg_which_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS)
xdg_which_SOURCES = xdg-which.c
xdg_which_LDADD = $(GLIB2_LIBS)

xdg_whereis_CPPFLAGS = -DNAME=\"xde-whereis\"
xdg_whereis_SOURCES = xdg-whereis.c

xdg_list_CPPFLAGS = -DNAME=\"xde-list\"
xdg_list_SOURCES = xdg-list.c

xdg_types_CPPFLAGS = -DNAME=\"xde-types\"
xdg_types_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS)
xdg_types_SOURCES = xdg-types.c
xdg_types_LDADD = $(GLIB2_LIBS)

xdg_find_CPPFLAGS = -DNAME=\"xde-find\"
xdg_find_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS)
xdg_find_SOURCES = xdg-find.c
xdg_find_LDADD = $(GLIB2_LIBS)

xdg_prefs_CPPFLAGS = -DNAME=\"xde-prefs\"
xdg_prefs_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS)
xdg_prefs_SOURCES = xdg-prefs.c
xdg_prefs_LDADD = $(GLIB2_LIBS)

dist_bin_SCRIPTS = \
	dmenu_apps \
	dmenu_launch \
	xdg-xsession \
	xdg-autostart \
	xdg-entry \
	xdg-toolwait \
	xdg-session

AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = dmenu_apps dmenu_launch

