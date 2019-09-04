[xdg-launch -- release notes.  2019-09-03]: #

Maintenance Release 1.9
=======================

This is another stable release of the xdg-launch package that provides a
"C"-language program that can be used to launch XDG desktop applications
with full startup notification and window manager assistance from the
command line.  The command is able to launch desktop applications,
autostart entries and xsession entries.

This release is a maintenance release that updates the build system,
generates a better NEWS file, handles annotated tags better, and
converts the release archives to lzip complression.

In addition to build improvements, this release includes the ability to
select the terminal emulation program that is used to run desktop
applications that specify `Terminal=true` in their desktop entry files,
using environment variables.  Baring selection, it will search for
available terminal emulation programs from a long list of popular ones
and select the first one found.

Included in the release is an autoconf tarball for building the package
from source.  See the [NEWS](NEWS) and [TODO](TODO) file in the release
for more information.  Please report problems to the issues list on
[GitHub](https://github.com/bbidulock/xdg-launch/issues).

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
