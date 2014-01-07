# button.tcl --
#
# This file defines the default bindings for Ck label, button,
# checkbutton, and radiobutton widgets and provides procedures
# that help in implementing those bindings.
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

set ckPriv(buttonWindow) ""

#-------------------------------------------------------------------------
# The code below creates the default class bindings for buttons.
#-------------------------------------------------------------------------

bind Button <FocusIn> {ckButtonFocus %W 1}
bind Button <FocusOut> {ckButtonFocus %W 0}
bind Button <space> {ckButtonInvoke %W}
bind Button <Return> {ckButtonInvoke %W}
bind Button <Linefeed> {ckButtonInvoke %W}
bind Button <Button-1> {ckButtonInvoke %W}

bind Checkbutton <FocusIn> {ckButtonFocus %W 1}
bind Checkbutton <FocusOut> {ckButtonFocus %W 0}
bind Checkbutton <space> {ckButtonInvoke %W}
bind Checkbutton <Return> {ckButtonInvoke %W}
bind Checkbutton <Linefeed> {ckButtonInvoke %W}
bind Checkbutton <Button-1> {ckButtonInvoke %W}

bind Radiobutton <FocusIn> {ckButtonFocus %W 1}
bind Radiobutton <FocusOut> {ckButtonFocus %W 0}
bind Radiobutton <space> {ckButtonInvoke %W}
bind Radiobutton <Return> {ckButtonInvoke %W}
bind Radiobutton <Linefeed> {ckButtonInvoke %W}
bind Radiobutton <Button-1> {ckButtonInvoke %W}

# ckButtonFocus --
# The procedure below is called when a button is invoked through
# the keyboard.
#
# Arguments:
# w -		The name of the widget.

proc ckButtonFocus {w flag} {
    global ckPriv
    if {[$w cget -state] == "disabled"} return
    if {$flag} {
        set ckPriv(buttonWindow) $w
        set ckPriv(buttonState) [$w cget -state]
        $w configure -state active
        return
    }
    if {$w == $ckPriv(buttonWindow)} {
        set ckPriv(buttonWindow) ""
        $w configure -state $ckPriv(buttonState)
        set ckPriv(buttonState) ""
    }
}

# ckButtonInvoke --
# The procedure below is called when a button is invoked through
# the keyboard.
#
# Arguments:
# w -		The name of the widget.

proc ckButtonInvoke w {
    if {[$w cget -state] != "disabled"} {
	uplevel #0 [list $w invoke]
    }
}
