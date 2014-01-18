

AM_CPPFLAGS = -include autoconf.h -DNAME='"xdg-launch"'
AM_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(SN_CFLAGS)

bin_PROGRAMS = xdg-launch xde-keys xde-theme

xdg_launch_SOURCES = xdg-launch.c
xdg_launch_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_keys_SOURCES = xde-keys.c
xde_keys_LDADD = $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_theme_SOURCES = xde-theme.c
xde_theme_LDADD =  $(SN_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)


man1_MANS  = \
	     xdg-launch.man \
	     xde-theme.man

xde-theme.man: xde-theme.pod
	pod2man --center="User Commands" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--name="XDE-THEME" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="1" \
		$< >$@


EXTRA_DIST = LICENSE README TODO ChangeLog xde-theme.pod
