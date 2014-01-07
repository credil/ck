# scrollbar.tcl --
#
# This file defines the default bindings for Tk scrollbar widgets.
# It also provides procedures that help in implementing the bindings.
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for scrollbars.
#-------------------------------------------------------------------------

bind Scrollbar <Up> {
    ckScrollByUnits %W v -1
}
bind Scrollbar <Down> {
    ckScrollByUnits %W v 1
}
bind Scrollbar <Left> {
    ckScrollByUnits %W h -1
}
bind Scrollbar <Right> {
    ckScrollByUnits %W h 1
}
bind Scrollbar <Prior> {
    ckScrollByPages %W hv -1
}
bind Scrollbar <Next> {
    ckScrollByPages %W hv 1
}
bind Scrollbar <Home> {
    ckScrollToPos %W 0
}
bind Scrollbar <End> {
    ckScrollToPos %W 1
}
bind Scrollbar <Button-1> {
    ckScrollByButton %W %x %y
}

bind Scrollbar <FocusIn> {%W activate}
bind Scrollbar <FocusOut> {%W deactivate}

# ckScrollByUnits --
# This procedure tells the scrollbar's associated widget to scroll up
# or down by a given number of units.  It notifies the associated widget
# in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# orient -	Which kinds of scrollbars this applies to:  "h" for
#		horizontal, "v" for vertical, "hv" for both.
# amount -	How many units to scroll:  typically 1 or -1.

proc ckScrollByUnits {w orient amount} {
    set cmd [$w cget -command]
    if {($cmd == "") || ([string first \
	    [string index [$w cget -orient] 0] $orient] < 0)} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd scroll $amount units
    } else {
	uplevel #0 $cmd [expr [lindex $info 2] + $amount]
    }
}

# ckScrollByPages --
# This procedure tells the scrollbar's associated widget to scroll up
# or down by a given number of screenfuls.  It notifies the associated
# widget in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# orient -	Which kinds of scrollbars this applies to:  "h" for
#		horizontal, "v" for vertical, "hv" for both.
# amount -	How many screens to scroll:  typically 1 or -1.

proc ckScrollByPages {w orient amount} {
    set cmd [$w cget -command]
    if {($cmd == "") || ([string first \
	    [string index [$w cget -orient] 0] $orient] < 0)} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd scroll $amount pages
    } else {
	uplevel #0 $cmd [expr [lindex $info 2] + $amount*([lindex $info 1] - 1)]
    }
}

# ckScrollToPos --
# This procedure tells the scrollbar's associated widget to scroll to
# a particular location, given by a fraction between 0 and 1.  It notifies
# the associated widget in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# pos -		A fraction between 0 and 1 indicating a desired position
#		in the document.

proc ckScrollToPos {w pos} {
    set cmd [$w cget -command]
    if {($cmd == "")} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd moveto $pos
    } else {
	uplevel #0 $cmd [expr round([lindex $info 0]*$pos)]
    }
}

# ckScrollByButton --
# This procedure is invoked for button presses on any element of the
# scrollbar.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates of button press.

proc ckScrollByButton {w x y} {
    set element [$w identify $x $y]
    if {$element == "arrow1"} {
        ckScrollByUnits $w hv -1
    } elseif {$element == "trough1"} {
        ckScrollByPages $w hv -1
    } elseif {$element == "trough2"} {
        ckScrollByPages $w hv 1
    } elseif {$element == "arrow2"} {
        ckScrollByUnits $w hv 1
    } else {
        return
    }
}

