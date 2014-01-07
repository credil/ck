# entry.tcl --
#
# This file defines the default bindings for entry widgets and provides
# procedures that help in implementing those bindings.
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for entries.
#-------------------------------------------------------------------------

bind Entry <Left> {
    ckEntrySetCursor %W [expr [%W index insert] - 1]
}
bind Entry <Right> {
    ckEntrySetCursor %W [expr [%W index insert] + 1]
}
bind Entry <Home> {
    ckEntrySetCursor %W 0
}
bind Entry <End> {
    ckEntrySetCursor %W end
}
bind Entry <Delete> {
    if [%W selection present] {
	%W delete sel.first sel.last
    } else {
	%W delete insert
    }
}
bind Entry <ASCIIDelete> {
    if [%W selection present] {
	%W delete sel.first sel.last
    } else {
	%W delete insert
    }
}
bind Entry <BackSpace> {
    ckEntryBackspace %W
}
bind Entry <Select> {
    %W selection from insert
}
bind Entry <KeyPress> {
    ckEntryInsert %W %A
}
bind Entry <Control> {# nothing}
bind Entry <Escape> {# nothing}
bind Entry <Return> {# nothing}
bind Entry <Linefeed> {# nothing}
bind Entry <Tab> {# nothing}
bind Entry <BackTab> {# nothing}

bind Entry <Button-1> {
    if [ckFocusOK %W] {%W icursor @%x ; focus %W}
}

# Additional emacs-like bindings:

bind Entry <Control-a> {
    ckEntrySetCursor %W 0
}
bind Entry <Control-b> {
    ckEntrySetCursor %W [expr [%W index insert] - 1]
}
bind Entry <Control-d> {
    %W delete insert
}
bind Entry <Control-e> {
    ckEntrySetCursor %W end
}
bind Entry <Control-f> {
    ckEntrySetCursor %W [expr [%W index insert] + 1]
}
bind Entry <Control-h> {
    ckEntryBackspace %W
}
bind Entry <Control-k> {
    %W delete insert end
}
bind Entry <Control-t> {
    ckEntryTranspose %W
}

# ckEntryKeySelect --
# This procedure is invoked when stroking out selections using the
# keyboard.  It moves the cursor to a new position, then extends
# the selection to that position.
#
# Arguments:
# w -		The entry window.
# new -		A new position for the insertion cursor (the cursor hasn't
#		actually been moved to this position yet).

proc ckEntryKeySelect {w new} {
    if ![$w selection present] {
	$w selection from insert
	$w selection to $new
    } else {
	$w selection adjust $new
    }
    $w icursor $new
}

# ckEntryInsert --
# Insert a string into an entry at the point of the insertion cursor.
# If there is a selection in the entry, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The entry window in which to insert the string
# s -		The string to insert (usually just a single character)

proc ckEntryInsert {w s} {
    if {$s == ""} return
    catch {
	set insert [$w index insert]
	if {([$w index sel.first] <= $insert)
		&& ([$w index sel.last] >= $insert)} {
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    ckEntrySeeInsert $w
}

# ckEntryBackspace --
# Backspace over the character just before the insertion cursor.
# If backspacing would move the cursor off the left edge of the
# window, reposition the cursor at about the middle of the window.
#
# Arguments:
# w -		The entry window in which to backspace.

proc ckEntryBackspace w {
    if [$w selection present] {
	$w delete sel.first sel.last
    } else {
	set x [expr {[$w index insert] - 1}]
	if {$x >= 0} {$w delete $x}
	if {[$w index @0] >= [$w index insert]} {
	    set range [$w xview]
	    set left [lindex $range 0]
	    set right [lindex $range 1]
	    $w xview moveto [expr $left - ($right - $left)/2.0]
	}
    }
}

# ckEntrySeeInsert --
# Make sure that the insertion cursor is visible in the entry window.
# If not, adjust the view so that it is.
#
# Arguments:
# w -		The entry window.

proc ckEntrySeeInsert w {
    set c [$w index insert]
    set left [$w index @0]
    if {$left > $c} {
	$w xview $c
	return
    }
    set x [winfo width $w]
    while {([$w index @$x] <= $c) && ($left < $c)} {
	incr left
	$w xview $left
    }
}

# ckEntrySetCursor -
# Move the insertion cursor to a given position in an entry.  Also
# clears the selection, if there is one in the entry, and makes sure
# that the insertion cursor is visible.
#
# Arguments:
# w -		The entry window.
# pos -		The desired new position for the cursor in the window.

proc ckEntrySetCursor {w pos} {
    $w icursor $pos
    $w selection clear
    ckEntrySeeInsert $w
}

# ckEntryTranspose -
# This procedure implements the "transpose" function for entry widgets.
# It tranposes the characters on either side of the insertion cursor,
# unless the cursor is at the end of the line.  In this case it
# transposes the two characters to the left of the cursor.  In either
# case, the cursor ends up to the right of the transposed characters.
#
# Arguments:
# w -		The entry window.

proc ckEntryTranspose w {
    set i [$w index insert]
    if {$i < [$w index end]} {
	incr i
    }
    set first [expr $i-2]
    if {$first < 0} {
	return
    }
    set new [string index [$w get] [expr $i-1]][string index [$w get] $first]
    $w delete $first $i
    $w insert insert $new
    ckEntrySeeInsert $w
}
