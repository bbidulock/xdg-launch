
xdg_man1_files = \
	dmenu_launch.man \
	xdg-assist.man \
	xdg-autostart.man \
	xdg-entry.man \
	xdg-icon.man \
	xdg-launch.man \
	xdg-list.man \
	xdg-session.man \
	xdg-startup.man \
	xdg-toolwait.man \
	xdg-types.man \
	xdg-whereis.man \
	xdg-which.man \
	xdg-xsession.man

if HAVE_GIO_GLIB2_SUPPORT
xdg_man1_files += \
	xdg-find.man \
	xdg-prefs.man
endif

nodist_man1_MANS = $(xdg_man1_files)
CLEANFILES = $(xdg_man1_files)

EXTRA_DIST = \
	dmenu_launch.pod \
	xdg-assist.pod \
	xdg-autostart.pod \
	xdg-entry.pod \
	xdg-icon.pod \
	xdg-find.pod \
	xdg-launch.pod \
	xdg-list.pod \
	xdg-prefs.pod \
	xdg-session.pod \
	xdg-startup.pod \
	xdg-toolwait.pod \
	xdg-types.pod \
	xdg-whereis.pod \
	xdg-which.pod \
	xdg-xsession.pod

$(xdg_man1_files): %.man: %.pod
	$(AM_V_GEN)pod2man --center="User Commands" \
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

#$(xdg_man5_files): %.man: %.pod
#	$(AM_V_GEN)pod2man --center="Standards, Environments and Macros" \
#		--date="$(DATE)" \
#		--errors="none" \
#		--fixed="CW" \
#		--fixedbold="CB" \
#		--fixeditalic="CI" \
#		--fixedbolditalic="CX" \
#		--nourls \
#		--release="$(PACKAGE) $(VERSION)" \
#		--section="5" \
#		$< >$@

