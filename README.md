  [xdg-launch -- read me first file.  2021-12-12]: #

xdg-launch
===============

Package `xdg-launch-1.11` was released under GPLv3 license
2021-12-12.

This is a "C"-language program that can be used to launch XDG desktop
applications with full startup notification and window manager
assistance from the command line.  The command is able to launch desktop
applications, autostart entries and xsession entries.  It is useful when
generating applications root menus for light-weight window managers that
do not provide startup notification for applications launched using the
keyboard or root menu.

The source for `xdg-launch` is hosted on [GitHub][1].


Release
-------

This is the `xdg-launch-1.11` package, released 2021-12-12.
This release, and the latest version, can be obtained from [GitHub][1],
using a command such as:

    $> git clone https://github.com/bbidulock/xdg-launch.git

Please see the [RELEASE][3] and [NEWS][4] files for release notes and
history of user visible changes for the current version, and the
[ChangeLog][5] file for a more detailed history of implementation
changes.  The [TODO][6] file lists features not yet implemented and
other outstanding items.

Please see the [INSTALL][8] file for installation instructions.

When working from `git(1)`, please use this file.  An abbreviated
installation procedure that works for most applications appears below.

This release is published under GPLv3.  Please see the license in the
file [COPYING][10].


Quick Start
-----------

The quickest and easiest way to get `xdg-launch` up and
running is to run the following commands:

    $> git clone https://github.com/bbidulock/xdg-launch.git
    $> cd xdg-launch
    $> ./autogen.sh
    $> ./configure
    $> make
    $> make DESTDIR="$pkgdir" install

This will configure, compile and install `xdg-launch` the
quickest.  For those who like to spend the extra 15 seconds reading
`./configure --help`, some compile time options can be turned on and off
before the build.

For general information on GNU's `./configure`, see the file
[INSTALL][8].

Dependencies
------------

To build and install this package, the `libxrandr`, `libxinerama` and
`glib2` packages should be installed first.  To run the `dmenu_launch`
script in the package requires the `dmenu` package to be installed.

Running
-------

Read the manual page after installation:

    $> man xdg-launch


Features
--------

The following programs are included in `xdg-launch`:

 - [__`xdg-launch(1)`__][11] -- This is the primary program.

 - [__`dmenu_launch(1)`__][12] -- This is a little script that uses
   `xdg-launch(1)` and dmenu(1) to provide an application menu.

 - [__`xdg-autostart(1)`__][13] -- This is a little script that invokes
   xdg-launch(1) as a launcher for auto-start programs instead of
   applications.

 - [__`xdg-xsession(1)`__][14] -- This is a little script that invokes
   `xdg-launch(1)` as a launcher for X Sessions programs (window
   managers) instead of applications.

 - [__`xdg-session(1)`__][15] -- This is a little script that invokes
   `xdg-launch(1)` as a launcher for X Sessions programs (window
   managers) instead of applications a simple session and full
   auto-start procedure.

 - [__`xdg-entry(1)`__][16] -- This is a little script that invokes
   xdg-launch(1)` with the --info option and lists which information
   would be used for startup notification and launching of the resulting
   application.

 - [__`xdg-toolwait(1)`__][17] -- This is a little script that invokes
   `xdg-launch(1)` with the --toolwait option.

 - [__`xdg-which(1)`__][18], [__`xdg-whereis(1)`__][19] -- Two little programs what
   parallel `which(1)` and `whereis(1)` for desktop entry files instead
   of binaries.

 - [__`xdg-assist(1)`__][20] -- A program to monitor startup notification and
   provide notification as programs start up and performs startup
   notification completion of applications that do not complete.  It
   also assists window managers with supporting startup notification and
   other EWMH/NetWM features.

 - [__`xdg-list(1)`__][21] -- lists desktop entry files.

 - [__`xdg-types(1)`__][22] -- shows the content types provided by a desktop
   application.

Also provided (when `glib2` is available) are some little tools for the
XDG desktop:

 - [__`xdg-find(1)`__][23] -- finds desktop entry files based on a number of
   search criteria.

 - [__`xdg-prefs(1)`__][24] -- locates or sets preferred applications by
   category or mime type.


Issues
------

Report issues on GitHub [here][2].



[1]: https://github.com/bbidulock/xdg-launch
[2]: https://github.com/bbidulock/xdg-launch/issues
[3]: https://github.com/bbidulock/xdg-launch/blob/1.11/RELEASE
[4]: https://github.com/bbidulock/xdg-launch/blob/1.11/NEWS
[5]: https://github.com/bbidulock/xdg-launch/blob/1.11/ChangeLog
[6]: https://github.com/bbidulock/xdg-launch/blob/1.11/TODO
[7]: https://github.com/bbidulock/xdg-launch/blob/1.11/COMPLIANCE
[8]: https://github.com/bbidulock/xdg-launch/blob/1.11/INSTALL
[9]: https://github.com/bbidulock/xdg-launch/blob/1.11/LICENSE
[10]: https://github.com/bbidulock/xdg-launch/blob/1.11/COPYING
[11]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-launch.pod
[12]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/dmenu_launch.pod
[13]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-autostart.pod
[14]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-xsession.pod
[15]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-session.pod
[16]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-entry.pod
[17]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-toolwait.pod
[18]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-which.pod
[19]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-whereis.pod
[20]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-assist.pod
[21]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-list.pod
[22]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-types.pod
[23]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-find.pod
[24]: https://github.com/bbidulock/xdg-launch/blob/1.11/man/xdg-prefs.pod

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
