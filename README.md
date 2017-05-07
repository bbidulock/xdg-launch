[xdg-launch -- read me first file.  @DATE]: #

xdg-launch
===============

Package `xdg-launch-1.4` was released under GPLv3 license 2017-05-07.

This is a "C"-language program that can be used to launch XDG desktop
applications with full startup notification and window manager
assistance from the command line.  The command is able to launch
desktop applications, autostart entries and xsession entries.  It is
useful when generating applications root menus for light-weight window
managers that do not provide startup notification for applications
launched using the keyboard or root menu.  The source is hosted on
[GitHub](https://github.com/bbidulock/xdg-launch).  What it includes is
just the xdg-launch program and manual page, as well as a handfull of
wrapper scripts.


Release
-------

This is the `xdg-launch-1.4` package, released 2017-05-07.  This
release, and the latest version, can be obtained from the [GitHub
repository][1], using a command such as:

    $> git clone https://github.com/bbidulock/xdg-launch.git

Please see the [NEWS][2] file for release notes and history of user
visible changes for the current version, and the [ChangeLog][3]
file for a more detailed history of implementation changes.  The
[TODO][4] file lists features not yet implemented and other
outstanding items.

Please see the [INSTALL][5] file for installation instructions.

When working from `git(1)`, please use this file.  An abbreviated
installation procedure that works for most applications appears below.

This release is published under GPLv3.  Please see the license in
the file [COPYING][6].


Quick Start
-----------

The quickest and easiest way to get `xdg-launch` up and running
is to run the following commands:

    $> git clone https://github.com/bbidulock/xdg-launch.git
    $> cd xdg-launch
    $> ./autogen.sh
    $> ./configure --prefix=/usr --sysconfdir=/etc
    $> make V=0
    $> make DESTDIR="$pkgdir" install

This will configure, compile and install `xdg-launch` the quickest.
For those who would like to spend the extra 15 seconds reading
the output of `./configure --help`, some compile options can be
turned on and off before the build.

For general information on GNU's `./configure`, see the file
[INSTALL][5].


Running xdg-launch
-----------------

Read the manual page after installation:

    man xdg-launch

The following programs are included in xdg-launch:

 - __`xdg-launch(1)`__ -- This is the primary program.

 - __`dmenu_launch(1)`__ -- This is a little script that uses
   `xdg-launch(1)` and dmenu(1) to provide an application menu.

 - __`xdg-autostart(1)`__ -- This is a little script that invokes
   xdg-launch(1) as a launcher for autostart programs instead of
   applications.

 - __`xdg-xsession(1)`__ -- This is a little script that invokes
   `xdg-launch(1)` as a launcher for X Sessions programs (window
   managers) instead of applications.

 - __`xdg-entry(1)`__ -- This is a little script that invokes
   xdg-launch(1)` with the --info option and lists which information
   would be used for startup notification and launching of the resulting
   application.

 - __`xdg-toolwait(1)`__ -- This is a little script that invokes
   `xdg-launch(1)` with the --toolwait option.

 - __`xdg-which(1)`__, __`xdg-whereis(1)`__ -- Two little programs what
   parallel `which(1)` and `whereis(1)` for desktop entry files instead
   of binaries.

 - __`xdg-assist(1)`__ -- A program to monitor startup notification and
   provide notification as programs start up and performs startup
   notification completion of applications that do not complete.  It
   also assists window managers with supporting startup notification and
   other EWMH/NetWM features.

Also provided (when `glib2` is available) are some little tools for the
XDG desktop:

 - __`xdg-list(1)`__ -- lists desktop entry files.

 - __`xdg-types(1)`__ -- shows the content types provided by a desktop
   application.

 - __`xdg-find(1)`__ -- finds desktop entry files based on a number of
   search criteria.

 - __`xdg-which(1)`__ -- determine which XDG desktop entries correspond
   to a given application id.

 - __`xdg-prefs(1)`__ -- locates or sets preferred applications by
   category or mime type.


Issues
------

Report problems at GitHub [here][7].



[1]: https://github.com/bbidulock/xdg-launch
[2]: NEWS
[3]: ChangeLog
[4]: TODO
[5]: INSTALL
[6]: COPYING
[7]: https://github.com/bbidulock/xdg-launch/issues

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
