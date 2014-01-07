/*
 * ckPort.h --
 *
 *	This file is included by all of the curses wish C files.
 *	It contains information that may be configuration-dependent,
 *	such as #includes for system include files and a few other things.
 *
 * Copyright (c) 1991-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _CKPORT
#define _CKPORT

#if defined(_WIN32) || defined(WIN32)
#   include <windows.h>
#else
#   define _XOPEN_SOURCE
#endif

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.  This macro may be used in some of the include
 * files below, which is why it is defined here.
 */

#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#if defined(_WIN32) || defined(WIN32)
#   include <limits.h>
#else
#   ifdef HAVE_LIMITS_H
#      include <limits.h>
#   else
#      include "compat/limits.h"
#   endif
#endif
#include <math.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <pwd.h>
#endif
#ifdef NO_STDLIB_H
#   include "compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#include <string.h>
#include <sys/types.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <sys/file.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif
#include <sys/stat.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <sys/time.h>
#endif
#ifndef _TCL
#   include <tcl.h>
#endif
#if !defined(_WIN32) && !defined(WIN32)
#   ifdef HAVE_UNISTD_H
#      include <unistd.h>
#   else
#      include "compat/unistd.h"
#   endif
#endif

#if (TCL_MAJOR_VERSION < 7)
#error Tcl major version must be 7 or greater
#endif

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.
 */

#if !defined(_WIN32) && !defined(WIN32)
extern int errno;
#endif

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)

/*
 * The following macro defines the type of the mask arguments to
 * select:
 */

#ifndef NO_FD_SET
#   define SELECT_MASK fd_set
#else
#   ifndef _AIX
	typedef long fd_mask;
#   endif
#   if defined(_IBMR2)
#	define SELECT_MASK void
#   else
#	define SELECT_MASK int
#   endif
#endif

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * The following macro defines the number of fd_masks in an fd_set:
 */

#ifndef FD_SETSIZE
#   ifdef OPEN_MAX
#	define FD_SETSIZE OPEN_MAX
#   else
#	define FD_SETSIZE 256
#   endif
#endif
#if !defined(howmany)
#   define howmany(x, y) (((x)+((y)-1))/(y))
#endif
#ifndef NFDBITS
#   define NFDBITS NBBY*sizeof(fd_mask)
#endif
#define MASK_SIZE howmany(FD_SETSIZE, NFDBITS)

/*
 * The following macro checks to see whether there is buffered
 * input data available for a stdio FILE.  This has to be done
 * in different ways on different systems.  TK_FILE_GPTR and
 * TK_FILE_COUNT are #defined by autoconf.
 */

#ifdef TK_FILE_COUNT
#   define TK_READ_DATA_PENDING(f) ((f)->TK_FILE_COUNT > 0)
#else
#   ifdef TK_FILE_GPTR
#       define TK_READ_DATA_PENDING(f) ((f)->_gptr < (f)->_egptr)
#   else
#       ifdef TK_FILE_READ_PTR
#	    define TK_READ_DATA_PENDING(f) ((f)->_IO_read_ptr != (f)->_IO_read_end)
#	else
	    /*
	     * Don't know what to do for this system; whoever installs
	     * Tk will have to write a function TkReadDataPending to do
	     * the job.
	     */
	    EXTERN int TkReadDataPending _ANSI_ARGS_((FILE *f));
#           define TK_READ_DATA_PENDING(f) TkReadDataPending(f)
#	endif
#   endif
#endif

/*
 * Substitute Tcl's own versions for several system calls.  The
 * Tcl versions retry automatically if interrupted by signals.
 */

#define open(a,b,c) TclOpen(a,b,c)
#define read(a,b,c) TclRead(a,b,c)
#define waitpid(a,b,c) TclWaitpid(a,b,c)
#define write(a,b,c) TclWrite(a,b,c)
EXTERN int	TclOpen _ANSI_ARGS_((char *path, int oflag, mode_t mode));
EXTERN int	TclRead _ANSI_ARGS_((int fd, VOID *buf,
		    unsigned int numBytes));
EXTERN int	TclWaitpid _ANSI_ARGS_((pid_t pid, int *statPtr, int options));
EXTERN int	TclWrite _ANSI_ARGS_((int fd, VOID *buf,
		    unsigned int numBytes));

/*
 * If this system has a BSDgettimeofday function (e.g. IRIX) use it
 * instead of gettimeofday; the gettimeofday function has a different
 * interface than the BSD one that this code expects.
 */

#ifdef HAVE_BSDGETTIMEOFDAY
#   define gettimeofday BSDgettimeofday
#endif
#ifdef GETTOD_NOT_DECLARED
EXTERN int		gettimeofday _ANSI_ARGS_((struct timeval *tp,
			    struct timezone *tzp));
#endif

#else

/*
 * Provide some defines to get some Tk functionality which was in
 * tkEvent.c prior to Tcl version 7.5.
 */

#define Tk_BackgroundError Tcl_BackgroundError
#define Tk_DoWhenIdle Tcl_DoWhenIdle
#define Tk_DoWhenIdle2 Tcl_DoWhenIdle
#define Tk_CancelIdleCall Tcl_CancelIdleCall
#define Tk_CreateTimerHandler Tcl_CreateTimerHandler
#define Tk_DeleteTimerHandler Tcl_DeleteTimerHandler
#define Tk_AfterCmd Tcl_AfterCmd
#define Tk_FileeventCmd Tcl_FileEventCmd
#define Tk_DoOneEvent Tcl_DoOneEvent

#endif

/*
 * Declarations for various library procedures that may not be declared
 * in any other header file.
 */

#ifndef panic
extern void		panic();
#endif

/*
 * Return type for signal(), this taken from TclX.
 */

#ifndef RETSIGTYPE
#   define RETSIGTYPE void
#endif

typedef RETSIGTYPE (*Ck_SignalProc) _ANSI_ARGS_((int));


#endif /* _CKPORT */
