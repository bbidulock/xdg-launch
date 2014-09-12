
EXTRA_DIST = TODO

bin_PROGRAMS = xdg-launch xdg-assist xdg-monitor xdg-which
EXTRA_PROGRAMS =

xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xdg_launch_CPPFLAGS = -include autoconf.h -DNAME='"xdg-launch"'
xdg_launch_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

xdg_assist_SOURCES = xdg-assist.c
xdg_assist_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xdg_assist_CPPFLAGS = -include autoconf.h -DNAME='"xdg-assist"'
xdg_assist_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

xdg_monitor_SOURCES = xdg-monitor.c
xdg_monitor_LDADD = $(GTK2_LIBS) $(NOTIFY_LIBS) $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xdg_monitor_CPPFLAGS = -include autoconf.h -DNAME='"xdg-monitor"'
xdg_monitor_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS) $(NOTIFY_CFLAGS) $(GTK2_CFLAGS)

xdg_which_SOURCES = xdg-which.c
xdg_which_CPPFLAGS = -include autoconf.h -DNAME='"xdg-which"'

dist_bin_SCRIPTS = dmenu_apps dmenu_launch xdg-xsession xdg-autostart

AM_STD_OPTIONS_EXEMPT = dmenu_apps dmenu_launch

dist_app_DATA = dmenu_launch.desktop xdg-launch.desktop

dist_auto_DATA = xdg-monitor.desktop

xdg_man_files = \
	     xdg-launch.man \
	     xdg-assist.man \
	     xdg-monitor.man \
	     dmenu_launch.man \
	     xdg-xsession.man \
	     xdg-autostart.man \
	     xdg-whereis.man \
	     xdg-which.man

xdg_pod_files = \
	     xdg-launch.pod \
	     xdg-assist.pod \
	     xdg-monitor.pod \
	     dmenu_launch.pod \
	     xdg-xsession.pod \
	     xdg-autostart.pod \
	     xdg-whereis.pod \
	     xdg-which.pod

nodist_man1_MANS = $(xdg_man_files)
CLEANFILES = $(xdg_man_files)
EXTRA_DIST += $(xdg_pod_files)

SUFFIXES = .man .pod

.pod.man:
	pod2man --center="User Commands" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="1" \
		$< >$@

