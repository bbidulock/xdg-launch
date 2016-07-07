
## xdg-launch

Package xdg-launch-1.0 was released under GPL license 2016-07-07.

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


### Release

This is the `xdg-launch-1.0` package, released 2016-07-07.  This release,
and the latest version, can be obtained from the GitHub repository at
https://github.com/bbidulock/xdg-launch, using a command such as:

```bash
git clone https://github.com/bbidulock/xdg-launch.git
```

Please see the [NEWS](NEWS) file for release notes and history of user visible
changes for the current version, and the [ChangeLog](ChangeLog) file for a more
detailed history of implementation changes.  The [TODO](TODO) file lists
features not yet implemented and other outstanding items.

Please see the [INSTALL](INSTALL) file for installation instructions.

When working from `git(1)', follow the instructions in this file.  An
abbreviated installation procedure that works for most applications
appears below.

This release is published under the GPL license that can be found in
the file [COPYING](COPYING).

### Quick Start:

The quickest and easiest way to get xdg-launch up and running is to run
the following commands:

```bash
git clone https://github.com/bbidulock/xdg-launch.git xdg-launch
cd xdg-launch
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc
make V=0
make DESTDIR="$pkgdir" install
```

This will configure, compile and install xdg-launch the quickest.  For
those who would like to spend the extra 15 seconds reading `./configure
--help`, some compile time options can be turned on and off before the
build.

For general information on GNU's `./configure`, see the file [INSTALL](INSTALL).

### Running xdg-launch

Read the manual page after installation:

    man xdg-launch.

The following programs are included in xdg-launch:

- xdg-launch(1) -- This is the primary program.

- dmenu_launch(1) -- This is a little script that uses xdg-launch(1) and
  dmenu(1) to provide an application menu.

- xdg-autostart(1) -- This is a little script that invokes xdg-launch(1)
  as a launcher for autostart programs instead of applications.

- xdg-xsession(1) -- This is a little script that invokes xdg-launch(1)
  as a launcher for X Sessions programs (window managers) instead of
  applications.

- xdg-entry(1) -- This is a little script that invokes xdg-launch(1)
  with the --info option and lists which information would be used for
  startup notification and launching of the resulting application.

- xdg-toolwait(1) -- This is a little script that invokes xdg-launch(1)
  with the --toolwait option.

- xdg-which(1), xdg-whereis(1) -- Two little programs what parallel
  which(1) and whereis(1) for desktop entry files instead of binaries.

- xdg-assist(1) -- A program to monitor startup notification and
  provide notification as programs start up and performs startup
  notification completion of applications that do not complete.
  It also assists window managers with supporting startup notification
  and other EWMH/NetWM features.

Also provided (when glib2 is available) are some little tools for
the XDG desktop:

- xdg-list(1) -- lists desktop entry files.

- xdg-types(1) -- shows the content types provided by a desktop
  application.

- xdg-find(1) -- finds desktop entry files based on a number of
  search criteria.

- xdg-which(1) -- determine which XDG desktop entries correspond to
  a given application id.

- xdg-prefs(1) -- locates or sets preferred applications by category
  or mime type.


### Issues

Report issues to https://github.com/bbidulock/xdg-launch/issues.

