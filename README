How to compile and install Ck8.0
--------------------------------

1. Type "./configure". This runs a configuration script made by GNU
   autoconf, which configures Ck for your system and creates a Makefile.
   The configure script allows you to customize the configuration to
   your local needs; for details how to do this, type "./configure --help"
   or refer to the autoconf documentation (not included here).
   The following special switches are supported by "configure":
	--enable-shared		If this switch is specified Ck will
				compile itself as a shared library if
				configure can figure out how to do this
				on this platform.
	--with-tcl		Specifies the directory containing the
				Tcl binaries and Tcl's platform-dependent
				configuration information. By default the
				Tcl distribution is assumed to be in
				"../../tcl8.0".

2. Type "make". This will create a library called "libck.a" or "libck8.0.so"
   and an interpreter application called "cwsh" that allows you to type
   Tcl commands interactively or execute scripts.

3. Type "make install" to install Ck's binaries, script files, and man
   pages in standard places. You'll need write permission on the install
   directories to do this. If you plan to install the libraries, executables,
   and script files whitout documentation, use "make install-binaries" and
   "make install-libraries".

4. Now you should be able to execute "cwsh". However, if you haven't installed
   Ck then you'll need to set the CK_LIBRARY environment variable to hold the
   full path name of the "library" subdirectory. If Ck has been built as
   shared library, you have to set the LD_LIBRARY_PATH to include the directory
   where "libck8.0.so" resides.


So far, Ck8.0 has been successfully tested on various Linux distributions,
on FreeBSD 3.3 with manually adapted Makefile, and on Windows NT 4.0 with
a modified PDCURSES library. The Ck8.0 source tree should be able to be
combined with Tcl7.4, 7.5, 7.6, and 8.0.
Older version of Ck (which use Tcl7.4 or Tcl7.5) are in use for several
years on HP-UX, AIX, and DEC Unix.


Christian Werner, December 1999
mailto:Christian.Werner@t-online.de
