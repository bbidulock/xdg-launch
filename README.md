
## xdg-launch

Package xdg-launch-0.8.23 was released under GPL license 2016-04-11.

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

This is the `xdg-launch-0.8.23` package, released 2016-04-11.  This release,
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

When working from `git(1)', please see the [README-git](README-git) file.  An
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

Read the manual pages after installation:

    man xdg-launch

### Issues

Report issues to https://github.com/bbidulock/xdg-launch/issues.

