# dialog.tcl --
#
# This file defines the procedure tk_dialog, which creates a dialog
# box containing a bitmap, a message, and one or more buttons.
#
# Copyright (c) 1992-1993 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# ck_dialog:
#
# This procedure displays a dialog box, waits for a button in the dialog
# to be invoked, then returns the index of the selected button.
#
# Arguments:
# w -		Window to use for dialog top-level.
# title -	Title to display in dialog's decorative frame.
# text -	Message to display in dialog.
# args -	One or more strings to display in buttons across the
#		bottom of the dialog box.

proc ck_dialog {w title text args} {
    global ckPriv
    if {[llength $args] <= 0} {
	return -1
    }
    catch {destroy $w}
    toplevel $w -class Dialog \
	-border {ulcorner hline urcorner vline lrcorner hline llcorner vline}
    place $w -relx 0.5 -rely 0.5 -anchor center
    if {[string length $title] > 0} {
	label $w.title -text $title
	pack $w.title -side top -fill x
	frame $w.sep0 -border hline -height 1
	pack $w.sep0 -side top -fill x
    }
    message $w.msg -text $text
    pack $w.msg -side top
    frame $w.sep1 -border hline -height 1
    pack $w.sep1 -side top -fill x
    frame $w.b
    pack $w.b -side top -fill x
    set i 0
    foreach but $args {
	button $w.b.b$i -text $but -command \
	    "set ckPriv(button) $i ; destroy $w"
	pack $w.b.b$i -side left -ipadx 1 -expand 1
	incr i
    }
    focus $w.b.b0
    tkwait window $w
    return $ckPriv(button)
}
