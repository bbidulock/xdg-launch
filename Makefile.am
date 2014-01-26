

AM_CPPFLAGS = -include autoconf.h -DNAME='"xdg-launch"'
AM_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

bin_PROGRAMS = xdg-launch

xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

nodist_man1_MANS  = \
	     xdg-launch.man

xdg-launch.man: xdg-launch.pod

CLEANFILES = xdg-launch.man

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

EXTRA_DIST = TODO xdg-launch.pod
