AM_CFLAGS = $(DEBUG_CFLAGS)

bin_PROGRAMS = \
	xdg-launch \
	xdg-assist \
	xdg-monitor \
	xdg-which \
	xdg-show

EXTRA_PROGRAMS = \
	xdg-whereis

xdg_launch_CPPFLAGS = -DNAME=\"xde-launch\"
xdg_launch_CFLAGS = $(AM_CFLAGS) $(RU_CFLAGS) $(SN_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(X11_CFLAGS)
xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(RU_LIBS) $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xdg_assist_CPPFLAGS = -DNAME=\"xde-assist\"
xdg_assist_CFLAGS = $(AM_CFLAGS) $(GTK2_CFLAGS) $(SN_CFLAGS) $(X11_CFLAGS)
xdg_assist_SOURCES = xdg-assist.c
xdg_assist_LDADD = $(GTK2_LIBS) $(SN_LIBS) $(X11_LIBS)

xdg_monitor_CPPFLAGS = -DNAME=\"xde-monitor\"
xdg_monitor_CFLAGS = $(AM_CFLAGS) $(GTK2_CFLAGS) $(SN_CFLAGS) $(NOTIFY_CFLAGS) $(X11_CFLAGS)
xdg_monitor_SOURCES = xdg-monitor.c
xdg_monitor_LDADD = $(GTK2_LIBS) $(NOTIFY_LIBS) $(X11_LIBS)

xdg_which_CPPFLAGS = -DNAME=\"xde-which\"
xdg_which_SOURCES = xdg-which.c

xdg_whereis_CPPFLAGS = -DNAME=\"xde-whereis\"
xdg_whereis_SOURCES = xdg-whereis.c

xdg_show_CPPFLAGS = -DNAME=\"xde-show\"
xdg_show_CFLAGS = $(AM_CFLAGS) $(GTK2_CFLAGS) $(SN_CFLAGS) $(NOTIFY_CFLAGS) $(X11_CFLAGS)
xdg_show_SOURCES = xdg-show.c
xdg_show_LDADD = $(GTK2_LIBS) $(NOTIFY_LIBS) $(X11_LIBS)

dist_bin_SCRIPTS = \
	dmenu_apps \
	dmenu_launch \
	xdg-xsession \
	xdg-autostart \
	xdg-entry \
	xdg-toolwait

AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = dmenu_apps dmenu_launch

