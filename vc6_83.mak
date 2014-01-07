##############################################################################
# Ck for Win32 using Microsoft Visual C++ 6.0
##############################################################################

#
# TCL_DIR must be set to the installation directory of Tcl8.3
#
TCL_DIR =		C:\progra~1\Tcl

#
# CURSES_INCLUDES must point to the directory where PDCURSES include files are
#
CURSES_INCLUDES =       -IE:\pdcurses

#
# CURSES_LIB must point to the PDCURSES link library
#
CURSES_LIB =            E:\pdcurses\win32\pdcurses.lib

#
# Installation directory of MS VC 6
#
MSVC =                  "C:\progra~1\microsoft visual studio\vc98"

#-----------------------------------------------------------------------------
# The information below should be usable as is.

CC =                    cl
LN =                    link
RC =			rc

LIBS = $(TCL_DIR)\lib\tcl83.lib libcmt.lib kernel32.lib user32.lib \
	$(CURSES_LIB)

DLLLIBS = $(TCL_DIR)\lib\tcl83.lib msvcrt.lib kernel32.lib user32.lib \
	$(CURSES_LIB)

CFLAGS = -Zi -Gs -GD -c -W3 -nologo -D_MT -DWIN32 -I$(MSVC)\include \
	 -I$(TCL_DIR)\include $(CURSES_INCLUDES)

LFLAGS =  /NODEFAULTLIB /RELEASE /NOLOGO /MACHINE:IX86 /SUBSYSTEM:WINDOWS \
	  /ENTRY:WinMainCRTStartup

DLLLFLAGS = /NODEFAULTLIB /RELEASE /NOLOGO /MACHINE:IX86 /SUBSYSTEM:WINDOWS \
	    /ENTRY:_DllMainCRTStartup@12 /DLL

WIDGOBJS = ckButton.obj ckEntry.obj ckFrame.obj ckListbox.obj \
	ckMenu.obj ckMenubutton.obj ckMessage.obj ckScrollbar.obj ckTree.obj

TEXTOBJS = ckText.obj ckTextBTree.obj ckTextDisp.obj ckTextIndex.obj \
	ckTextMark.obj ckTextTag.obj

OBJS =  ckBind.obj ckBorder.obj ckCmds.obj ckConfig.obj ckEvent.obj \
	ckFocus.obj \
	ckGeometry.obj ckGet.obj ckGrid.obj ckMain.obj ckOption.obj \
	ckPack.obj ckPlace.obj \
	ckPreserve.obj ckRecorder.obj ckUtil.obj ckWindow.obj tkEvent.obj \
	ckAppInit.obj $(WIDGOBJS) $(TEXTOBJS)

HDRS = default.h ks_names.h ck.h ckPort.h ckText.h

all: ck83.dll cwsh.exe

ck83.dll: $(OBJS)
	set LIB=$(MSVC)\lib
	$(LN) $(DLLLFLAGS) -out:$@ $(DLLLIBS) @<<
$(OBJS)
<<

cwsh.exe: winMain.obj cwsh.res ck83.dll
	set LIB=$(MSVC)\lib
	$(LN) $(LFLAGS) winMain.obj cwsh.res -out:$@ $(DLLLIBS) ck83.lib

clean:
	del *.obj
	del *.lib
	del *.exp
	del *.exe
	del *.res
	del *.pdb
	del ck83.dll

.c.obj:
	$(CC) -DBUILD_ck -D_DLL $(CFLAGS) $<

.rc.res:
	$(RC) -I$(MSVC)\include -I$(TCL_DIR)\include -fo $@ -r $<

winMain.obj: winMain.c
	$(CC) $(CFLAGS) -D_DLL winMain.c


