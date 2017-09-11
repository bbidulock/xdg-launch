
xdg_man1_files = \
	     xdg-launch.man \
	     xdg-assist.man \
	     dmenu_launch.man \
	     xdg-xsession.man \
	     xdg-autostart.man \
	     xdg-whereis.man \
	     xdg-which.man \
	     xdg-entry.man \
	     xdg-toolwait.man \
	     xdg-list.man

if HAVE_GIO_GLIB2_SUPPORT
xdg_man1_files += \
	     xdg-types.man \
	     xdg-prefs.man \
	     xdg-find.man
endif

nodist_man1_MANS = $(xdg_man1_files)
CLEANFILES = $(xdg_man1_files)

EXTRA_DIST = \
	     xdg-launch.pod \
	     xdg-assist.pod \
	     dmenu_launch.pod \
	     xdg-xsession.pod \
	     xdg-autostart.pod \
	     xdg-whereis.pod \
	     xdg-which.pod \
	     xdg-entry.pod \
	     xdg-toolwait.pod \
	     xdg-list.pod \
	     xdg-types.pod \
	     xdg-prefs.pod \
	     xdg-find.pod

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

