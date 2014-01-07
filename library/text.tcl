# text.tcl --
#
# This file defines the default bindings for text widgets and provides
# procedures that help in implementing the bindings.
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for texts.
#-------------------------------------------------------------------------

bind Text <Left> {
    ckTextSetCursor %W [%W index {insert - 1c}]
}
bind Text <Right> {
    ckTextSetCursor %W [%W index {insert + 1c}]
}
bind Text <Up> {
    ckTextSetCursor %W [ckTextUpDownLine %W -1]
}
bind Text <Down> {
    ckTextSetCursor %W [ckTextUpDownLine %W 1]
}
bind Text <Prior> {
    ckTextSetCursor %W [ckTextScrollPages %W -1]
}
bind Text <Next> {
    ckTextSetCursor %W [ckTextScrollPages %W 1]
}
bind Text <Home> {
    ckTextSetCursor %W {insert linestart}
}
bind Text <End> {
    ckTextSetCursor %W {insert lineend}
}
bind Text <Tab> {
    ckTextInsert %W \t
    focus %W
    break
}
bind Text <Return> {
    ckTextInsert %W \n
}
bind Text <Linefeed> {
    ckTextInsert %W \n
}
bind Text <Delete> {
    if {[%W tag nextrange sel 1.0 end] != ""} {
	%W delete sel.first sel.last
    } else {
	%W delete insert
	%W see insert
    }
}
bind Text <ASCIIDelete> {
    if {[%W tag nextrange sel 1.0 end] != ""} {
	%W delete sel.first sel.last
    } else {
	%W delete insert
	%W see insert
    }
}
bind Text <BackSpace> {
    if {[%W tag nextrange sel 1.0 end] != ""} {
	%W delete sel.first sel.last
    } elseif [%W compare insert != 1.0] {
	%W delete insert-1c
	%W see insert
    }
}
bind Text <KeyPress> {
    ckTextInsert %W %A
}
bind Text <Button-1> {
    if [ckFocusOK %W] {
        ckTextSetCursor %W @%x,%y
        focus %W
    }
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for <Escape>.

bind Text <Control> {# nothing}

bind Text <Control-x> {focus [ck_focusNext %W]}

bind Text <Control-a> {
    ckTextSetCursor %W {insert linestart}
}
bind Text <Control-b> {
    ckTextSetCursor %W insert-1c
}
bind Text <Control-d> {
    %W delete insert
}
bind Text <Control-e> {
    ckTextSetCursor %W {insert lineend}
}
bind Text <Control-f> {
    ckTextSetCursor %W insert+1c
}
bind Text <Control-k> {
    if [%W compare insert == {insert lineend}] {
	%W delete insert
    } else {
	%W delete insert {insert lineend}
    }
}
bind Text <Control-n> {
    ckTextSetCursor %W [ckTextUpDownLine %W 1]
}
bind Text <Control-o> {
    %W insert insert \n
    %W mark set insert insert-1c
}
bind Text <Control-p> {
    ckTextSetCursor %W [ckTextUpDownLine %W -1]
}
bind Text <Control-v> {
    ckTextScrollPages %W 1
}
bind Text <Control-h> {
    if [%W compare insert != 1.0] {
	%W delete insert-1c
	%W see insert
    }
}
bind Text <FocusIn> {%W see insert}

set ckPriv(prevPos) {}

# ckTextSetCursor
# Move the insertion cursor to a given position in a text.  Also
# clears the selection, if there is one in the text, and makes sure
# that the insertion cursor is visible.  Also, don't let the insertion
# cursor appear on the dummy last line of the text.
#
# Arguments:
# w -           The text window.
# pos -         The desired new position for the cursor in the window.

proc ckTextSetCursor {w pos} {
    global ckPriv

    if [$w compare $pos == end] {
        set pos {end - 1 chars}
    }
    $w mark set insert $pos
    $w tag remove sel 1.0 end
    $w see insert
}

# ckTextInsert --
# Insert a string into a text at the point of the insertion cursor.
# If there is a selection in the text, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The text window in which to insert the string
# s -		The string to insert (usually just a single character)

proc ckTextInsert {w s} {
    if {([string length $s] == 0) || ([$w cget -state] == "disabled")} {
	return
    }
    catch {
	if {[$w compare sel.first <= insert]
		&& [$w compare sel.last >= insert]} {
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    $w see insert
}

# ckTextUpDownLine --
# Returns the index of the character one line above or below the
# insertion cursor.  There are two tricky things here.  First,
# we want to maintain the original column across repeated operations,
# even though some lines that will get passed through don't have
# enough characters to cover the original column.  Second, don't
# try to scroll past the beginning or end of the text.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# n -		The number of lines to move: -1 for up one line,
#		+1 for down one line.

proc ckTextUpDownLine {w n} {
    global ckPriv

    set i [$w index insert]
    scan $i "%d.%d" line char
    if {[string compare $ckPriv(prevPos) $i] != 0} {
	set ckPriv(char) $char
    }
    set new [$w index [expr $line + $n].$ckPriv(char)]
    if {[$w compare $new == end] || [$w compare $new == "insert linestart"]} {
	set new $i
    }
    set ckPriv(prevPos) $new
    return $new
}

# ckTextScrollPages --
# This is a utility procedure used in bindings for moving up and down
# pages and possibly extending the selection along the way.  It scrolls
# the view in the widget by the number of pages, and it returns the
# index of the character that is at the same position in the new view
# as the insertion cursor used to be in the old view.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# count -	Number of pages forward to scroll;  may be negative
#		to scroll backwards.

proc ckTextScrollPages {w count} {
    set bbox [$w bbox insert]
    $w yview scroll $count pages
    if {$bbox == ""} {
	return [$w index @[expr [winfo height $w]/2],0]
    }
    return [$w index @[lindex $bbox 0],[lindex $bbox 1]]
}
