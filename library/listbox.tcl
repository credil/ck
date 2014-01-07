# listbox.tcl --
#
# This file defines the default bindings for Tk listbox widgets
# and provides procedures that help in implementing those bindings.
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#--------------------------------------------------------------------------
# ckPriv elements used in this file:
#
# listboxPrev -		The last element to be selected or deselected
#			during a selection operation.
# listboxSelection -	All of the items that were selected before the
#			current selection operation (such as a mouse
#			drag) started;  used to cancel an operation.
#--------------------------------------------------------------------------

#-------------------------------------------------------------------------
# The code below creates the default class bindings for listboxes.
#-------------------------------------------------------------------------

bind Listbox <Up> {
    ckListboxUpDown %W -1
}
bind Listbox <Down> {
    ckListboxUpDown %W 1
}
bind Listbox <Left> {
    %W xview scroll -1 units
}
bind Listbox <Right> {
    %W xview scroll 1 units
}
bind Listbox <Prior> {
    %W yview scroll -1 pages
    %W activate @0,0
}
bind Listbox <Next> {
    %W yview scroll 1 pages
    %W activate @0,0
}
bind Listbox <Home> {
    %W xview moveto 0
}
bind Listbox <End> {
    %W xview moveto 1
}
bind Listbox <space> {
    ckListboxBeginSelect %W [%W index active]
}
bind Listbox <Select> {
    ckListboxBeginSelect %W [%W index active]
}
bind Listbox <Escape> {
    ckListboxCancel %W
}
bind Listbox <Button-1> {
    focus %W
    ckListboxBeginSelect %W [%W index @0,%y]
}

# ckListboxBeginSelect --
#
# This procedure is typically invoked on space presses.  It begins
# the process of making a selection in the listbox.  Its exact behavior
# depends on the selection mode currently in effect for the listbox;
# see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc ckListboxBeginSelect {w el} {
    global ckPriv
    if {[$w cget -selectmode] == "multiple"} {
	if [$w selection includes $el] {
	    $w selection clear $el
	} else {
	    $w selection set $el
	}
    } else {
	$w activate $el
	$w selection clear 0 end
	$w selection set $el
	$w selection anchor $el
	set ckPriv(listboxSelection) {}
	set ckPriv(listboxPrev) $el
    }
}

# ckListboxUpDown --
#
# Moves the location cursor (active element) up or down by one element,
# and changes the selection if we're in browse or extended selection
# mode.
#
# Arguments:
# w -		The listbox widget.
# amount -	+1 to move down one item, -1 to move back one item.

proc ckListboxUpDown {w amount} {
    global ckPriv
    $w activate [expr [$w index active] + $amount]
    $w see active
    switch [$w cget -selectmode] {
	browse {
	    $w selection clear 0 end
	    $w selection set active
	}
	extended {
	    $w selection clear 0 end
	    $w selection set active
	    $w selection anchor active
	    set ckPriv(listboxPrev) [$w index active]
	    set ckPriv(listboxSelection) {}
	}
    }
}

# ckListboxCancel
#
# This procedure is invoked to cancel an extended selection in
# progress.  If there is an extended selection in progress, it
# restores all of the items between the active one and the anchor
# to their previous selection state.
#
# Arguments:
# w -		The listbox widget.

proc ckListboxCancel w {
    global ckPriv
    if {[$w cget -selectmode] != "extended"} {
	return
    }
    set first [$w index anchor]
    set last $ckPriv(listboxPrev)
    if {$first > $last} {
	set tmp $first
	set first $last
	set last $tmp
    }
    $w selection clear $first $last
    while {$first <= $last} {
	if {[lsearch $ckPriv(listboxSelection) $first] >= 0} {
	    $w selection set $first
	}
	incr first
    }
}
