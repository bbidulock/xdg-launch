
xdg_man_files = \
	     xdg-launch.man \
	     xdg-assist.man \
	     xdg-monitor.man \
	     dmenu_launch.man \
	     xdg-xsession.man \
	     xdg-autostart.man \
	     xdg-whereis.man \
	     xdg-which.man \
	     xdg-entry.man \
	     xdg-show.man \
	     xdg-toolwait.man

nodist_man1_MANS = $(xdg_man_files)
CLEANFILES = $(xdg_man_files)

EXTRA_DIST = \
	     xdg-launch.pod \
	     xdg-assist.pod \
	     xdg-monitor.pod \
	     dmenu_launch.pod \
	     xdg-xsession.pod \
	     xdg-autostart.pod \
	     xdg-whereis.pod \
	     xdg-which.pod \
	     xdg-entry.pod \
	     xdg-show.pod \
	     xdg-toolwait.pod

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

