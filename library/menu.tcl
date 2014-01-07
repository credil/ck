# menu.tcl --
#
# This file defines the default bindings for Tk menus and menubuttons.
# It also implements keyboard traversal of menus and implements a few
# other utility procedures related to menus.
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# Elements of ckPriv that are used in this file:
#
# focus -		Saves the focus during a menu selection operation.
#			Focus gets restored here when the menu is unposted.
# postedMb -		Name of the menubutton whose menu is currently
#			posted, or an empty string if nothing is posted
# popup -		If a menu has been popped up via ck_popup, this
#			gives the name of the menu. Otherwise this value
#			is empty.
#-------------------------------------------------------------------------

set ckPriv(postedMb) ""
set ckPriv(popup) ""

#-------------------------------------------------------------------------
# The code below creates the default class bindings for menus
# and menubuttons.
#-------------------------------------------------------------------------

bind Menubutton <FocusIn> {}
bind Menubutton <space> {
    if {[ckMbPost %W]} {
	ckMenuFirstEntry [%W cget -menu]
    }
}
bind Menubutton <Return> {
    if {[ckMbPost %W]} {
	ckMenuFirstEntry [%W cget -menu]
    }
}
bind Menubutton <Linefeed> {
    if {[ckMbPost %W]} {
	ckMenuFirstEntry [%W cget -menu]
    }
}
bind Menubutton <Button-1> {
    if {[ckMbPost %W]} {
	ckMenuFirstEntry [%W cget -menu]
    }
}

bind Menu <space> {
    ckMenuInvoke %W
}
bind Menu <Return> {
    ckMenuInvoke %W
}
bind Menu <Linefeed> {
    ckMenuInvoke %W
}
bind Menu <Escape> {
    ckMenuEscape %W ; break
}
bind Menu <Left> {
    ckMenuLeftRight %W left
}
bind Menu <Right> {
    ckMenuLeftRight %W right
}
bind Menu <Up> {
    ckMenuNextEntry %W -1
}
bind Menu <Down> {
    ckMenuNextEntry %W +1
}
bind Menu <KeyPress> {
    ckTraverseWithinMenu %W %A
}
bind Menu <Button-1> {
    %W activate @%y
    ckMenuInvoke %W
}

bind all <F10> {
    ckFirstMenu %W
}

# ckMbPost --
# Given a menubutton, this procedure does all the work of posting
# its associated menu and unposting any other menu that is currently
# posted.
#
# Arguments:
# w -			The name of the menubutton widget whose menu
#			is to be posted.
# x, y -		Root coordinates of cursor, used for positioning
#			option menus.  If not specified, then the center
#			of the menubutton is used for an option menu.

proc ckMbPost {w {x {}} {y {}}} {
    global ckPriv
    if {([$w cget -state] == "disabled") || ($w == $ckPriv(postedMb))} {
	return 0
    }
    set menu [$w cget -menu]
    if {$menu == ""} {
	return 0
    }
    if ![string match $w.* $menu] {
	error "can't post $menu:  it isn't a descendant of $w"
    }
    set cur $ckPriv(postedMb)
    if {$cur != ""} {
	ckMenuUnpost {}
    }
    set ckPriv(postedMb) $w
    set ckPriv(focus) [focus]
    $menu activate none

    # If this looks like an option menubutton then post the menu so
    # that the current entry is on top of the mouse.  Otherwise post
    # the menu just below the menubutton, as for a pull-down.

    if {([$w cget -indicatoron] == 1) && ([$w cget -textvariable] != "")} {
	if {$y == ""} {
	    set x [expr [winfo rootx $w] + [winfo width $w]/2]
	    set y [expr [winfo rooty $w] + [winfo height $w]/2]
	}
	ckPostOverPoint $menu $x $y [ckMenuFindName $menu [$w cget -text]]
    } else {
	$menu post [winfo rootx $w] [expr [winfo rooty $w]+[winfo height $w]]
    }
    focus $menu
    $w configure -state active
    return 1
}

# ckMenuUnpost --
# This procedure unposts a given menu, plus all of its ancestors up
# to (and including) a menubutton, if any. It also restores various
# values to what they were before the menu was posted, and releases
# a grab if there's a menubutton involved. Special notes:
# Be sure to enclose various groups of commands in "catch" so that
# the procedure will complete even if the menubutton or the menu
# has been deleted.
#
# Arguments:
# menu -		Name of a menu to unpost.  Ignored if there
#			is a posted menubutton.

proc ckMenuUnpost menu {
    global ckPriv
    set mb $ckPriv(postedMb)
    catch {
	if {$mb != ""} {
            $mb configure -state normal
	    catch {focus $ckPriv(focus)}
	    set ckPriv(focus) ""
	    set menu [$mb cget -menu]
	    $menu unpost
	    set ckPriv(postedMb) {}
	} elseif {[string length $ckPriv(popup)]} {
	    catch {focus $ckPriv(focus)}
	    set ckPriv(focus) ""
	    $ckPriv(popup) unpost
	    set ckPriv(popup) ""
	}
    }
}

# ckMenuInvoke --
# This procedure is invoked when button 1 is released over a menu.
# It invokes the appropriate menu action and unposts the menu if
# it came from a menubutton.
#
# Arguments:
# w -			Name of the menu widget.

proc ckMenuInvoke w {
    if {[$w type active] == "cascade"} {
	$w postcascade active
	set menu [$w entrycget active -menu]
	ckMenuFirstEntry $menu
    } else {
	ckMenuUnpost $w
	uplevel #0 [list $w invoke active]
    }
}

# ckMenuEscape --
# This procedure is invoked for the Cancel (or Escape) key.  It unposts
# the given menu and, if it is the top-level menu for a menu button,
# unposts the menu button as well.
#
# Arguments:
# menu -		Name of the menu window.

proc ckMenuEscape menu {
    if {[winfo class [winfo parent $menu]] != "Menu"} {
	ckMenuUnpost $menu
    } else {
	ckMenuLeftRight $menu -1
    }
}

# ckMenuLeftRight --
# This procedure is invoked to handle "left" and "right" traversal
# motions in menus.  It traverses to the next menu in a menu bar,
# or into or out of a cascaded menu.
#
# Arguments:
# menu -		The menu that received the keyboard
#			event.
# direction -		Direction in which to move: "left" or "right"

proc ckMenuLeftRight {menu direction} {
    global ckPriv

    # First handle traversals into and out of cascaded menus.

    if {$direction == "right"} {
	set count 1
	if {[$menu type active] == "cascade"} {
	    $menu postcascade active
	    set m2 [$menu entrycget active -menu]
	    if {$m2 != ""} {
		ckMenuFirstEntry $m2
	    }
	    return
	}
    } else {
	set count -1
	set m2 [winfo parent $menu]
	if {[winfo class $m2] == "Menu"} {
	    focus $m2
            $m2 postcascade none
	    return
	}
    }

    # Can't traverse into or out of a cascaded menu.  Go to the next
    # or previous menubutton, if that makes sense.

    set w $ckPriv(postedMb)
    if {$w == ""} {
	return
    }
    set buttons [winfo children [winfo parent $w]]
    set length [llength $buttons]
    set i [expr [lsearch -exact $buttons $w] + $count]
    while 1 {
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	set mb [lindex $buttons $i]
	if {([winfo class $mb] == "Menubutton")
		&& ([$mb cget -state] != "disabled")
		&& ([$mb cget -menu] != "")
		&& ([[$mb cget -menu] index last] != "none")} {
	    break
	}
	if {$mb == $w} {
	    return
	}
	incr i $count
    }
    if {[ckMbPost $mb]} {
	ckMenuFirstEntry [$mb cget -menu]
    }
}

# ckMenuNextEntry --
# Activate the next higher or lower entry in the posted menu,
# wrapping around at the ends.  Disabled entries are skipped.
#
# Arguments:
# menu -			Menu window that received the keystroke.
# count -			1 means go to the next lower entry,
#				-1 means go to the next higher entry.

proc ckMenuNextEntry {menu count} {
    global ckPriv
    if {[$menu index last] == "none"} {
	return
    }
    set length [expr [$menu index last]+1]
    set active [$menu index active]
    if {$active == "none"} {
	set i 0
    } else {
	set i [expr $active + $count]
    }
    while 1 {
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	if {[catch {$menu entrycget $i -state} state] == 0} {
	    if {$state != "disabled"} {
		break
	    }
	}
	if {$i == $active} {
	    return
	}
	incr i $count
    }
    $menu activate $i
}

# ckMenuFind --
# This procedure searches the entire window hierarchy under w for
# a menubutton that isn't disabled and whose underlined character
# is "char".  It returns the name of that window, if found, or an
# empty string if no matching window was found.  If "char" is an
# empty string then the procedure returns the name of the first
# menubutton found that isn't disabled.
#
# Arguments:
# w -				Name of window where key was typed.
# char -			Underlined character to search for;
#				may be either upper or lower case, and
#				will match either upper or lower case.

proc ckMenuFind {w char} {
    global ckPriv
    set char [string tolower $char]

    foreach child [winfo child $w] {
	switch [winfo class $child] {
	    Menubutton {
		set char2 [string index [$child cget -text] \
			[$child cget -underline]]
		if {([string compare $char [string tolower $char2]] == 0)
			|| ($char == "")} {
		    if {[$child cget -state] != "disabled"} {
			return $child
		    }
		}
	    }
	    Frame {
		set match [ckMenuFind $child $char]
		if {$match != ""} {
		    return $match
		}
	    }
	}
    }
    return {}
}

# ckFirstMenu --
# This procedure traverses to the first menubutton in the toplevel
# for a given window, and posts that menubutton's menu.
#
# Arguments:
# w -				Name of a window.  Selects which toplevel
#				to search for menubuttons.

proc ckFirstMenu w {
    set w [ckMenuFind [winfo toplevel $w] ""]
    if {$w != ""} {
	if {[ckMbPost $w]} {
	    ckMenuFirstEntry [$w cget -menu]
	}
    }
}

# ckTraverseWithinMenu
# This procedure implements keyboard traversal within a menu.  It
# searches for an entry in the menu that has "char" underlined.  If
# such an entry is found, it is invoked and the menu is unposted.
#
# Arguments:
# w -				The name of the menu widget.
# char -			The character to look for;  case is
#				ignored.  If the string is empty then
#				nothing happens.

proc ckTraverseWithinMenu {w char} {
    if {$char == ""} {
	return
    }
    set char [string tolower $char]
    set last [$w index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if [catch {set char2 [string index \
		[$w entrycget $i -label] \
		[$w entrycget $i -underline]]}] {
	    continue
	}
	if {[string compare $char [string tolower $char2]] == 0} {
	    if {[$w type $i] == "cascade"} {
		$w postcascade $i
		$w activate $i
		set m2 [$w entrycget $i -menu]
		if {$m2 != ""} {
		    tkMenuFirstEntry $m2
		}
	    } else {
		ckMenuUnpost $w
		uplevel #0 [list $w invoke $i]
	    }
	    return
	}
    }
}

# ckMenuFirstEntry --
# Given a menu, this procedure finds the first entry that isn't
# disabled or a tear-off or separator, and activates that entry.
# However, if there is already an active entry in the menu (e.g.,
# because of a previous call to tkPostOverPoint) then the active
# entry isn't changed.  This procedure also sets the input focus
# to the menu.
#
# Arguments:
# menu -		Name of the menu window (possibly empty).

proc ckMenuFirstEntry menu {
    if {$menu == ""} {
	return
    }
    focus $menu
    if {[$menu index active] != "none"} {
	return
    }
    set last [$menu index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if {([catch {set state [$menu entrycget $i -state]}] == 0)
		&& ($state != "disabled")} {
	    $menu activate $i
	    return
	}
    }
}

# ckMenuFindName --
# Given a menu and a text string, return the index of the menu entry
# that displays the string as its label.  If there is no such entry,
# return an empty string.  This procedure is tricky because some names
# like "active" have a special meaning in menu commands, so we can't
# always use the "index" widget command.
#
# Arguments:
# menu -		Name of the menu widget.
# s -			String to look for.

proc ckMenuFindName {menu s} {
    set i ""
    if {![regexp {^active$|^last$|^none$|^[0-9]|^@} $s]} {
	catch {set i [$menu index $s]}
	return $i
    }
    set last [$menu index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if ![catch {$menu entrycget $i -label} label] {
	    if {$label == $s} {
		return $i
	    }
	}
    }
    return ""
}

# ckPostOverPoint --
# This procedure posts a given menu such that a given entry in the
# menu is centered over a given point in the root window.  It also
# activates the given entry.
#
# Arguments:
# menu -		Menu to post.
# x, y -		Root coordinates of point.
# entry -		Index of entry within menu to center over (x,y).
#			If omitted or specified as {}, then the menu's
#			upper-left corner goes at (x,y).

proc ckPostOverPoint {menu x y {entry {}}}  {
    if {$entry != {}} {
	if {$entry == [$menu index last]} {
	    incr y [expr -([$menu yposition $entry] \
		    + [winfo reqheight $menu])/2]
	} else {
	    incr y [expr -([$menu yposition $entry] \
		    + [$menu yposition [expr $entry+1]])/2]
	}
	incr x [expr -[winfo reqwidth $menu]/2]
    }
    $menu post $x $y
    if {($entry != {}) && ([$menu entrycget $entry -state] != "disabled")} {
	$menu activate $entry
    }
}

# ck_popup --
# This procedure pops up a menu and sets things up for traversing
# the menu and its submenus.
#
# Arguments:
# menu -		Name of the menu to be popped up.
# x, y -		Root coordinates at which to pop up the
#			menu.
# entry -		Index of a menu entry to center over (x,y).
#			If omitted or specified as {}, then menu's
#			upper-left corner goes at (x,y).

proc ck_popup {menu x y {entry {}}} {
    global ckPriv
    if {($ckPriv(popup) != "") || ($ckPriv(postedMb) != "")} {
	ckMenuUnpost {}
    }
    ckPostOverPoint $menu $x $y $entry
    set ckPriv(focus) [focus]
    set ckPriv(popup) $menu
    focus $menu
}
