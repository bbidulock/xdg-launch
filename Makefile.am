
bin_PROGRAMS = xdg-launch xdg-assist

xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xdg_launch_CPPFLAGS = -include autoconf.h -DNAME='"xdg-launch"'
xdg_launch_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

xdg_assist_SOURCES = xdg-assist.c
xdg_assist_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xdg_assist_CPPFLAGS = -include autoconf.h -DNAME='"xdg-assist"'
xdg_assist_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

nodist_man1_MANS  = \
	     xdg-launch.man \
	     xdg-assist.man \
	     xdg-monitor.man

xdg-launch.man: xdg-launch.pod

xdg-assist.man: xdg-assist.pod

xdg-monitor.man: xdg-monitor.pod

CLEANFILES = \
	     xdg-launch.man \
	     xdg-assist.man \
	     xdg-monitor.man

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

EXTRA_DIST = TODO xdg-launch.pod xdg-assist.pod xdg-monitor.pod
