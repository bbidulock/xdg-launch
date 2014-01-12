

AM_CPPFLAGS = -include autoconf.h -DNAME='"xdg-launch"'
AM_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

bin_PROGRAMS = xdg-launch

xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

man1_MANS  = xdg-launch.man

EXTRA_DIST = LICENSE README TODO ChangeLog
