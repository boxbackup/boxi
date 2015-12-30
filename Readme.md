# Boxi

A graphical user interface for Box Backup, the online encrypted backup
system by Ben Summers.

## Summary

Boxi is a graphical user interface to configure, manage, and administer
the Internet backup software called Box Backup by Ben Summers, available from
[https://www.boxbackup.org/](https://www.boxbackup.org/).

Box Backup is "an open source, completely automatic on-line backup system
for UNIX" with the following advantages:

- File Based

    All backed up data is stored on the server in files on a filesystem
    \-- no tape or archive devices are used

- Untrusted Servers

    The server is trusted only to make files available when they are
    required -- all data is encrypted on the client

- Automatic Backups

    A backup daemon runs on systems to be backed up, and copies
    encrypted data to the server when it notices changes

- Bandwidth Efficient

    Only changes within files are sent to the server, just like rsync

- Storage Efficient

    Old versions of files on the server are stored as changes from
    the current version

- File Versioning

    Behaves like tape -- old versions and deleted files are available

- Low Cost and Portable

    Designed to be easy and cheap to run a server. Portable implementation,
    and optional RAID implemented in userland for reliability without complex
    server setup or expensive hardware

Boxi is a graphical user interface to allow easy configuration and
administration of the Box Backup client, and access to files on the server.

Boxi is developed in C++ using the wxWidgets GUI toolkit, available from
[http://www.wxwidgets.org/](http://www.wxwidgets.org/). This means that it should be possible in
theory to compile Boxi on Windows, Macintosh and most UNIX systems,
although so far only the following systems have been tested:

- Fedora Core 2 (Linux, development platform)
- SuSE Professional 9.1 (Linux)
- Ubuntu 5.04 (Linux, thanks to Scott Langley)
- Ubuntu 5.10 (Linux, development platform)
- Cygwin on Windows XP (development platform)

Feedback about and patches for compatibility with other systems are
most welcome.

## Download

<!--- The latest stable version of Boxi is 0.1.0.

- Source code for all platforms (version 0.1.0)

    [http://prdownloads.sourceforge.net/boxi/boxi-0.1.0.tar.gz?download](http://prdownloads.sourceforge.net/boxi/boxi-0.1.0.tar.gz?download)

- Compiled executable for Windows/Cygwin (version 0.0.7)

    [http://prdownloads.sourceforge.net/boxi/boxi-0\_0\_7.zip?download](http://prdownloads.sourceforge.net/boxi/boxi-0_0_7.zip?download)

- Latest source code from Github
-->

Download the source code in the usual way and then downlowd the dependent boxbackup source code as a submodule

```shell
git clone git@github.com:boxbackup/boxi.git
cd boxi
git submodule init
git submodule update
```

## Compiling

Boxi is distributed as source code, and binaries for Windows/Cygwin.
Source code must be compiled before use.

The following programs and libraries are required to compile Boxi:

- perl (version 5.8 or higher)
- patchutils (or at least patch)
- openssl (version 0.9.7a or higher, with development libraries)
- libncurses-devel (version 5, for boxbackup)
- gcc, g++ and libstdc++-devel
- wxWidgets (version 2.6.x or later, with development libraries)
- CppUnit (version 1.10.2 or later, with development libraries)

If you are working from a CVS checkout, you will also need:

- m4 (version 1.4.1 or higher)
- autoconf (version 2.59 or higher)
- automake (version 1.6 or higher)
- libtool (version 1.5.6 or higher)
- gettext-devel
- pkg-config

To compile on Windows, you must satisfy the requirements of Box Backup.
You may find the this document helpful:

[http://www.boxbackup.org/svn/box/trunk/docs/api-notes/win32\_build\_on\_cygwin\_using\_mingw.txt](http://www.boxbackup.org/svn/box/trunk/docs/api-notes/win32_build_on_cygwin_using_mingw.txt).

In addition, you will also need to compile and install CppUnit and wxMSW.
Please see the document [cygwin-mingw-build.txt](https://metacpan.org/pod/cygwin-mingw-build.txt) for instructions.

<!--- If you downloaded a release version, just extract the contents of the
archive, and change into the directory it creates (e.g. boxi-0.1.0). -->

To compile, run the following commands in the downloaded boxi source directory:

```shell
./autogen.sh
./configure
make
```

Don't even think about installing it yet! There really isn't any point.
Run "src/boxi \[-c\] \[&lt;config-file>\]" to start the GUI.

The "-c" option is ignored if present, and provided to comfort those
more familiar with the Box Backup command-line tools.

<!--- ## Documentation
This Readme.md markdown page was generated from the docs/README.pod file
through the command:

    perl -MPod::Markdown -e 'Pod::Markdown->new->filter(@ARGV)' docs/README.pod > Readme.md

A text version of this document can be generated with the command:

    pod2text docs/README.pod > README.txt

Or an HTML version can be generated with the command:

    pod2html docs/README.pod -t "Boxi README" --noindex > README.html
-->

## Screenshots

![Backup Process](http://boxi.sourceforge.net/daemonctrl.png "Backup Process")

![Configuration](http://boxi.sourceforge.net/config.png "Configuration")

![Locations](http://boxi.sourceforge.net/locations.png "Locations")

![File List](http://boxi.sourceforge.net/filelist.png "File List")



## Changes

This software is currently experimental. I hope that you will try it and
find it useful, but it doesn't come with any warranty.
If it breaks, you get to keep both pieces.


### A note about versions

Henceforth, odd minor numbers (x.1.y, x.3.y, etc.) will be experimental,
and even numbers (x.2.y, x.4.y) slightly less experimental (maybe stable
one day).

- Version 0.1.0 (released 2005-05-26)

    [http://prdownloads.sourceforge.net/boxi/boxi-0.1.0.tar.gz?download](http://prdownloads.sourceforge.net/boxi/boxi-0.1.0.tar.gz?download)

    Commemorates the release of public CVS, and interest of other developers
    in joining in the project, with a minor version number increase.

    Fixed compilation on wxWindows 2.6.0 (at least on Cygwin, hopefully Linux
    too).

    Tweaked Box Backup's build infrastructure to require fewer rebuilds,
    while still satisfying dependencies (I hope :-)

- Version 0.0.7 (released 2005-05-21)

    [http://www.qwirx.com/boxi/boxi-0.0.7.tar.gz](http://www.qwirx.com/boxi/boxi-0.0.7.tar.gz)

    No need to start with the name of a configuration file any more!
    Built-in defaults for configuration values.

    Load configurations from a file while Boxi is running.

    Prompt to save configuration changes when exiting Boxi.

    Checks configuration for errors in real time while editing.

    Fewer pop-up warnings on Cygwin.

    First binary release for Windows (Cygwin).

- Version 0.0.6 (released 2005-05-08)

    [http://www.qwirx.com/boxi/boxi-0.0.6.tar.gz](http://www.qwirx.com/boxi/boxi-0.0.6.tar.gz) (released 2005-05-08)

    Server Files panel: hide deleted files and old versions of files on the
    server (see the new "View" menu). Disable Delete/Undelete buttons when
    a file is selected (need a protocol change to make this work for files).

    Locations panel: edit and remove backup locations.

    Local Files panel: view include/exclude status and reasons for local files.

    Backup Daemon panel: new GUI control panel for the bbackupd daemon. Start and
    stop the daemon, kill it, ask it to sync or reload (with Real Buttons(TM)).

    Box Backup: integrated build process with GNU autotools. Fixed "make clean"
    to do what I want (ensure a rebuild next time). Fixed "make" to work properly
    when remaking after a code change. Moved command socket handling to a
    separate class, to make it easier to access from other places in the code.
    Listen for command socket connections and commands while a backup is in
    progress, while sleeping on error, and while sleeping on store full.

    Server Connection: new class to encapsulate server interface.

    General: fixed Cygwin (more or less, perhaps less :-). Many thanks to
    Gary for his enthusiasm pushing me to make this happen.

    The code is stable, but not all new (or existing!) features work perfectly.
    The interface to control Local Files checking on server needs more work.
    Some things, like location matching, don't work on Cygwin but do work on
    Linux (and possibly other Unixes).

- Version 0.0.5

    [http://www.qwirx.com/boxi/boxi-0.0.5.tar.gz](http://www.qwirx.com/boxi/boxi-0.0.5.tar.gz) (released 2005-03-19)

    Separated configuration settings into Basic and Advanced, to be less
    intimidating for new users and to fit more easily into the available
    screen space.

    Allow navigating into directories by double-clicking files in the server
    file list (similar to Windows Explorer behaviour).

    Allow sorting the file list by filename, size or date, in forward or
    reverse order. Replaced the "Flags" column by "Last Modified" date, which
    is more useful.

    Redraw the GUI during directory restore operations, showing progress in the
    status bar (current file and number of files restored).

    Changed most error message dialogs to have more politically-correct titles
    (Boxi Error and Boxi Message).

    Added the "-c" command-line option, which is ignored, and renamed README.txt
    back to README, to stop Make from complaining.

    Many thanks to Richard Eigenmann for these suggestions.

- Version 0.0.4

    [http://www.qwirx.com/boxi/boxi-0.0.4.tar.gz](http://www.qwirx.com/boxi/boxi-0.0.4.tar.gz) (released 2005-03-14)

    Added the ability to restore files and directories from the server,
    and to delete and undelete directories.

    Fixed many crashes due to server disconnecting during operation,
    and internal buffer overflows in Box Backup's debugging code.

    Display file sizes (nicely formatted) in the server file list.

- Version 0.0.3

    [http://www.qwirx.com/boxi/boxi-0.0.3.tar.gz](http://www.qwirx.com/boxi/boxi-0.0.3.tar.gz) (released 2005-03-01)

    Split the code into multiple files (modules), which should make it
    easier to understand and faster to recompile after making changes.

    Fixed a bug that made it impossible to connect to the server
    and browse files stored there, since before version 0.0.2 was released.

    Missing essential configuration parameters are detected when trying
    to connect to the server, and a warning is given.

    Exceptions are handled much better, with Real Error Messages(TM).

    You can now save your changes to the configuration file by selecting
    the "Save" option from the "File" menu. This feature should be used
    with extreme caution! PLEASE back up your configuration file first!

## Bugs

_"There are no bugs, just undocumented features" (unknown)_

See the TODO file for a list of bugs to fix and features which are planned
for future releases. Feel free to contribute patches or suggestions for new
features :-)

If you find a problem that's not documented in the TODO file, please
submit a bug report on the Sourceforge bug tracker, at
[http://sourceforge.net/tracker/?group\_id=135105&atid=731482](http://sourceforge.net/tracker/?group_id=135105&atid=731482).

To request a new feature, please use the Sourceforge RFE tracker at
[http://sourceforge.net/tracker/?group\_id=135105&atid=731485](http://sourceforge.net/tracker/?group_id=135105&atid=731485).

## License

This software is copyright (C) Chris Wilson, 2004-05.
Licensed for public distribution under the GNU General Public License (GPL),
version 2 or later ([http://www.gnu.org/licenses/gpl.html](http://www.gnu.org/licenses/gpl.html)).

Contains a copy of Box Backup by Ben Summers, which is separately licensed
under a license very similar to the BSD license, which can be seen at
[https://github.com/boxbackup/boxbackup/blob/master/LICENSE-DUAL.txt](https://github.com/boxbackup/boxbackup/blob/master/LICENSE-DUAL.txt). This component is NOT
covered by the GPL.

The file boxbackup.patch contains a number of changes to Box Backup which
are useful for Boxi. That file is under the same license as Box Backup,
i.e. a BSD-like license, and copyright assigned to Ben Summers.

Portions of Boxi are copied, based on or inspired by Box Backup
itself, and it is not possible to compile Boxi without linking to
the Box Backup code. Therefore, the Boxi code contains the attribution
required by the Box Backup license, that it contains code by Ben Summers.
This attribution **must not** be removed from the code!

Previously hosted by SourceForge:

<div>
    <p><A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=135105&amp;type=5" width="210" height="62" border="0" alt="SourceForge.net Logo" /></A></p>
</div>

<div>
    <p><a href="http://sourceforge.net/donate/index.php?group_id=135105"><img src="http://images.sourceforge.net/images/project-support.jpg" width="88" height="32" border="0" alt="Support This Project" /></a></p>
</div>
