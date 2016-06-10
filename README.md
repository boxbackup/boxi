# Boxi

A graphical user interface for Box Backup, the online encrypted backup
system by Ben Summers.

![Travis CI](https://travis-ci.org/boxbackup/boxi.svg?branch=master)

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
- openSUSE Leap 42.1 (Linux) Jan 2016
- SuSE Professional 9.1 (Linux)
- Ubuntu 5.04 (Linux, thanks to Scott Langley)
- Ubuntu 5.10 (Linux, development platform)
- Cygwin on Windows XP (development platform)

Feedback about and patches for compatibility with other systems are
most welcome.

## Download

Download the source code in the usual way and then download the dependent boxbackup source code as a submodule

```shell
git clone git@github.com:boxbackup/boxi.git
cd boxi
git submodule update --init
```

## Compiling (on Linux)

Boxi is distributed as source code, and binaries for Windows/Cygwin (eventually).

The following programs and libraries are required to compile Boxi:

- perl (version 5.8 or higher)
- patchutils (or at least patch)
- openssl (version 0.9.7a or higher, with development libraries)
- libncurses-devel (version 5, for boxbackup)
- gcc, g++ and libstdc++-devel
- wxWidgets (version 2.6.x or later, with development libraries) (> 3.0 needs code changes on separate branch)
- CppUnit (version 1.10.2 or later, with development libraries)
- m4 (version 1.4.1 or higher)
- autoconf (version 2.59 or higher)
- automake (version 1.6 or higher)
- libtool (version 1.5.6 or higher)
- gettext-devel
- pkg-config
- ccache (strictly optional)

To compile, run the following commands in the downloaded boxi source directory:

```shell
./autogen.sh
make
```

All in one Line:

```bash
git clone git@github.com:boxbackup/boxi.git && cd boxi && git submodule update --init && ./autogen.sh && make
```

## Building on Windows

To compile on Windows, you must satisfy the requirements of Box Backup.
You may find the this document helpful:

[https://www.boxbackup.org/wiki/CompileWithCygwinMinGW](https://www.boxbackup.org/wiki/CompileWithCygwinMinGW).

In addition, you will also need to compile and install CppUnit and wxMSW.


## Running

To run type the following command into the main download directory (i.e. boxi):

```shell
cd src          #needed for the internationalization
./boxi -c /etc/boxbackup/bbackupd.conf

Usage: boxi [-c] [-t <str>] [-l <str>] [-h] [<bbackupd-config-file>]
  -c                    ignored for compatibility with boxbackup command-line tools
  -t, --test=<str>      run the specified unit test, or ALL
  -l, --lang=<str>      load the specified language or translation
  -h, --help            displays this help text
```

Boxi reads the environment variable LANG and can pick up Spanish (es) and German (de) translations. (needs work)

If you supply the -c option and a bbackupd-config-file boxi will read your configuration file and populate the configuration for you. (The -c strictly is not required but you will be familiar with the option after having set up BoxBackup.) If you don't supply this click on the Wizard or Advanced Button to supply the values directly. You can also load the configuration file with the File > Open menu and write back changes.

-t Offers TestWizard, TestBackupConfig, TestBackup, TestConfig, TestRestore, TestCompare, all.

-l offers de_DE and es_ES capabilities

**Note:** If you want to run the tests it is essential that you start boxi from the ./src directory. The tests expect to find their fixtures in the ../test/config directory (which is relative to the current directory).

## Doxygen documentation
The tool Doxygen can be used to build a web page with the classes of the project. To build this documentation you need to have Doxygen installed. You can then:
```bash
cd boxi/docs
mkdir -p ../build/doxygen   # ensure that the target dir exists
doxygen Doxyfile            # writes the documentation to boxi/build/doxygen
```
You then need to point your browser to the location of the index file:
    file:///your/source/code/directory/boxi/build/doxygen/html/index.html


## Old Screenshots

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
